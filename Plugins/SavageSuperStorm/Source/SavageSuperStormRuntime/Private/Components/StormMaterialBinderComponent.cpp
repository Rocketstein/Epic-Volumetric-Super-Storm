/**
 * @file StormMaterialBinderComponent.cpp
 * @brief Binds storm render data to one volumetric cloud material instance.
 */

#include "Components/StormMaterialBinderComponent.h"

#include "EngineUtils.h"
#include "SavageSuperStormRuntime.h"
#include "Actors/VolumetricSuperStormActor.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Material/StormCloudMaterialParameters.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Subsystems/StormRenderWorldSubsystem.h"
#include "UObject/UnrealType.h"

using namespace SavageSuperStorm;

UStormMaterialBinderComponent::UStormMaterialBinderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UStormMaterialBinderComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	if (!ObjectPropertyChangedHandle.IsValid())
	{
		ObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UStormMaterialBinderComponent::HandleObjectPropertyChanged);
	}
#endif
	TryAutoResolveTargetCloud();
	RefreshCloudMaterialBinding(false);
}

void UStormMaterialBinderComponent::OnUnregister()
{
	ReleaseVolumetricCloudMaterial();
#if WITH_EDITOR
	if (ObjectPropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectPropertyChangedHandle);
		ObjectPropertyChangedHandle.Reset();
	}
#endif
	Super::OnUnregister();
}

#if WITH_EDITOR
void UStormMaterialBinderComponent::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (!bTrackTargetCloudMaterialChange || !Object)
	{
		return;
	}

	if (Object != ResolvedTargetCloud.Get())
	{
		return;
	}

	const FName PropertyName               = PropertyChangedEvent.GetPropertyName();
	const bool  bMaterialChanged           = PropertyName == GET_MEMBER_NAME_CHECKED(UVolumetricCloudComponent, Material);
	const bool  bTransactionRestoredObject = PropertyName == NAME_None;
	if (!bMaterialChanged && !bTransactionRestoredObject)
	{
		return;
	}

	if (bMaterialChanged)
	{
		UVolumetricCloudComponent* Cloud = CastChecked<UVolumetricCloudComponent>(Object);

		UMaterialInterface* NewBase = Cloud->Material.LoadSynchronous();
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(NewBase))
		{
			NewBase = MID->Parent.Get();
		}

		if (NewBase && NewBase != BaseCloudMaterial)
		{
			Modify();
			BaseCloudMaterial = NewBase;
		}
	}

	QueueTargetCloudMaterialRebind();
}

void UStormMaterialBinderComponent::QueueTargetCloudMaterialRebind()
{
	if (bCloudMaterialRebindPending)
	{
		return;
	}

	bCloudMaterialRebindPending = true;
	TWeakObjectPtr<UStormMaterialBinderComponent> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis](float)
			{
				if (UStormMaterialBinderComponent* Binder = WeakThis.Get())
				{
					Binder->bCloudMaterialRebindPending = false;
					if (Binder->bTrackTargetCloudMaterialChange)
					{
						Binder->RefreshCloudMaterialBinding(false);
					}
				}
				return false;
			}
		),
		0.0f
	);
}
#endif

void UStormMaterialBinderComponent::PushStormRenderData(const FStormRenderData& RenderData)
{
	CaptureScaleReferenceIfNeeded(RenderData);

	if (!EnsureDynamicCloudMaterial())
	{
		return;
	}

	UploadAllParamsToMID(RenderData);
	LastFullyUploadedMaterial = DynamicCloudMaterial.Get();
}

void UStormMaterialBinderComponent::PushStormFrameData(const FStormRenderData& RenderData)
{
	CaptureScaleReferenceIfNeeded(RenderData);

	if (!DynamicCloudMaterial || LastFullyUploadedMaterial.Get() != DynamicCloudMaterial.Get())
	{
		return;
	}

	UploadFrameSpatialParamsToMID(RenderData);
	UploadMotionSettingsToMID(RenderData);
	UploadPerFrameParamsToMID(RenderData);
}

void UStormMaterialBinderComponent::PushStormProfileData(const FStormRenderData& RenderData)
{
	if (!EnsureDynamicCloudMaterial())
	{
		return;
	}

	auto Upload = [this](FName ParameterName, UTexture* Texture)
	{
		DynamicCloudMaterial->SetTextureParameterValue(ParameterName, Texture);
	};

	Upload(CloudMaterialParams::BottomProfileRT, RenderData.BottomProfileTexture);
	Upload(CloudMaterialParams::TopProfileRT, RenderData.TopProfileTexture);
	Upload(CloudMaterialParams::AnvilProfileRT, RenderData.AnvilProfileTexture);
}

UMaterialInstanceDynamic* UStormMaterialBinderComponent::ApplyVolumetricCloudMaterial()
{
	UMaterialInstanceDynamic* Material = EnsureDynamicCloudMaterial(true);
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

	const FStormRenderData& RenderData = StormActor->GetStormRenderDataRef();
	CaptureScaleReferenceIfNeeded(RenderData);
	UploadAllParamsToMID(RenderData);
	LastFullyUploadedMaterial = DynamicCloudMaterial.Get();
}

void UStormMaterialBinderComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	TryAutoResolveTargetCloud();
}

void UStormMaterialBinderComponent::BeginPlay()
{
	Super::BeginPlay();

	TryAutoResolveTargetCloud();
	RefreshCloudMaterialBinding(false);
}

void UStormMaterialBinderComponent::RefreshCloudMaterialBinding(bool bReapplyCloudMaterial)
{
	if (bReapplyCloudMaterial)
	{
		EnsureDynamicCloudMaterial(true);
	}

	if (const AVolumetricSuperStormActor* StormActor = Cast<AVolumetricSuperStormActor>(GetOwner()))
	{
		PushStormRenderData(StormActor->GetStormRenderDataRef());
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

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	const bool bTargetChanged   = MemberName == GET_MEMBER_NAME_CHECKED(UStormMaterialBinderComponent, TargetCloudReference);
	const bool bMaterialChanged = bTargetChanged || PropertyName == GET_MEMBER_NAME_CHECKED(UStormMaterialBinderComponent, BaseCloudMaterial);

	if (bTargetChanged)
	{
		ReleaseVolumetricCloudMaterial();
		ResolvedTargetCloud.Reset();
	}

	RefreshCloudMaterialBinding(bMaterialChanged);
}

#endif

UVolumetricCloudComponent* UStormMaterialBinderComponent::ResolveTargetCloud() const
{
	if (ResolvedTargetCloud.IsValid() && ResolvedTargetCloud->GetWorld() == GetWorld())
	{
		return ResolvedTargetCloud.Get();
	}

	UVolumetricCloudComponent* Cloud = nullptr;
	if (IsTargetCloudReferenceSet())
	{
		Cloud = Cast<UVolumetricCloudComponent>(TargetCloudReference.GetComponent(GetOwner()));
	}
	else
	{
		Cloud = FindUniqueWorldCloud();
	}

	if (!IsValid(Cloud) || Cloud->GetWorld() != GetWorld())
	{
		ResolvedTargetCloud.Reset();
		return nullptr;
	}

	bReportedAmbiguousCloudTarget = false;
	ResolvedTargetCloud           = Cloud;
	return Cloud;
}

void UStormMaterialBinderComponent::ReleaseVolumetricCloudMaterial()
{
	UVolumetricCloudComponent* Cloud      = ResolveTargetCloud();
	UMaterialInstanceDynamic*  CurrentMID = Cloud ? Cast<UMaterialInstanceDynamic>(Cloud->GetMaterial()) : nullptr;
	if (Cloud && CurrentMID && CurrentMID->GetOuter() == this && bHasCapturedPreviousCloudMaterial)
	{
		Cloud->SetMaterial(PreviousCloudMaterial.Get());
	}

	DynamicCloudMaterial              = nullptr;
	PreviousCloudMaterial             = nullptr;
	bHasCapturedPreviousCloudMaterial = false;
	LastFullyUploadedMaterial.Reset();
	ResolvedTargetCloud.Reset();
	bReportedAmbiguousCloudTarget = false;
	bReportedInvalidMaterial      = false;
	ScaleReferenceRadius          = 0.0f;
}

UMaterialInstanceDynamic* UStormMaterialBinderComponent::GetDynamicCloudMaterial() const
{
	return DynamicCloudMaterial;
}

void UStormMaterialBinderComponent::ApplyLightningMaterialState(const FStormLightningMaterialState& State)
{
	UMaterialInstanceDynamic* Material = EnsureDynamicCloudMaterial();
	if (!Material)
	{
		return;
	}

	Material->SetVectorParameterValue(CloudMaterialParams::LightningCenter, FLinearColor(State.CenterN.X, State.CenterN.Y, State.CenterN.Z, 0.0f));
	Material->SetVectorParameterValue(CloudMaterialParams::LightningFlashExtent, FLinearColor(State.FlashExtentN.X, State.FlashExtentN.Y, State.FlashExtentN.Z, 0.0f));
	Material->SetVectorParameterValue(CloudMaterialParams::LightningHaloExtent, FLinearColor(State.HaloExtentN.X, State.HaloExtentN.Y, State.HaloExtentN.Z, 0.0f));
	Material->SetVectorParameterValue(CloudMaterialParams::LightningHotWhiteColor, State.HotWhiteColor);
	Material->SetVectorParameterValue(CloudMaterialParams::LightningFillColor, State.FillColor);
	Material->SetVectorParameterValue(CloudMaterialParams::LightningLeakColor, State.LeakColor);
	Material->SetScalarParameterValue(CloudMaterialParams::LightningCorePeak, FMath::Max(0.0f, State.CorePeakHDR));
	Material->SetScalarParameterValue(CloudMaterialParams::LightningFillIntensity, FMath::Max(0.0f, State.FillIntensity));
	Material->SetScalarParameterValue(CloudMaterialParams::LightningLeakIntensity, FMath::Max(0.0f, State.LeakIntensity));
}

void UStormMaterialBinderComponent::SetLightningPulse(float Pulse)
{
	if (UMaterialInstanceDynamic* Material = EnsureDynamicCloudMaterial())
	{
		Material->SetScalarParameterValue(CloudMaterialParams::LightningPulse, FMath::Max(0.0f, Pulse));
	}
}

void UStormMaterialBinderComponent::ClearLightningMaterialState()
{
	SetLightningPulse(0.0f);
}

void UStormMaterialBinderComponent::CaptureScaleReferenceIfNeeded(const FStormRenderData& RenderData)
{
	if (ScaleReferenceRadius > 0.0f)
	{
		return;
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		if (!RenderData.WorldCenter.Equals(OwnerActor->GetActorLocation(), 1.0))
		{
			return;
		}
	}

	ScaleReferenceRadius = FMath::Max(1.0f, RenderData.Shape.Radius);
}

bool UStormMaterialBinderComponent::IsTargetCloudReferenceSet() const
{
	return TargetCloudReference.OtherActor.IsValid() || TargetCloudReference.OverrideComponent.IsValid() || TargetCloudReference.ComponentProperty != NAME_None || !TargetCloudReference.PathToComponent.IsEmpty();
}

UVolumetricCloudComponent* UStormMaterialBinderComponent::FindUniqueWorldCloud() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UVolumetricCloudComponent* Cloud      = nullptr;
	int32                      CloudCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UVolumetricCloudComponent*> CloudComponents(*It);
		for (UVolumetricCloudComponent* Candidate : CloudComponents)
		{
			if (IsValid(Candidate))
			{
				Cloud = Candidate;
				++CloudCount;
			}
		}
	}

	if (CloudCount == 1)
	{
		bReportedAmbiguousCloudTarget = false;
		return Cloud;
	}

	if (CloudCount > 1 && !bReportedAmbiguousCloudTarget)
	{
		UE_LOG(LogSavageSuperStormRuntime, Error, TEXT("Storm '%s' found %d volumetric cloud components. Set Target Cloud Reference explicitly."), *GetNameSafe(GetOwner()), CloudCount);
		bReportedAmbiguousCloudTarget = true;
	}

	return nullptr;
}

void UStormMaterialBinderComponent::TryAutoResolveTargetCloud()
{
	if (IsTargetCloudReferenceSet())
	{
		return;
	}

	UVolumetricCloudComponent* Cloud = FindUniqueWorldCloud();
	if (!Cloud)
	{
		return;
	}

	ResolvedTargetCloud = Cloud;
	if (!BaseCloudMaterial)
	{
		BaseCloudMaterial = Cloud->GetMaterial();
	}
}

UMaterialInstanceDynamic* UStormMaterialBinderComponent::EnsureDynamicCloudMaterial(bool bForceRecreate)
{
	const AVolumetricSuperStormActor* StormOwner      = Cast<AVolumetricSuperStormActor>(GetOwner());
	UWorld*                           World           = GetWorld();
	const UStormRenderWorldSubsystem* RenderSubsystem = World ? World->GetSubsystem<UStormRenderWorldSubsystem>() : nullptr;
	if (!StormOwner || !RenderSubsystem || !RenderSubsystem->IsRegisteredStorm(StormOwner))
	{
		return nullptr;
	}

	UVolumetricCloudComponent* Cloud = ResolveTargetCloud();
	if (!Cloud)
	{
		return nullptr;
	}

	UMaterialInterface*       CurrentMaterial = Cloud->GetMaterial();
	UMaterialInstanceDynamic* CurrentMID      = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
	const bool                bOwnedByThis    = CurrentMID && CurrentMID->GetOuter() == this;
	if (!bOwnedByThis)
	{
		PreviousCloudMaterial             = CurrentMaterial;
		bHasCapturedPreviousCloudMaterial = true;
	}

	if (!bForceRecreate && bOwnedByThis)
	{
		const bool bReparentNeeded = BaseCloudMaterial && CurrentMID->Parent != BaseCloudMaterial;
		if (!bReparentNeeded)
		{
			DynamicCloudMaterial = CurrentMID;
			return CurrentMID;
		}
	}

	UMaterialInterface* Parent = BaseCloudMaterial ? BaseCloudMaterial.Get() : (bOwnedByThis ? CurrentMID->Parent.Get() : CurrentMaterial);
	if (!Parent)
	{
		return nullptr;
	}

	if (!CloudMaterialParams::HasStormMaterialContract(Parent))
	{
		if (!bReportedInvalidMaterial)
		{
			UE_LOG(LogSavageSuperStormRuntime, Error, TEXT("Material '%s' does not implement the SavageSuperStorm cloud contract."), *GetNameSafe(Parent));
			bReportedInvalidMaterial = true;
		}
		return nullptr;
	}
	bReportedInvalidMaterial = false;

	UMaterialInstanceDynamic* CachedMID          = DynamicCloudMaterial.Get();
	const bool                bCanReuseCachedMID = !bForceRecreate && IsValid(CachedMID) && CachedMID->GetOuter() == this && CachedMID->Parent == Parent;

	if (bCanReuseCachedMID)
	{
		Cloud->SetMaterial(CachedMID);
		return CachedMID;
	}

	DynamicCloudMaterial = UMaterialInstanceDynamic::Create(Parent, this);
	if (DynamicCloudMaterial)
	{
		Cloud->SetMaterial(DynamicCloudMaterial);
	}

	return DynamicCloudMaterial;
}

void UStormMaterialBinderComponent::UploadAllParamsToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial)
	{
		return;
	}
	UploadTexturesToMID(RenderData);
	UploadLayerParametersToMID(RenderData);
	UploadShapeSettingsToMID(RenderData);
	UploadMotionSettingsToMID(RenderData);
	UploadPerFrameParamsToMID(RenderData);
}

void UStormMaterialBinderComponent::UploadTexturesToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial)
	{
		return;
	}

	auto Upload = [](UMaterialInstanceDynamic* MID, FName TextureName, UTexture* Texture)
	{
		if (MID)
		{
			MID->SetTextureParameterValue(TextureName, Texture);
		}
	};

	UTextureRenderTarget2D* ShapeRenderTarget  = nullptr;
	UTextureRenderTarget2D* ShapeRenderTarget2 = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (UStormRenderWorldSubsystem* RenderSubsystem = World->GetSubsystem<UStormRenderWorldSubsystem>())
		{
			ShapeRenderTarget  = RenderSubsystem->GetShapeRenderTarget();
			ShapeRenderTarget2 = RenderSubsystem->GetShapeRenderTarget2();
		}
	}

	Upload(DynamicCloudMaterial, CloudMaterialParams::ShapeTexture, ShapeRenderTarget);
	Upload(DynamicCloudMaterial, CloudMaterialParams::ShapeTexture2, ShapeRenderTarget2);
	Upload(DynamicCloudMaterial, CloudMaterialParams::BottomProfileRT, RenderData.BottomProfileTexture);
	Upload(DynamicCloudMaterial, CloudMaterialParams::TopProfileRT, RenderData.TopProfileTexture);
	Upload(DynamicCloudMaterial, CloudMaterialParams::AnvilProfileRT, RenderData.AnvilProfileTexture);
	Upload(DynamicCloudMaterial, CloudMaterialParams::FlowMapLower, RenderData.FlowMap.LowerTexture);
	Upload(DynamicCloudMaterial, CloudMaterialParams::FlowMapMiddle, RenderData.FlowMap.MiddleTexture);
	Upload(DynamicCloudMaterial, CloudMaterialParams::FlowMapUpper, RenderData.FlowMap.UpperTexture);
}

void UStormMaterialBinderComponent::UploadLayerParametersToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial)
	{
		return;
	}
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::FlowMapLayerHeights, FLinearColor(RenderData.FlowMap.LayerHeights.X, RenderData.FlowMap.LayerHeights.Y, RenderData.FlowMap.LayerHeights.Z, 0.0f));
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::FlowMapUVWStrength, FLinearColor(RenderData.FlowMap.UVWStrength.X, RenderData.FlowMap.UVWStrength.Y, RenderData.FlowMap.UVWStrength.Z, FMath::Max(RenderData.FlowMap.CycleDurationSeconds, 0.1f)));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::FlowMapEnabled, RenderData.FlowMap.bEnabled ? 1.0f : 0.0f);
}

void UStormMaterialBinderComponent::UploadFrameSpatialParamsToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial)
	{
		return;
	}

	const float CurrentRadius   = FMath::Max(1.0f, RenderData.Shape.Radius);
	const float ReferenceRadius = ScaleReferenceRadius > 0.0f ? ScaleReferenceRadius : CurrentRadius;

	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormCenterRadius, FLinearColor(RenderData.WorldCenter.X, RenderData.WorldCenter.Y, CurrentRadius, 0.0f));
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormCenter, FLinearColor(RenderData.WorldCenter.X, RenderData.WorldCenter.Y, 0.0f, 0.0f));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::StormCoordinateScale, ReferenceRadius / CurrentRadius);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormExtent, FLinearColor(FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.X))), FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.Y))), FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.Z))), 0.0f));
}

void UStormMaterialBinderComponent::UploadShapeSettingsToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial)
	{
		return;
	}
	const FStormShapeSettings& Shape                = RenderData.Shape;
	const float                CurrentRadius        = FMath::Max(1.0f, Shape.Radius);
	const float                ReferenceRadius      = ScaleReferenceRadius > 0.0f ? ScaleReferenceRadius : CurrentRadius;
	const float                ReferenceRadiusKm    = ReferenceRadius / CloudMaterialParams::KilometersToCentimeters;
	const float                LFNoiseWorldSizeKm   = DynamicCloudMaterial->K2_GetScalarParameterValue(CloudMaterialParams::LFNoiseWorldSize);
	const float                HFNoiseWorldSizeKm   = DynamicCloudMaterial->K2_GetScalarParameterValue(CloudMaterialParams::HFNoiseWorldSize);
	const float                CurlNoiseWorldSizeKm = DynamicCloudMaterial->K2_GetScalarParameterValue(CloudMaterialParams::CurlNoiseWorldSize);
	const auto                 UnitsPerBodyRadius   = [ReferenceRadiusKm](const float NoiseWorldSizeKm)
	{
		constexpr float NoiseWorldSizeEpsilonKm = 1.0e-6f;
		return ReferenceRadiusKm / FMath::Max(FMath::Abs(NoiseWorldSizeKm), NoiseWorldSizeEpsilonKm);
	};
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormCenterRadius, FLinearColor(RenderData.WorldCenter.X, RenderData.WorldCenter.Y, CurrentRadius, 0.0f));
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormCenter, FLinearColor(RenderData.WorldCenter.X, RenderData.WorldCenter.Y, 0.0f, 0.0f));
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormBaseCloudColor, Shape.StormBaseColor);

	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormReferenceCenter, FLinearColor::Black);
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::StormCoordinateScale, ReferenceRadius / CurrentRadius);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::StormExtent, FLinearColor(FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.X))), FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.Y))), FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.Z))), 0.0f));

	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::RotationSign, Shape.bClockwise ? 1.0f : -1.0f);
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::AnvilTwistRadians, FMath::DegreesToRadians(FMath::Clamp(Shape.AnvilHeightTwistDegrees, -30.0f, 30.0f)));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::AnvilTwistOrigin, FMath::Clamp(Shape.AnvilHeightTwistStart01, 0.0f, 0.9f));

	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::OuterBrimRadiusScale, Shape.GetAnvilOuterRadiusScale());

	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::LFUnitsPerBodyRadius, UnitsPerBodyRadius(LFNoiseWorldSizeKm));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::HFUnitsPerBodyRadius, UnitsPerBodyRadius(HFNoiseWorldSizeKm));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::CurlUnitsPerBodyRadius, UnitsPerBodyRadius(CurlNoiseWorldSizeKm));

	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::DensityMultiplier, FMath::Max(0.0f, Shape.Density));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::DensityGamma, FMath::Clamp(Shape.DensityGamma, 1.0e-3f, 4.0f));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::HFStrength, FMath::Clamp(Shape.HFStrength, 0.0f, 1.0f));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::UndersideVisibility, FMath::Clamp(Shape.UndersideVisibility, 0.0f, 1.0f));
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::BrimEmissiveColor, Shape.BrimEmissiveColor);
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::AnvilStrength, FMath::Clamp(Shape.AnvilStrength, 0.0f, 1.0f));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::AnvilDepth01, FMath::Clamp(Shape.AnvilDepth01, 0.0f, 1.0f));
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::AnvilAnchorHeight01, 1.0f);
}

void UStormMaterialBinderComponent::UploadMotionSettingsToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial)
	{
		return;
	}

	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::MDRBoundaries0, RenderData.MDRBoundaries0);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::MDRBoundaries1, RenderData.MDRBoundaries1);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::MDRSpeeds0, RenderData.MDRSpeeds0);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::MDRSpeeds1, RenderData.MDRSpeeds1);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::MDRPhases0, RenderData.MDRPhases0);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::MDRPhases1, RenderData.MDRPhases1);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::MDRSkews0, RenderData.MDRSkews0);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::MDRSkews1, RenderData.MDRSkews1);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::MDRControl0, RenderData.MDRControl0);
	DynamicCloudMaterial->SetVectorParameterValue(CloudMaterialParams::MDRControl1, RenderData.MDRControl1);

	float RingInfluenceDebug = 0.0f;
#if WITH_EDITORONLY_DATA
	RingInfluenceDebug = RenderData.Motion.bDebugDrawRingEndRadii ? 1.0f : 0.0f;
#endif
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::MDRDebugRingInfluence, RingInfluenceDebug);
}

void UStormMaterialBinderComponent::UploadPerFrameParamsToMID(const FStormRenderData& RenderData) const
{
	if (!DynamicCloudMaterial)
	{
		return;
	}
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::StormTimeSeconds, RenderData.TimeSeconds);
	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::DensityMultiplier, RenderData.bLifecycleWarmup ? 0.0f : FMath::Max(0.0f, RenderData.Shape.Density));

	float EncodedLifecycle = RenderData.bLifecycleWarmup ? 0.42f : 1.0f;
	if (!RenderData.bLifecycleWarmup)
	{
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
		case EStormFormationState::Mature: default:
			EncodedLifecycle = 1.0f;
			break;
		}
	}

	DynamicCloudMaterial->SetScalarParameterValue(CloudMaterialParams::LifecycleControl, EncodedLifecycle);
}
