#include "Components/StormMaterialBinderComponent.h"

#include "Actors/VolumetricSuperStormActor.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Material/StormCloudMaterialParameters.h"
#include "Subsystems/CloudInteractionWorldSubsystem.h"

using namespace SavageSuperStorm;

namespace
{
void SetScalarParameter(UObject* WorldContextObject, UMaterialParameterCollection* Collection, FName ParameterName, float Value)
{
	if (WorldContextObject && Collection && ParameterName != NAME_None)
	{
		UKismetMaterialLibrary::SetScalarParameterValue(WorldContextObject, Collection, ParameterName, Value);
	}
}

void SetVectorParameter(UObject* WorldContextObject, UMaterialParameterCollection* Collection, FName ParameterName, const FLinearColor& Value)
{
	if (WorldContextObject && Collection && ParameterName != NAME_None)
	{
		UKismetMaterialLibrary::SetVectorParameterValue(WorldContextObject, Collection, ParameterName, Value);
	}
}

FLinearColor ToVectorParameter(const FVector& Vector, float W = 0.0f)
{
	return FLinearColor(Vector.X, Vector.Y, Vector.Z, W);
}

FLinearColor MakeCloudLayerParams(const FStormRenderData& RenderData, const UVolumetricCloudComponent* TargetCloud)
{
	// Fallback layer derived from the render bounds when no cloud component is set.
	const float FallbackHeight = FMath::Max(1.0f, FMath::Abs(RenderData.WorldExtent.Z));
	const float FallbackBottom = RenderData.WorldCenter.Z - FallbackHeight * 0.5f;
	return CloudMaterialParams::MakeCloudLayerParams(TargetCloud, FallbackBottom, FallbackHeight);
}
}

UStormMaterialBinderComponent::UStormMaterialBinderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStormMaterialBinderComponent::OnRegister()
{
	Super::OnRegister();

	TryAutoResolveTargetCloud();
	// Seed the binding. Safe + idempotent: a no-op until TargetCloudReference resolves, and
	// it only (re)creates the MID when the target is not already running ours.
	RefreshCloudMaterialBinding(false);
}

void UStormMaterialBinderComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	// The actor was just spawned/dropped (not loaded or PIE-duplicated); adopt the scene's
	// cloud when no target is set yet so it works out of the box.
	TryAutoResolveTargetCloud();
}

void UStormMaterialBinderComponent::BeginPlay()
{
	Super::BeginPlay();

	TryAutoResolveTargetCloud();
	RefreshCloudMaterialBinding(bApplyCloudMaterialOnBeginPlay);
}

void UStormMaterialBinderComponent::SetParameterCollection(UMaterialParameterCollection* InParameterCollection)
{
	ParameterCollection = InParameterCollection;
}

void UStormMaterialBinderComponent::SetParameterNames(const FStormMaterialParameterNames& InParameterNames)
{
	ParameterNames = InParameterNames;
}

void UStormMaterialBinderComponent::UpdateMPCParameters(UObject* WorldContextObject, const FStormRenderData& RenderData) const
{
	if (!HasValidMPCBinding()) return;

	// Does nothing for now
	//SetVectorParameter(WorldContextObject, ParameterCollection, ParameterNames.Center, ToVectorParameter(RenderData.WorldCenter, 1.0f));
	//SetVectorParameter(WorldContextObject, ParameterCollection, ParameterNames.Extent, ToVectorParameter(RenderData.WorldExtent, 0.0f));
	//SetVectorParameter(
	//	WorldContextObject,
	//	ParameterCollection,
	//	ParameterNames.Noise,
	//	FLinearColor(RenderData.NoiseScale, RenderData.NoiseIntensity, RenderData.NoiseSpeed, 0.0f));
	//SetVectorParameter(
	//	WorldContextObject,
	//	ParameterCollection,
	//	ParameterNames.Wind,
	//	ToVectorParameter(RenderData.WindDirection, RenderData.WindSpeed));

	//SetScalarParameter(WorldContextObject, ParameterCollection, ParameterNames.Density, RenderData.Density);
	//SetScalarParameter(WorldContextObject, ParameterCollection, ParameterNames.EdgeFalloff, RenderData.EdgeFalloff);
	//SetScalarParameter(WorldContextObject, ParameterCollection, ParameterNames.LightningIntensity, RenderData.LightningIntensity);
	//SetScalarParameter(WorldContextObject, ParameterCollection, ParameterNames.PrecipitationIntensity, RenderData.PrecipitationIntensity);
	//SetScalarParameter(WorldContextObject, ParameterCollection, ParameterNames.TimeSeconds, RenderData.TimeSeconds);

	//const int32 InteractionSlots = FMath::Min(ParameterNames.InteractionPositionRadius.Num(), ParameterNames.InteractionStrength.Num());
	//const int32 InteractionCount = FMath::Min(RenderData.ActiveInteractions.Num(), InteractionSlots);
	//SetScalarParameter(WorldContextObject, ParameterCollection, ParameterNames.InteractionCount, static_cast<float>(InteractionCount));

	//for (int32 Index = 0; Index < InteractionSlots; ++Index)
	//{
	//	const bool bHasInteraction = Index < InteractionCount;
	//	const FStormInteractionSample* Interaction = bHasInteraction ? &RenderData.ActiveInteractions[Index] : nullptr;

	//	const FVector Position = Interaction ? Interaction->WorldPosition : FVector::ZeroVector;
	//	const float Radius = Interaction ? Interaction->Radius : 0.0f;
	//	const float Strength = Interaction ? Interaction->Strength * Interaction->GetLifeAlpha() : 0.0f;

	//	SetVectorParameter(
	//		WorldContextObject,
	//		ParameterCollection,
	//		ParameterNames.InteractionPositionRadius[Index],
	//		ToVectorParameter(Position, Radius));
	//	SetScalarParameter(
	//		WorldContextObject,
	//		ParameterCollection,
	//		ParameterNames.InteractionStrength[Index],
	//		Strength);
	//}

}

void UStormMaterialBinderComponent::PushStormRenderData(const FStormRenderData& RenderData)
{
	CaptureScaleReferenceIfNeeded(RenderData);
	UpdateMPCParameters(GetWorld(), RenderData);

	// Re-assert the binding invariant before uploading. Cheap once bound, so this is the
	// single path that keeps the MID live in both the editor and PIE without a tick sweep.
	if (!EnsureDynamicCloudMaterial())
	{
		return;
	}

	UploadAllParamsToMID(RenderData);
}

UMaterialInstanceDynamic* UStormMaterialBinderComponent::ApplyVolumetricCloudMaterial()
{
	// CallInEditor entry point: force a fresh MID from the desired parent, then push.
	UMaterialInstanceDynamic* Material = EnsureDynamicCloudMaterial(/*bForceRecreate=*/true);
	UpdateVolumetricCloudMaterial();
	return Material;
}

void UStormMaterialBinderComponent::UpdateVolumetricCloudMaterial()
{
	if (!EnsureDynamicCloudMaterial())
	{
		return;
	}

	const AVolumetricSuperStormActor* StormActor = Cast<AVolumetricSuperStormActor>(GetOwner());
	if (!StormActor)
	{
		return;
	}

	const FStormRenderData& RenderData = StormActor->GetStormRenderData();
	CaptureScaleReferenceIfNeeded(RenderData);
	UploadAllParamsToMID(RenderData);
}

void UStormMaterialBinderComponent::RefreshCloudMaterialBinding(bool bReapplyCloudMaterial)
{
	if (bReapplyCloudMaterial)
	{
		EnsureDynamicCloudMaterial(/*bForceRecreate=*/true);
	}

	if (const AVolumetricSuperStormActor* StormActor = Cast<AVolumetricSuperStormActor>(GetOwner()))
	{
		PushStormRenderData(StormActor->GetStormRenderData());
	}
	else
	{
		UpdateVolumetricCloudMaterial();
	}
}

#if WITH_EDITOR
void UStormMaterialBinderComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	// Editing a sub-field of the component reference (e.g. the picked actor/component) reports
	// the leaf name, so match on the member name to catch any retarget.
	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	const bool bTargetChanged = MemberName == GET_MEMBER_NAME_CHECKED(UStormMaterialBinderComponent, TargetCloudReference);
	const bool bMaterialChanged = bTargetChanged ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UStormMaterialBinderComponent, BaseCloudMaterial);

	if (bTargetChanged)
	{
		// Drop the cache so the new reference is resolved instead of the stale cloud.
		ResolvedTargetCloud.Reset();
	}

	RefreshCloudMaterialBinding(bMaterialChanged);
}
#endif

UMaterialInstanceDynamic* UStormMaterialBinderComponent::GetDynamicCloudMaterial() const
{
	return DynamicCloudMaterial;
}

bool UStormMaterialBinderComponent::HasValidMPCBinding() const
{
	return ParameterCollection != nullptr && GetWorld() != nullptr;
}

bool UStormMaterialBinderComponent::IsTargetCloudReferenceSet() const
{
	return TargetCloudReference.OtherActor.IsValid()
		|| TargetCloudReference.OverrideComponent.IsValid()
		|| TargetCloudReference.ComponentProperty != NAME_None
		|| !TargetCloudReference.PathToComponent.IsEmpty();
}

void UStormMaterialBinderComponent::TryAutoResolveTargetCloud()
{
	if (IsTargetCloudReferenceSet())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UVolumetricCloudComponent* Cloud = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UVolumetricCloudComponent* Found = It->FindComponentByClass<UVolumetricCloudComponent>())
		{
			Cloud = Found;
			break;
		}
	}

	if (!Cloud)
	{
		return;
	}

	// Store by (owning actor + component name) so the pick is visible/editable in the details
	// panel and remaps into the PIE world just like an artist-authored reference. PathToComponent
	// finds the component by name under its owner (works for default subobjects and instances).
	TargetCloudReference = FComponentReference();
	TargetCloudReference.OtherActor = Cloud->GetOwner();
	TargetCloudReference.PathToComponent = Cloud->GetName();

	ResolvedTargetCloud = Cloud;
}

UVolumetricCloudComponent* UStormMaterialBinderComponent::ResolveTargetCloud() const
{
	// A cached resolution is valid only while it belongs to our current world; the editor
	// component and its PIE duplicate are different objects.
	if (ResolvedTargetCloud.IsValid() && ResolvedTargetCloud->GetWorld() == GetWorld())
	{
		return ResolvedTargetCloud.Get();
	}

	// FComponentReference resolves by (referenced actor + component name/path). The weak
	// actor pointer is remapped into the PIE world on duplication, so we bind the duplicated
	// cloud instead of copying a component into this actor as a hard pointer would.
	UActorComponent* Component = TargetCloudReference.GetComponent(GetOwner());
	UVolumetricCloudComponent* Cloud = Cast<UVolumetricCloudComponent>(Component);

	if (!IsValid(Cloud) || Cloud->GetWorld() != GetWorld())
	{
		ResolvedTargetCloud.Reset();
		return nullptr;
	}

	ResolvedTargetCloud = Cloud;
	return Cloud;
}

UMaterialInstanceDynamic* UStormMaterialBinderComponent::EnsureDynamicCloudMaterial(bool bForceRecreate)
{
	UVolumetricCloudComponent* Cloud = ResolveTargetCloud();
	if (!Cloud)
	{
		return nullptr;
	}

	UMaterialInterface* CurrentMaterial = Cloud->GetMaterial();
	UMaterialInstanceDynamic* CurrentMID = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
	const bool bOwnedByThis = CurrentMID && CurrentMID->GetOuter() == this;

	// Steady state: the target already runs our MID. Reuse it unless an explicit base
	// material was set and no longer matches, or a rebuild was explicitly requested.
	if (!bForceRecreate && bOwnedByThis)
	{
		const bool bReparentNeeded = BaseCloudMaterial && CurrentMID->Parent != BaseCloudMaterial;
		if (!bReparentNeeded)
		{
			DynamicCloudMaterial = CurrentMID;
			return CurrentMID;
		}
	}

	// Choose the parent explicitly. When we already own the current material, parent the
	// new MID to that MID's parent rather than to itself. Never Create() a null parent.
	UMaterialInterface* Parent = BaseCloudMaterial
		? BaseCloudMaterial.Get()
		: (bOwnedByThis ? CurrentMID->Parent.Get() : CurrentMaterial);
	if (!Parent)
	{
		return nullptr;
	}

	DynamicCloudMaterial = UMaterialInstanceDynamic::Create(Parent, this);
	if (DynamicCloudMaterial)
	{
		Cloud->SetMaterial(DynamicCloudMaterial);
	}

	return DynamicCloudMaterial;
}

void UStormMaterialBinderComponent::CaptureScaleReferenceIfNeeded(const FStormRenderData& RenderData)
{
	if (ScaleReferenceRadius > 0.0f)
	{
		return;
	}

	// OnRegister can run before the actor's first RebuildRenderData. Wait until the
	// cached center represents the owner so a placed actor does not capture (0,0,0).
	if (const AActor* OwnerActor = GetOwner())
	{
		if (!RenderData.WorldCenter.Equals(OwnerActor->GetActorLocation(), 1.0))
		{
			return;
		}
	}

	ScaleReferenceRadius = FMath::Max(1.0f, RenderData.Shape.Radius);
}

void UStormMaterialBinderComponent::UploadAllParamsToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial) return;
	UploadTexturesToMID(RenderData);
	UploadLayerParametersToMID(RenderData);
	UploadShapeSettingsToMID(RenderData);
	UploadMotionSettingsToMID(RenderData);
	UploadPerFrameParamsToMID(RenderData);
}

void UStormMaterialBinderComponent::UploadTexturesToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial) return;

	auto Upload = [](UMaterialInstanceDynamic* MID, FName TextureName, UTexture* Texture)
		{
			// Implementation Decision: Null Textures => Do Nothing
			if (!MID || !Texture) return;
			MID->SetTextureParameterValue(TextureName, Texture);
		};

	UTextureRenderTarget2D* ShapeRenderTarget = nullptr;
	UTextureRenderTarget2D* ShapeRenderTarget2 = nullptr;
	UTextureRenderTarget2D* MotionRenderTarget = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (UCloudInteractionWorldSubsystem* CloudSubsystem = World->GetSubsystem<UCloudInteractionWorldSubsystem>())
		{
			// MotionRenderTarget = CloudSubsystem->GetMotionRenderTarget(); // For later use
			ShapeRenderTarget = CloudSubsystem->GetShapeRenderTarget();
			ShapeRenderTarget2 = CloudSubsystem->GetShapeRenderTarget2();
		}
	}

	Upload(DynamicCloudMaterial, CloudMaterialParams::MotionTexture,	MotionRenderTarget);
	Upload(DynamicCloudMaterial, CloudMaterialParams::ShapeTexture,		ShapeRenderTarget);
	Upload(DynamicCloudMaterial, CloudMaterialParams::ShapeTexture2,	ShapeRenderTarget2);
	Upload(DynamicCloudMaterial, CloudMaterialParams::BottomProfileRT,	RenderData.BottomProfileTexture);
	Upload(DynamicCloudMaterial, CloudMaterialParams::TopProfileRT,		RenderData.TopProfileTexture);
	Upload(DynamicCloudMaterial, CloudMaterialParams::FlowMapLower,		RenderData.FlowMap.LowerTexture);
	Upload(DynamicCloudMaterial, CloudMaterialParams::FlowMapMiddle,	RenderData.FlowMap.MiddleTexture);
	Upload(DynamicCloudMaterial, CloudMaterialParams::FlowMapUpper,		RenderData.FlowMap.UpperTexture);
}


void UStormMaterialBinderComponent::UploadLayerParametersToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial) return;
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::CloudLayerParams,
		MakeCloudLayerParams(RenderData, ResolveTargetCloud()));
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::FlowMapLayerHeights,
		FLinearColor(
			RenderData.FlowMap.LayerHeights.X,
			RenderData.FlowMap.LayerHeights.Y,
			RenderData.FlowMap.LayerHeights.Z,
			0.0f));
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::FlowMapUVWStrength,
		FLinearColor(
			RenderData.FlowMap.UVWStrength.X,
			RenderData.FlowMap.UVWStrength.Y,
			RenderData.FlowMap.UVWStrength.Z,
			FMath::Max(RenderData.FlowMap.CycleDurationSeconds, 0.1f)));
	DynamicCloudMaterial->SetScalarParameterValue(
		CloudMaterialParams::FlowMapEnabled,
		RenderData.FlowMap.bEnabled ? 1.0f : 0.0f);
}

void UStormMaterialBinderComponent::UploadShapeSettingsToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial) return;
	const FStormShapeSettings& Shape = RenderData.Shape;
	const float CurrentRadius = FMath::Max(1.0f, Shape.Radius);
	const float ReferenceRadius = ScaleReferenceRadius > 0.0f ? ScaleReferenceRadius : CurrentRadius;
	const float FieldExtentX = FMath::Abs(static_cast<float>(RenderData.WorldExtent.X));
	const float FieldExtentY = FMath::Abs(static_cast<float>(RenderData.WorldExtent.Y));
	const float PaddedFieldRadius = FMath::Max(
		CurrentRadius,
		1.08f * FMath::Sqrt(
			FieldExtentX * FieldExtentX
			+ FieldExtentY * FieldExtentY));

	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormCenterRadius,		FLinearColor(RenderData.WorldCenter.X, RenderData.WorldCenter.Y, PaddedFieldRadius, 0.0f));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::StormRadius,				CurrentRadius);
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::FieldResolution,			Shape.ShapeRenderTargetResolution);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormCenter,				FLinearColor(RenderData.WorldCenter.X, RenderData.WorldCenter.Y, 0.0f, 0.0f));
	// The density transport needs a true storm-centred direction field. Keeping
	// this reference at zero makes LF/HF XY coordinates storm local; the noise
	// remains continuous because both scales share the same coordinate carrier.
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormReferenceCenter,	FLinearColor::Black);
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::StormCoordinateScale,	ReferenceRadius / CurrentRadius);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormExtent,				FLinearColor(
		FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.X))),
		FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.Y))),
		FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.Z))),
		0.0f));

	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::DensityMultiplier,		FMath::Max(0.0f, Shape.Density));
	DynamicCloudMaterial->SetScalarParameterValue(
		CloudMaterialParams::UndersideVisibility,
		FMath::Clamp(Shape.UndersideVisibility, 0.0f, 1.0f));
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::BrimEmissiveColor,
		Shape.BrimEmissiveColor);
}

void UStormMaterialBinderComponent::UploadPerFrameParamsToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial) return;
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::StormTimeSeconds, RenderData.TimeSeconds);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::LifecycleState, FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
	FVector2D InitialFlowDirection(RenderData.WindDirection.X, RenderData.WindDirection.Y);
	if (!InitialFlowDirection.Normalize())
	{
		InitialFlowDirection = FVector2D(1.0, 0.0);
	}

	// One internal scalar drives the 3D material-coordinate transport. It reuses
	// the existing serialized LFMacroMax pin, so no new material or actor
	// parameters are exposed. Default 1.0 remains exact mature identity.
	float EncodedLifecycle = 1.0f;
	switch (RenderData.FormationState)
	{
	case EStormFormationState::Hidden:
		EncodedLifecycle = 3.0f;
		break;
	case EStormFormationState::Forming:
		EncodedLifecycle = FMath::Clamp(RenderData.FormationProgress, 0.0f, 0.998f);
		break;
	case EStormFormationState::Dissolving:
		EncodedLifecycle = 2.0f + FMath::Clamp(RenderData.FormationProgress, 0.0f, 0.998f);
		break;
	case EStormFormationState::Mature:
	default:
		EncodedLifecycle = 1.0f;
		break;
	}

	DynamicCloudMaterial->SetScalarParameterValue(
		CloudMaterialParams::LifecycleControl,
		EncodedLifecycle);
}

void UStormMaterialBinderComponent::UploadMotionSettingsToMID(
	const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial)
	{
		return;
	}

	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::MDRBoundaries0,
		RenderData.MDRBoundaries0);
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::MDRBoundaries1,
		RenderData.MDRBoundaries1);
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::MDRSpeeds0,
		RenderData.MDRSpeeds0);
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::MDRSpeeds1,
		RenderData.MDRSpeeds1);
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::MDRPhases0,
		RenderData.MDRPhases0);
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::MDRPhases1,
		RenderData.MDRPhases1);
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::MDRSkews0,
		RenderData.MDRSkews0);
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::MDRSkews1,
		RenderData.MDRSkews1);
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::MDRControl0,
		RenderData.MDRControl0);
	DynamicCloudMaterial->SetVectorParameterValue(
		CloudMaterialParams::MDRControl1,
		RenderData.MDRControl1);
}
