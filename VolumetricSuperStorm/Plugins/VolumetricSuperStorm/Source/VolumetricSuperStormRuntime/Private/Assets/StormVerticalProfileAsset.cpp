// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormVerticalProfileAsset.cpp
 * @brief Implements the baked vertical profile asset.
 */

#include "Assets/StormVerticalProfileAsset.h"

#include "Data/StormRenderTargetResolution.h"
#include "Engine/Texture2D.h"
#include "Serialization/CustomVersion.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace StormVerticalProfileAssetVersion
{
const FGuid GUID(0x523776A1, 0x0CB14D35, 0xA071AB52, 0x9D6F2C84);

enum Type
{
	BeforeCustomVersion = 0,
	SequencedProfileKeys = 1,
	LatestVersion = SequencedProfileKeys,
};

FCustomVersionRegistration Registration(
	GUID,
	LatestVersion,
	TEXT("StormVerticalProfileAssetVersion"));
}

namespace
{
const FGuid LegacyPrimaryKeyId(
	0x3484F582,
	0xFA9C4B13,
	0xB8F6A1DE,
	0x5C127903);

bool IsSupportedEasing(EStormProfileTransitionEasing Easing)
{
	return Easing == EStormProfileTransitionEasing::Linear ||
		Easing == EStormProfileTransitionEasing::SmoothStep;
}
}

void UStormVerticalProfileAsset::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(StormVerticalProfileAssetVersion::GUID);
	Super::Serialize(Ar);
}

void UStormVerticalProfileAsset::PostLoad()
{
	Super::PostLoad();

	const int32 Version =
		GetLinkerCustomVersion(StormVerticalProfileAssetVersion::GUID);
	if (Version < StormVerticalProfileAssetVersion::SequencedProfileKeys ||
		Keys.IsEmpty())
	{
		MigrateLegacyProfileToPrimaryKey();
	}
}

void UStormVerticalProfileAsset::MigrateLegacyProfileToPrimaryKey()
{
	if (!Keys.IsEmpty() ||
		(!TopProfile && !BottomProfile && !AnvilProfile))
	{
		return;
	}

	FStormProfileKey& PrimaryKey = Keys.AddDefaulted_GetRef();
	PrimaryKey.KeyId = LegacyPrimaryKeyId;
	PrimaryKey.TimeSeconds = 0.0f;
	PrimaryKey.Params = Params;
	PrimaryKey.BottomProfile = BottomProfile;
	PrimaryKey.TopProfile = TopProfile;
	PrimaryKey.AnvilProfile = AnvilProfile;
}

int32 UStormVerticalProfileAsset::GetKeyCount() const
{
	return Keys.Num();
}

const FStormProfileKey* UStormVerticalProfileAsset::GetKey(int32 Index) const
{
	return Keys.IsValidIndex(Index)
		? &Keys[Index]
		: nullptr;
}

const FStormProfileKey* UStormVerticalProfileAsset::GetPrimaryKey() const
{
	return GetKey(0);
}

float UStormVerticalProfileAsset::GetDurationSeconds() const
{
	if (Keys.IsEmpty() || !FMath::IsFinite(Keys.Last().TimeSeconds))
	{
		return 0.0f;
	}

	return FMath::Max(Keys.Last().TimeSeconds, 0.0f);
}

bool UStormVerticalProfileAsset::FindSegment(
	float TimeSeconds,
	int32& OutFromIndex,
	int32& OutToIndex,
	float& OutAlpha) const
{
	OutFromIndex = INDEX_NONE;
	OutToIndex = INDEX_NONE;
	OutAlpha = 0.0f;

	if (Keys.IsEmpty() || !FMath::IsFinite(TimeSeconds))
	{
		return false;
	}

	for (int32 Index = 0; Index < Keys.Num(); ++Index)
	{
		if (!FMath::IsFinite(Keys[Index].TimeSeconds) ||
			Keys[Index].TimeSeconds < 0.0f ||
			(Index > 0 &&
				Keys[Index].TimeSeconds <= Keys[Index - 1].TimeSeconds))
		{
			return false;
		}
	}

	if (Keys.Num() == 1)
	{
		OutFromIndex = 0;
		OutToIndex = 0;
		return true;
	}

	const float ClampedTime = FMath::Clamp(
		TimeSeconds,
		Keys[0].TimeSeconds,
		Keys.Last().TimeSeconds);
	if (ClampedTime <= Keys[0].TimeSeconds ||
		FMath::IsNearlyEqual(ClampedTime, Keys[0].TimeSeconds))
	{
		OutFromIndex = 0;
		OutToIndex = 0;
		return true;
	}

	for (int32 ToIndex = 1; ToIndex < Keys.Num(); ++ToIndex)
	{
		const float ToTime = Keys[ToIndex].TimeSeconds;
		if (FMath::IsNearlyEqual(ClampedTime, ToTime))
		{
			OutFromIndex = ToIndex;
			OutToIndex = ToIndex;
			return true;
		}

		if (ClampedTime < ToTime)
		{
			const int32 FromIndex = ToIndex - 1;
			const float FromTime = Keys[FromIndex].TimeSeconds;
			const float SegmentDuration = ToTime - FromTime;
			if (SegmentDuration <= SMALL_NUMBER)
			{
				return false;
			}

			OutFromIndex = FromIndex;
			OutToIndex = ToIndex;
			OutAlpha = FMath::Clamp(
				(ClampedTime - FromTime) / SegmentDuration,
				0.0f,
				1.0f);
			return true;
		}
	}

	const int32 LastIndex = Keys.Num() - 1;
	OutFromIndex = LastIndex;
	OutToIndex = LastIndex;
	return true;
}

bool UStormVerticalProfileAsset::IsKeyComplete(int32 Index) const
{
	const FStormProfileKey* Key = GetKey(Index);
	return Key &&
		Key->BottomProfile &&
		Key->TopProfile &&
		Key->AnvilProfile;
}

bool UStormVerticalProfileAsset::IsSequenceComplete() const
{
	if (Keys.IsEmpty() ||
		Keys.Num() > MaxProfileKeys ||
		Keys[0].TimeSeconds != 0.0f)
	{
		return false;
	}

	float PreviousTime = -1.0f;
	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
	{
		const FStormProfileKey& Key = Keys[KeyIndex];
		if (!IsKeyComplete(KeyIndex) ||
			!Key.KeyId.IsValid() ||
			!FMath::IsFinite(Key.TimeSeconds) ||
			Key.TimeSeconds < 0.0f ||
			(KeyIndex > 0 && Key.TimeSeconds <= PreviousTime) ||
			!IsSupportedEasing(Key.OutgoingEasing))
		{
			return false;
		}
		PreviousTime = Key.TimeSeconds;
	}

	return true;
}

#if WITH_EDITOR
#define LOCTEXT_NAMESPACE "StormVerticalProfileAsset"

EDataValidationResult UStormVerticalProfileAsset::IsDataValid(
	FDataValidationContext& Context) const
{
	bool bInvalid =
		Super::IsDataValid(Context) == EDataValidationResult::Invalid;
	auto AddError = [&Context, &bInvalid](const FText& Error)
		{
			Context.AddError(Error);
			bInvalid = true;
		};

	if (Keys.IsEmpty())
	{
		AddError(LOCTEXT(
			"NoProfileKeys",
			"A vertical profile asset must contain at least one profile key."));
		return EDataValidationResult::Invalid;
	}

	if (Keys.Num() > MaxProfileKeys)
	{
		AddError(FText::Format(
			LOCTEXT(
				"TooManyProfileKeys",
				"The asset contains {0} profile keys; the maximum is {1}."),
			FText::AsNumber(Keys.Num()),
			FText::AsNumber(MaxProfileKeys)));
	}

	if (FMath::IsFinite(Keys[0].TimeSeconds) &&
		Keys[0].TimeSeconds != 0.0f)
	{
		AddError(LOCTEXT(
			"FirstKeyMustStartAtZero",
			"The first profile key must occur at 0 seconds."));
	}

	TSet<FGuid> SeenKeyIds;
	TSet<const UTexture2D*> SeenTextures;
	float PreviousTime = -1.0f;
	int32 ExpectedSizeX = INDEX_NONE;
	int32 ExpectedSizeY = INDEX_NONE;
	ETextureSourceFormat ExpectedFormat = TSF_Invalid;

	for (int32 KeyIndex = 0; KeyIndex < Keys.Num(); ++KeyIndex)
	{
		const FStormProfileKey& Key = Keys[KeyIndex];
		const FText KeyNumber = FText::AsNumber(KeyIndex);

		if (!Key.KeyId.IsValid())
		{
			AddError(FText::Format(
				LOCTEXT(
					"InvalidKeyId",
					"Profile key {0} has no valid key ID."),
				KeyNumber));
		}
		else if (SeenKeyIds.Contains(Key.KeyId))
		{
			AddError(FText::Format(
				LOCTEXT(
					"DuplicateKeyId",
					"Profile key {0} reuses another key's ID."),
				KeyNumber));
		}
		else
		{
			SeenKeyIds.Add(Key.KeyId);
		}

		if (!FMath::IsFinite(Key.TimeSeconds) || Key.TimeSeconds < 0.0f)
		{
			AddError(FText::Format(
				LOCTEXT(
					"InvalidKeyTime",
					"Profile key {0} must have a finite, non-negative time."),
				KeyNumber));
		}
		else if (Key.TimeSeconds > MaxKeyTimeSeconds)
		{
			AddError(FText::Format(
				LOCTEXT(
					"KeyTimeAboveMaximum",
					"Profile key {0} occurs at {1}s, beyond the {2}s maximum."),
				KeyNumber,
				FText::AsNumber(Key.TimeSeconds),
				FText::AsNumber(MaxKeyTimeSeconds)));
		}
		else if (KeyIndex > 0 && Key.TimeSeconds <= PreviousTime)
		{
			AddError(FText::Format(
				LOCTEXT(
					"NonIncreasingKeyTime",
					"Profile key {0} must occur after the previous key."),
				KeyNumber));
		}
		PreviousTime = Key.TimeSeconds;

		if (!IsSupportedEasing(Key.OutgoingEasing))
		{
			AddError(FText::Format(
				LOCTEXT(
					"InvalidKeyEasing",
					"Profile key {0} has an unsupported outgoing easing value."),
				KeyNumber));
		}

		struct FNamedTexture
		{
			const TCHAR* Name;
			const UTexture2D* Texture;
		};
		const FNamedTexture ProfileTextures[] =
		{
			{ TEXT("Bottom"), Key.BottomProfile.Get() },
			{ TEXT("Top"), Key.TopProfile.Get() },
			{ TEXT("Anvil"), Key.AnvilProfile.Get() },
		};

		int32 KeySizeX = INDEX_NONE;
		int32 KeySizeY = INDEX_NONE;
		ETextureSourceFormat KeyFormat = TSF_Invalid;
		for (const FNamedTexture& ProfileTexture : ProfileTextures)
		{
			if (!ProfileTexture.Texture)
			{
				AddError(FText::Format(
					LOCTEXT(
						"MissingKeyTexture",
						"Profile key {0} is missing its {1} texture."),
					KeyNumber,
					FText::FromString(FString(ProfileTexture.Name))));
				continue;
			}

			if (SeenTextures.Contains(ProfileTexture.Texture))
			{
				AddError(FText::Format(
					LOCTEXT(
						"SharedKeyTexture",
						"Profile key {0}'s {1} texture is shared by another key; key textures must be independently editable."),
					KeyNumber,
					FText::FromString(FString(ProfileTexture.Name))));
			}
			else
			{
				SeenTextures.Add(ProfileTexture.Texture);
			}

			if (ProfileTexture.Texture->GetOuter() != this)
			{
				AddError(FText::Format(
					LOCTEXT(
						"ExternalKeyTexture",
						"Profile key {0}'s {1} texture must be embedded in this profile asset."),
					KeyNumber,
					FText::FromString(FString(ProfileTexture.Name))));
			}

			const bool bHasSource = ProfileTexture.Texture->Source.IsValid();
			const int32 SizeX = bHasSource
				? static_cast<int32>(ProfileTexture.Texture->Source.GetSizeX())
				: ProfileTexture.Texture->GetSizeX();
			const int32 SizeY = bHasSource
				? static_cast<int32>(ProfileTexture.Texture->Source.GetSizeY())
				: ProfileTexture.Texture->GetSizeY();
			const ETextureSourceFormat Format = bHasSource
				? ProfileTexture.Texture->Source.GetFormat()
				: TSF_Invalid;

			if (bHasSource && Format != TSF_RGBA16F)
			{
				AddError(FText::Format(
					LOCTEXT(
						"UnsupportedKeyTextureFormat",
						"Profile key {0}'s {1} texture must use RGBA16F source data."),
					KeyNumber,
					FText::FromString(FString(ProfileTexture.Name))));
			}

			if (KeySizeX == INDEX_NONE)
			{
				KeySizeX = SizeX;
				KeySizeY = SizeY;
				KeyFormat = Format;
			}
			else if (SizeX != KeySizeX || SizeY != KeySizeY ||
				(Format != TSF_Invalid && KeyFormat != TSF_Invalid &&
					Format != KeyFormat))
			{
				AddError(FText::Format(
					LOCTEXT(
						"InconsistentKeyTexture",
						"Profile key {0}'s Bottom, Top, and Anvil textures must have matching dimensions and source formats."),
					KeyNumber));
			}
		}

		if (KeySizeX != INDEX_NONE)
		{
			if (KeySizeX != VolumetricSuperStorm::RenderTargetResolution::Profile ||
				KeySizeY != VolumetricSuperStorm::RenderTargetResolution::Profile)
			{
				AddError(FText::Format(
					LOCTEXT(
						"KeyResolutionMismatch",
						"Profile key {0}'s texture dimensions do not match the fixed profile resolution."),
					KeyNumber));
			}

			if (ExpectedSizeX == INDEX_NONE)
			{
				ExpectedSizeX = KeySizeX;
				ExpectedSizeY = KeySizeY;
				ExpectedFormat = KeyFormat;
			}
			else if (KeySizeX != ExpectedSizeX || KeySizeY != ExpectedSizeY ||
				(KeyFormat != TSF_Invalid && ExpectedFormat != TSF_Invalid &&
					KeyFormat != ExpectedFormat))
			{
				AddError(FText::Format(
					LOCTEXT(
						"InconsistentSequenceTexture",
						"Profile key {0} does not match the dimensions or source format used by the other keys."),
					KeyNumber));
			}
		}
	}

	return bInvalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}

#undef LOCTEXT_NAMESPACE
#endif
