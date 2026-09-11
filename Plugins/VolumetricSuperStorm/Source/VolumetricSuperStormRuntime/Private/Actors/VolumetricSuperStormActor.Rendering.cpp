// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormActor.Rendering.cpp
 * @brief Builds render data and coordinates the single-storm render subsystem.
 */

#include "Actors/VolumetricSuperStormActor.h"

#include "Components/StormFlowMapComponent.h"
#include "Components/StormMaterialBinderComponent.h"
#include "Components/StormVerticalProfileToolComponent.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Material/StormMaterialPayload.h"
#include "Motion/StormMotionSettingsUtils.h"
#include "Subsystems/StormRenderWorldSubsystem.h"

namespace
{
	FVector SanitizeStormExtent(const FVector& Extent)
	{
		return FVector(FMath::Max(1.0, Extent.X), FMath::Max(1.0, Extent.Y), FMath::Max(1.0, Extent.Z));
	}

	FStormShapeSettings SanitizeStormShape(const FStormShapeSettings& Shape)
	{
		FStormShapeSettings Result         = Shape;
		Result.Radius                      = FMath::Max(1.0f, Shape.Radius);
		Result.CoordinateScale             = FMath::Max(0.01f, Shape.CoordinateScale);
		Result.Extent                      = SanitizeStormExtent(Shape.Extent);
		Result.Extent.X                    = FMath::Max(Result.Extent.X, static_cast<double>(Result.Radius));
		Result.Extent.Y                    = FMath::Max(Result.Extent.Y, static_cast<double>(Result.Radius));
		Result.UndersideVisibility         = FMath::Clamp(Shape.UndersideVisibility, 0.0f, 1.0f);
		Result.StormStaticGlowIntensity    = FMath::Max(0.0f, Shape.StormStaticGlowIntensity);
		Result.GlowExtent                  = FVector(
			FMath::Max(1.0e-3, Shape.GlowExtent.X),
			FMath::Max(1.0e-3, Shape.GlowExtent.Y),
			FMath::Max(1.0e-3, Shape.GlowExtent.Z));
		Result.DensityGamma                = FMath::Clamp(Shape.DensityGamma, 1.0e-3f, 4.0f);
		Result.HFStrength                  = FMath::Clamp(Shape.HFStrength, 0.0f, 1.0f);
		Result.AnvilCoverage               = FMath::Clamp(Shape.AnvilCoverage, 0.0f, 1.0f);
		Result.AnvilStrength               = FMath::Clamp(Shape.AnvilStrength, 0.0f, 1.0f);
		Result.AnvilDepth01                = FMath::Clamp(Shape.AnvilDepth01, 0.0f, 1.0f);
		Result.AnvilHeightTwistDegrees     = FMath::Clamp(Shape.AnvilHeightTwistDegrees, -30.0f, 30.0f);
		Result.AnvilHeightTwistStart01     = FMath::Clamp(Shape.AnvilHeightTwistStart01, 0.0f, 0.9f);
		return Result;
	}

	FVector GetSafeWindDirection(const FVector& Direction)
	{
		const FVector2D HorizontalDirection(Direction.X, Direction.Y);
		const FVector2D Normalized = HorizontalDirection.GetSafeNormal();
		return Normalized.IsNearlyZero() ? FVector::ForwardVector : FVector(Normalized.X, Normalized.Y, 0.0);
	}

	constexpr double FeederDomainPaddingBodyRadii = 0.68;
}

void AVolumetricSuperStormActor::RequestTextureContentUpdate(EStormTextureDirtyFlags DirtyTextures)
{
	if (!MaterialBinder || DirtyTextures == EStormTextureDirtyFlags::None)
	{
		return;
	}

	const EStormTextureDirtyFlags ProfileTextures =
		EStormTextureDirtyFlags::ProfileBottom |
		EStormTextureDirtyFlags::ProfileTop |
		EStormTextureDirtyFlags::ProfileAnvil;
	const EStormTextureDirtyFlags FlowTextures =
		EStormTextureDirtyFlags::FlowLower |
		EStormTextureDirtyFlags::FlowMiddle |
		EStormTextureDirtyFlags::FlowUpper;
	if (EnumHasAnyFlags(DirtyTextures, ProfileTextures))
	{
		UpdateProfileRenderData();
	}
	if (EnumHasAnyFlags(DirtyTextures, FlowTextures))
	{
		UpdateFlowMapRenderData();
	}

	VolumetricSuperStorm::FStormMaterialTextureSources TextureSources;
	if (UWorld* World = GetWorld())
	{
		if (UStormRenderWorldSubsystem* Subsystem = World->GetSubsystem<UStormRenderWorldSubsystem>())
		{
			TextureSources.Shape = Subsystem->GetShapeRenderTarget();
			TextureSources.Shape2 = Subsystem->GetShapeRenderTarget2();
		}
	}

	VolumetricSuperStorm::FStormMaterialPayload Payload;
	VolumetricSuperStorm::BuildTextureStormMaterialPayload(
		DirtyTextures,
		CachedRenderData,
		TextureSources,
		Payload);
	MaterialBinder->QueueStormTexturePayload(Payload);
}

void AVolumetricSuperStormActor::RebuildRenderData()
{
	RequestRenderDataUpdate(EStormRenderUpdateScope::Full);
}

int32 AVolumetricSuperStormActor::GetStableStormId() const
{
	return ResolveStableStormId();
}

FStormRenderData AVolumetricSuperStormActor::GetStormRenderData() const
{
	return CachedRenderData;
}

const FStormRenderData& AVolumetricSuperStormActor::GetStormRenderDataRef() const
{
	return CachedRenderData;
}

bool AVolumetricSuperStormActor::RegisterWithRenderSubsystem()
{
	UWorld*                     World     = GetWorld();
	UStormRenderWorldSubsystem* Subsystem = World ? World->GetSubsystem<UStormRenderWorldSubsystem>() : nullptr;
	if (!Subsystem)
	{
		bRegisteredWithRenderSubsystem = false;
		return false;
	}

	if (bRegisteredWithRenderSubsystem && Subsystem->IsRegisteredStorm(this))
	{
		return true;
	}

	bRegisteredWithRenderSubsystem = Subsystem->RegisterStorm(this);
	SetActorTickEnabled(bRegisteredWithRenderSubsystem);
	return bRegisteredWithRenderSubsystem;
}

void AVolumetricSuperStormActor::UnregisterFromRenderSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (UStormRenderWorldSubsystem* Subsystem = World->GetSubsystem<UStormRenderWorldSubsystem>())
		{
			Subsystem->UnregisterStorm(this);
		}
	}
	bRegisteredWithRenderSubsystem = false;
}

int32 AVolumetricSuperStormActor::ResolveStableStormId() const
{
	if (StableStormId > 0)
	{
		return StableStormId;
	}

	const uint32 NameHash = GetTypeHash(GetFName());
	return FMath::Max(1, static_cast<int32>(NameHash & 0x7fffffffu));
}

void AVolumetricSuperStormActor::RequestRenderDataUpdate(EStormRenderUpdateScope Scope)
{
	if (!RegisterWithRenderSubsystem())
	{
		return;
	}

	switch (Scope)
	{
	case EStormRenderUpdateScope::Full:
		UpdateStaticRenderData();
		UpdateFrameRenderData();
		break;
	case EStormRenderUpdateScope::Profile:
		UpdateProfileRenderData();
		break;
	case EStormRenderUpdateScope::Frame: default:
		UpdateFrameRenderData();
		break;
	}

	PublishRenderDataUpdate(Scope);
}

void AVolumetricSuperStormActor::PublishRenderDataUpdate(EStormRenderUpdateScope Scope)
{
	if (Scope == EStormRenderUpdateScope::Full)
	{
		if (UWorld* World = GetWorld())
		{
			if (UStormRenderWorldSubsystem* Subsystem = World->GetSubsystem<UStormRenderWorldSubsystem>())
			{
				if (!Subsystem->SubmitStormRenderData(this, CachedRenderData))
				{
					return;
				}
			}
		}
	}

	if (MaterialBinder)
	{
		MaterialBinder->PushStormData(CachedRenderData, Scope);
	}
}

void AVolumetricSuperStormActor::UpdateStaticRenderData()
{
	ShapeSettings.ShapeCurves.EnsureNamedCurves();

	const FStormShapeSettings  SanitizedShape  = SanitizeStormShape(ShapeSettings);
	const FStormMotionSettings SanitizedMotion = VolumetricSuperStorm::Motion::SanitizeSettings(MotionSettings);
	SynchronizeMotionPhaseCount(SanitizedMotion);

	FVector      FieldExtent              = SanitizedShape.Extent;
	const double SupportScale             = static_cast<double>(SanitizedShape.GetAnvilOuterRadiusScale()) + FeederDomainPaddingBodyRadii;
	const double RequiredHorizontalExtent = static_cast<double>(SanitizedShape.Radius) * SupportScale;
	FieldExtent.X                         = FMath::Max(FieldExtent.X, RequiredHorizontalExtent);
	FieldExtent.Y                         = FMath::Max(FieldExtent.Y, RequiredHorizontalExtent);

	CachedRenderData.WorldExtent   = FieldExtent;
	CachedRenderData.Shape         = SanitizedShape;
	CachedRenderData.WindDirection = GetSafeWindDirection(SanitizedShape.WindDirection);
	CachedRenderData.Motion        = SanitizedMotion;
	UpdateProfileRenderData();
	UpdateFlowMapRenderData();
}

void AVolumetricSuperStormActor::UpdateFrameRenderData()
{
	const FStormShapeSettings& Shape           = CachedRenderData.Shape;
	const FStormMotionSettings SanitizedMotion = VolumetricSuperStorm::Motion::SanitizeSettings(MotionSettings);
	SynchronizeMotionPhaseCount(SanitizedMotion);
	CachedRenderData.Motion            = SanitizedMotion;
	CachedRenderData.WorldCenter       = GetActorLocation();
	CachedRenderData.TimeSeconds       = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	CachedRenderData.FormationState    = FormationState;
	CachedRenderData.FormationProgress = FMath::Clamp(FormationProgress, 0.0f, 1.0f);
	CachedRenderData.bLifecycleWarmup  = LifecycleWarmupFramesRemaining > 0;

	const float RotationDirection = Shape.bClockwise ? 1.0f : -1.0f;
	const float OuterRadiusScale  = Shape.GetAnvilOuterRadiusScale();
	const auto  ToBodyRadius      = [OuterRadiusScale](float OuterRadius01)
	{
		return VolumetricSuperStorm::Motion::OuterBrimRadius01ToBodyRadius(OuterRadius01, OuterRadiusScale);
	};
	const auto BoundaryBodyRadius = [&SanitizedMotion, &ToBodyRadius](int32 BoundaryIndex)
	{
		const float Radius01 = SanitizedMotion.RingEndRadii01.IsValidIndex(BoundaryIndex) ? SanitizedMotion.RingEndRadii01[BoundaryIndex] : SanitizedMotion.MotionRadiusScale;
		return ToBodyRadius(Radius01);
	};
	const auto Phase = [this](int32 RingIndex)
	{
		return RingPhases.IsValidIndex(RingIndex) ? static_cast<float>(RingPhases[RingIndex].WrappedRadians) : 0.0f;
	};
	const auto Turns = [this](int32 RingIndex)
	{
		return RingPhases.IsValidIndex(RingIndex) ? static_cast<float>(RingPhases[RingIndex].TurnCount) : 0.0f;
	};
	const auto Skew = [&SanitizedMotion](int32 RingIndex)
	{
		const float Degrees = SanitizedMotion.RingSkewDegrees.IsValidIndex(RingIndex) ? SanitizedMotion.RingSkewDegrees[RingIndex] : SanitizedMotion.RingSkewDegrees.Last();
		return FMath::DegreesToRadians(Degrees);
	};

	CachedRenderData.MDRBoundaries0 = FLinearColor(BoundaryBodyRadius(0), BoundaryBodyRadius(1), BoundaryBodyRadius(2), BoundaryBodyRadius(3));
	CachedRenderData.MDRBoundaries1 = FLinearColor(BoundaryBodyRadius(4), ToBodyRadius(SanitizedMotion.BoundaryOverlap01), ToBodyRadius(SanitizedMotion.MotionRadiusScale), ToBodyRadius(SanitizedMotion.RadialFeather01));
	CachedRenderData.MDRSpeeds0     = FLinearColor(Turns(0), Turns(1), Turns(2), Turns(3));
	CachedRenderData.MDRSpeeds1     = FLinearColor(Turns(4), Turns(5), SanitizedMotion.MotionStrength, SanitizedMotion.bEnabled ? 1.0f : 0.0f);
	CachedRenderData.MDRPhases0     = FLinearColor(Phase(0), Phase(1), Phase(2), Phase(3));
	CachedRenderData.MDRPhases1     = FLinearColor(Phase(4), Phase(5), SanitizedMotion.RadialShearGain, RotationDirection);
	CachedRenderData.MDRSkews0      = FLinearColor(Skew(0), Skew(1), Skew(2), Skew(3));
	CachedRenderData.MDRSkews1      = FLinearColor(Skew(4), Skew(5), SanitizedMotion.HeightMin01, SanitizedMotion.HeightMax01);
	CachedRenderData.MDRControl0    = FLinearColor(SanitizedMotion.HeightFeather01, static_cast<float>(static_cast<uint8>(SanitizedMotion.TransitionMode)), SanitizedMotion.LFRotationMultiplier, SanitizedMotion.HFRotationMultiplier);
	CachedRenderData.MDRControl1    = FLinearColor(SanitizedMotion.CurlRotationMultiplier, static_cast<float>(ResolveStableStormId()), static_cast<float>(SanitizedMotion.RingCount), SanitizedMotion.TimeScale);
}

void AVolumetricSuperStormActor::UpdateProfileRenderData()
{
	FStormProfileRenderData ProfileData;
	if (VerticalProfileTool)
	{
		VerticalProfileTool->GetProfileRenderData(ProfileData);
	}

	CachedRenderData.BottomProfileTexture = ProfileData.BottomProfile;
	CachedRenderData.TopProfileTexture    = ProfileData.TopProfile;
	CachedRenderData.AnvilProfileTexture  = ProfileData.AnvilProfile;
}

void AVolumetricSuperStormActor::UpdateFlowMapRenderData()
{
	FStormFlowMapRenderData FlowMapData;
	if (FlowMapComponent)
	{
		FlowMapComponent->GetFlowMapRenderData(FlowMapData);
	}
	CachedRenderData.FlowMap = FlowMapData;
}
