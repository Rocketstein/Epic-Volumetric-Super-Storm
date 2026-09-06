#include "Widgets/Profile/StormProfileAssetEditorUtils.h"

#include "Assets/StormVerticalProfileAsset.h"
#include "Components/StormVerticalProfileToolComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "ScopedTransaction.h"
#include "TextureResource.h"

namespace
{
FName MakeKeyTextureName(
	const FGuid& KeyId,
	const TCHAR* Channel)
{
	return FName(*FString::Printf(
		TEXT("ProfileKey_%s_%s"),
		*KeyId.ToString(EGuidFormats::Digits),
		Channel));
}

UTexture2D* CreateOrUpdateEmbeddedProfileTexture(
	UTextureRenderTarget2D* SourceRT,
	UStormVerticalProfileAsset* ProfileAsset,
	const FName TextureName,
	UTexture2D* ExistingTexture)
{
	if (!SourceRT || !ProfileAsset)
	{
		return nullptr;
	}

	FTextureRenderTargetResource* RTResource =
		SourceRT->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		return nullptr;
	}

	TArray<FFloat16Color> Pixels;
	if (!RTResource->ReadFloat16Pixels(Pixels))
	{
		return nullptr;
	}

	const int32 SizeX = SourceRT->SizeX;
	const int32 SizeY = SourceRT->SizeY;
	if (Pixels.Num() != SizeX * SizeY)
	{
		return nullptr;
	}

	UTexture2D* Texture =
		ExistingTexture && ExistingTexture->GetOuter() == ProfileAsset
			? ExistingTexture
			: FindObject<UTexture2D>(ProfileAsset, *TextureName.ToString());
	if (!Texture)
	{
		Texture = NewObject<UTexture2D>(
			ProfileAsset,
			TextureName,
			RF_Transactional);
	}
	if (!Texture)
	{
		return nullptr;
	}

	Texture->Modify();
	Texture->SetFlags(RF_Transactional);
	Texture->ClearFlags(RF_Public | RF_Standalone);
	Texture->PreEditChange(nullptr);
	Texture->Source.Init(
		SizeX,
		SizeY,
		/*NumSlices*/ 1,
		/*NumMips*/ 1,
		TSF_RGBA16F,
		reinterpret_cast<const uint8*>(Pixels.GetData()));
	Texture->SRGB = false;
	Texture->CompressionSettings = TC_HDR;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
	Texture->PostEditChange();
	Texture->UpdateResource();
	return Texture;
}

UTexture2D* DuplicateEmbeddedProfileTexture(
	UTexture2D* SourceTexture,
	UStormVerticalProfileAsset* ProfileAsset,
	const FGuid& NewKeyId,
	const TCHAR* Channel)
{
	if (!SourceTexture || !ProfileAsset)
	{
		return nullptr;
	}

	const FName DesiredName = MakeKeyTextureName(NewKeyId, Channel);
	const FName UniqueName = MakeUniqueObjectName(
		ProfileAsset,
		UTexture2D::StaticClass(),
		DesiredName);
	UTexture2D* Duplicate = DuplicateObject<UTexture2D>(
		SourceTexture,
		ProfileAsset,
		UniqueName);
	if (Duplicate)
	{
		Duplicate->SetFlags(RF_Transactional);
		Duplicate->ClearFlags(RF_Public | RF_Standalone);
		Duplicate->MarkPackageDirty();
	}
	return Duplicate;
}
}

namespace SavageSuperStorm::ProfileEditor
{
int32 FindKeyIndexById(
	const UStormVerticalProfileAsset* ProfileAsset,
	const FGuid& KeyId)
{
	if (!ProfileAsset || !KeyId.IsValid())
	{
		return INDEX_NONE;
	}

	return ProfileAsset->Keys.IndexOfByPredicate(
		[&KeyId](const FStormProfileKey& Key)
		{
			return Key.KeyId == KeyId;
		});
}

float SuggestTimeAfter(
	const UStormVerticalProfileAsset* ProfileAsset,
	int32 SourceIndex)
{
	if (!ProfileAsset || !ProfileAsset->Keys.IsValidIndex(SourceIndex))
	{
		return 0.0f;
	}

	const float SourceTime =
		ProfileAsset->Keys[SourceIndex].TimeSeconds;
	if (!FMath::IsFinite(SourceTime))
	{
		return 0.0f;
	}

	const int32 NextIndex = SourceIndex + 1;
	if (ProfileAsset->Keys.IsValidIndex(NextIndex))
	{
		const float NextTime =
			ProfileAsset->Keys[NextIndex].TimeSeconds;
		if (FMath::IsFinite(NextTime) &&
			NextTime > SourceTime)
		{
			return SourceTime +
				(NextTime - SourceTime) * 0.5f;
		}
	}

	return SourceTime + 1.0f;
}

bool CommitToolToKey(
	UStormVerticalProfileToolComponent* Tool,
	UStormVerticalProfileAsset* ProfileAsset,
	int32 KeyIndex)
{
	if (!Tool || !ProfileAsset ||
		!ProfileAsset->Keys.IsValidIndex(KeyIndex))
	{
		return false;
	}

	FStormProfileKey& Key = ProfileAsset->Keys[KeyIndex];

	ProfileAsset->Modify();

	// A key being baked for the first time adopts the tool's active working-set id
	// instead of minting a fresh one. The tool always has a working set now, even
	// before any asset exists, and it carries that surface's edit history -- taking
	// its id here is what keeps the history attached to the key it becomes, rather
	// than stranding it under an id nothing references.
	//
	// This must happen before GetKeyRenderTargetState: the lookup is by key id, and
	// an unadopted key would miss the active set entirely.
	if (!Key.KeyId.IsValid())
	{
		const FGuid ActiveKeyId = Tool->GetActiveKeyId();
		Key.KeyId = ActiveKeyId.IsValid() ? ActiveKeyId : FGuid::NewGuid();
	}

	UTextureRenderTarget2D* BottomRT = nullptr;
	UTextureRenderTarget2D* TopRT = nullptr;
	UTextureRenderTarget2D* AnvilRT = nullptr;
	FStormProfileParams Params;
	if (!Tool->GetKeyRenderTargetState(
			Key.KeyId,
			BottomRT,
			TopRT,
			AnvilRT,
			Params))
	{
		// The key exists but no working set answers to its id, so there are no
		// pixels to bake. Silently returning false here produces a valid-looking
		// asset with empty textures, so say why.
		UE_LOG(LogTemp, Error,
			TEXT("Profile key %s has no working set on the tool; nothing was baked. "
				 "This usually means the key was created with a fresh id instead of "
				 "adopting the tool's active working-set id."),
			*Key.KeyId.ToString(EGuidFormats::Digits));
		return false;
	}
	if (!BottomRT || !TopRT || !AnvilRT)
	{
		return false;
	}

	FlushRenderingCommands();

	UTexture2D* TopTexture =
		CreateOrUpdateEmbeddedProfileTexture(
			TopRT,
			ProfileAsset,
			MakeKeyTextureName(Key.KeyId, TEXT("Top")),
			Key.TopProfile);
	UTexture2D* BottomTexture =
		CreateOrUpdateEmbeddedProfileTexture(
			BottomRT,
			ProfileAsset,
			MakeKeyTextureName(Key.KeyId, TEXT("Bottom")),
			Key.BottomProfile);
	UTexture2D* AnvilTexture =
		CreateOrUpdateEmbeddedProfileTexture(
			AnvilRT,
			ProfileAsset,
			MakeKeyTextureName(Key.KeyId, TEXT("Anvil")),
			Key.AnvilProfile);
	if (!TopTexture || !BottomTexture || !AnvilTexture)
	{
		return false;
	}

	Key.Params = Params;
	Key.TopProfile = TopTexture;
	Key.BottomProfile = BottomTexture;
	Key.AnvilProfile = AnvilTexture;
	ProfileAsset->PostEditChange();
	ProfileAsset->MarkPackageDirty();

	Tool->Modify();
	Tool->PersistentProfileAsset = ProfileAsset;
	Tool->MarkPackageDirty();
	Tool->MarkKeyRenderTargetsSaved(Key.KeyId);
	return true;
}

bool CommitToolToAllKeys(
	UStormVerticalProfileToolComponent* Tool,
	UStormVerticalProfileAsset* ProfileAsset,
	const FGuid& ActiveKeyId)
{
	if (!Tool || !ProfileAsset || ProfileAsset->Keys.IsEmpty())
	{
		return false;
	}

	if (!Tool->HasAnyKeyRenderTargets())
	{
		int32 KeyIndex = FindKeyIndexById(
			ProfileAsset,
			ActiveKeyId);
		if (!ProfileAsset->Keys.IsValidIndex(KeyIndex))
		{
			KeyIndex = 0;
		}
		return CommitToolToKey(Tool, ProfileAsset, KeyIndex);
	}

	bool bCommittedAny = false;
	for (int32 KeyIndex = 0;
		KeyIndex < ProfileAsset->Keys.Num();
		++KeyIndex)
	{
		if (!Tool->HasKeyRenderTargets(
				ProfileAsset->Keys[KeyIndex].KeyId))
		{
			continue;
		}
		if (!CommitToolToKey(Tool, ProfileAsset, KeyIndex))
		{
			return false;
		}
		bCommittedAny = true;
	}

	if (!bCommittedAny)
	{
		// Resident working sets exist but none belong to this asset's keys, so
		// every key already matches its embedded textures. CommitToolToKey would
		// have bound the tool to ProfileAsset as a side effect, and Save As
		// depends on that retarget, so do it explicitly here.
		Tool->Modify();
		Tool->PersistentProfileAsset = ProfileAsset;
		Tool->MarkPackageDirty();
	}

	// Nothing left unbaked, so this succeeded.
	return true;
}

bool DuplicateKeyAfter(
	UStormVerticalProfileAsset* ProfileAsset,
	int32 SourceIndex,
	FGuid& OutNewKeyId)
{
	return DuplicateKeyAtTime(
		ProfileAsset,
		SourceIndex,
		SuggestTimeAfter(ProfileAsset, SourceIndex),
		OutNewKeyId);
}

bool DuplicateKeyAtTime(
	UStormVerticalProfileAsset* ProfileAsset,
	int32 SourceIndex,
	float TimeSeconds,
	FGuid& OutNewKeyId)
{
	OutNewKeyId.Invalidate();
	if (!ProfileAsset ||
		!ProfileAsset->Keys.IsValidIndex(SourceIndex) ||
		ProfileAsset->Keys.Num() >=
			UStormVerticalProfileAsset::MaxProfileKeys ||
		!ProfileAsset->IsKeyComplete(SourceIndex) ||
		!FMath::IsFinite(TimeSeconds) ||
		TimeSeconds < 0.0f ||
		TimeSeconds >
			UStormVerticalProfileAsset::MaxKeyTimeSeconds)
	{
		return false;
	}
	for (const FStormProfileKey& ExistingKey :
		ProfileAsset->Keys)
	{
		if (FMath::IsNearlyEqual(
				ExistingKey.TimeSeconds,
				TimeSeconds))
		{
			return false;
		}
	}

	const FStormProfileKey& SourceKey =
		ProfileAsset->Keys[SourceIndex];
	FStormProfileKey NewKey;
	NewKey.KeyId = FGuid::NewGuid();
	NewKey.TimeSeconds = TimeSeconds;
	NewKey.Params = SourceKey.Params;
	NewKey.OutgoingEasing = SourceKey.OutgoingEasing;

	// Not const: a failed duplication below has to cancel this.
	FScopedTransaction Transaction(
		INVTEXT("Add Storm Profile Key"));
	ProfileAsset->Modify();

	NewKey.BottomProfile =
		DuplicateEmbeddedProfileTexture(
			SourceKey.BottomProfile.Get(),
			ProfileAsset,
			NewKey.KeyId,
			TEXT("Bottom"));
	NewKey.TopProfile =
		DuplicateEmbeddedProfileTexture(
			SourceKey.TopProfile.Get(),
			ProfileAsset,
			NewKey.KeyId,
			TEXT("Top"));
	NewKey.AnvilProfile =
		DuplicateEmbeddedProfileTexture(
			SourceKey.AnvilProfile.Get(),
			ProfileAsset,
			NewKey.KeyId,
			TEXT("Anvil"));
	if (!NewKey.BottomProfile ||
		!NewKey.TopProfile ||
		!NewKey.AnvilProfile)
	{
		// Roll back rather than committing a no-op entry
		Transaction.Cancel();
		return false;
	}

	const FGuid NewKeyId = NewKey.KeyId;
	int32 InsertionIndex =
		ProfileAsset->Keys.IndexOfByPredicate(
			[TimeSeconds](const FStormProfileKey& Key)
			{
				return Key.TimeSeconds > TimeSeconds;
			});
	if (InsertionIndex == INDEX_NONE)
	{
		InsertionIndex = ProfileAsset->Keys.Num();
	}
	ProfileAsset->Keys.Insert(
		MoveTemp(NewKey),
		InsertionIndex);
	ProfileAsset->PostEditChange();
	ProfileAsset->MarkPackageDirty();
	OutNewKeyId = NewKeyId;
	return true;
}

bool RemoveKey(
	UStormVerticalProfileAsset* ProfileAsset,
	int32 KeyIndex)
{
	if (!ProfileAsset ||
		ProfileAsset->Keys.Num() <= 1 ||
		!ProfileAsset->Keys.IsValidIndex(KeyIndex))
	{
		return false;
	}

	const FScopedTransaction Transaction(
		INVTEXT("Delete Storm Profile Key"));
	ProfileAsset->Modify();
	const FStormProfileKey& RemovedKey =
		ProfileAsset->Keys[KeyIndex];
	UTexture2D* RemovedTextures[] =
	{
		RemovedKey.BottomProfile.Get(),
		RemovedKey.TopProfile.Get(),
		RemovedKey.AnvilProfile.Get(),
	};
	for (UTexture2D* Texture : RemovedTextures)
	{
		if (Texture && Texture->GetOuter() == ProfileAsset)
		{
			// A referenced non-public subobject is still serialized. Once the
			// key is removed, clearing top-level flags lets SavePackage omit it;
			// keeping the UObject alive also makes undo safe.
			Texture->Modify();
			Texture->ClearFlags(
				RF_Public | RF_Standalone);
		}
	}
	const bool bRemovedFirstKey = KeyIndex == 0;
	ProfileAsset->Keys.RemoveAt(KeyIndex);
	if (bRemovedFirstKey && !ProfileAsset->Keys.IsEmpty())
	{
		// Preserve every remaining segment duration while moving the new
		// sequence origin to zero.
		const float NewOrigin =
			ProfileAsset->Keys[0].TimeSeconds;
		if (FMath::IsFinite(NewOrigin))
		{
			for (FStormProfileKey& Key : ProfileAsset->Keys)
			{
				Key.TimeSeconds =
					FMath::Max(Key.TimeSeconds - NewOrigin, 0.0f);
			}
		}
		// Assign exactly zero so authored data satisfies the invariant without
		// accumulating floating-point subtraction residue.
		ProfileAsset->Keys[0].TimeSeconds = 0.0f;
	}
	ProfileAsset->PostEditChange();
	ProfileAsset->MarkPackageDirty();
	return true;
}

bool CopyKeysToAsset(
	const UStormVerticalProfileAsset* SourceAsset,
	UStormVerticalProfileAsset* DestinationAsset)
{
	if (!SourceAsset || !DestinationAsset ||
		SourceAsset == DestinationAsset ||
		SourceAsset->Keys.IsEmpty() ||
		SourceAsset->Keys.Num() >
			UStormVerticalProfileAsset::MaxProfileKeys)
	{
		return false;
	}

	for (int32 KeyIndex = 0;
		KeyIndex < SourceAsset->Keys.Num();
		++KeyIndex)
	{
		if (!SourceAsset->IsKeyComplete(KeyIndex))
		{
			return false;
		}
	}

	DestinationAsset->Modify();
	DestinationAsset->Keys.Reset(
		SourceAsset->Keys.Num());
	DestinationAsset->bLoopByDefault =
		SourceAsset->bLoopByDefault;

	for (const FStormProfileKey& SourceKey :
		SourceAsset->Keys)
	{
		FStormProfileKey NewKey;
		NewKey.KeyId = SourceKey.KeyId;
		NewKey.TimeSeconds = SourceKey.TimeSeconds;
		NewKey.Params = SourceKey.Params;
		NewKey.OutgoingEasing =
			SourceKey.OutgoingEasing;
		NewKey.BottomProfile =
			DuplicateEmbeddedProfileTexture(
				SourceKey.BottomProfile.Get(),
				DestinationAsset,
				NewKey.KeyId,
				TEXT("Bottom"));
		NewKey.TopProfile =
			DuplicateEmbeddedProfileTexture(
				SourceKey.TopProfile.Get(),
				DestinationAsset,
				NewKey.KeyId,
				TEXT("Top"));
		NewKey.AnvilProfile =
			DuplicateEmbeddedProfileTexture(
				SourceKey.AnvilProfile.Get(),
				DestinationAsset,
				NewKey.KeyId,
				TEXT("Anvil"));
		if (!NewKey.BottomProfile ||
			!NewKey.TopProfile ||
			!NewKey.AnvilProfile)
		{
			DestinationAsset->Keys.Reset();
			return false;
		}

		DestinationAsset->Keys.Add(MoveTemp(NewKey));
	}

	DestinationAsset->PostEditChange();
	DestinationAsset->MarkPackageDirty();
	return true;
}
}
