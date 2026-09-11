// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormMaterialPayload.cpp
 * @brief Encodes storm render data into backend-neutral material parameters.
 */

#include "Material/StormMaterialPayload.h"

#include "Data/StormLightningTypes.h"
#include "Data/StormRenderData.h"
#include "Material/StormCloudMaterialParameters.h"

namespace VolumetricSuperStorm
{
	namespace
	{
		void AddScalar(FStormMaterialPayload& Payload, FName Name, float Value)
		{
			Payload.Scalars.Add({ Name, Value });
		}

		void AddVector(FStormMaterialPayload& Payload, FName Name, const FLinearColor& Value)
		{
			Payload.Vectors.Add({ Name, Value });
		}

		void AddTexture(
			FStormMaterialPayload& Payload,
			EStormMaterialTextureSlot Slot,
			FName ParameterName,
			UTexture* Texture)
		{
			Payload.Textures.Add({ Slot, ParameterName, Texture });
		}

		float ResolveCurrentRadius(const FStormRenderData& RenderData)
		{
			return FMath::Max(1.0f, RenderData.Shape.Radius);
		}

		float ResolveCoordinateScale(const FStormRenderData& RenderData)
		{
			return FMath::Max(0.01f, RenderData.Shape.CoordinateScale);
		}

		FLinearColor MakeStormExtent(const FStormRenderData& RenderData)
		{
			return FLinearColor(
				FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.X))),
				FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.Y))),
				FMath::Max(1.0f, static_cast<float>(FMath::Abs(RenderData.WorldExtent.Z))),
				0.0f);
		}

		float EncodeLifecycle(const FStormRenderData& RenderData)
		{
			if (RenderData.bLifecycleWarmup)
			{
				return 0.42f;
			}

			switch (RenderData.FormationState)
			{
			case EStormFormationState::Hidden:
				return 3.0f;
			case EStormFormationState::Forming:
				return FMath::Clamp(RenderData.FormationProgress, 0.0f, 0.998f);
			case EStormFormationState::Dissolving:
				return 2.0f + FMath::Clamp(RenderData.FormationProgress, 0.0f, 0.998f);
			case EStormFormationState::Mature:
			default:
				return 1.0f;
			}
		}

		void AppendAllTextures(
			const FStormRenderData& RenderData,
			const FStormMaterialTextureSources& TextureSources,
			FStormMaterialPayload& Payload)
		{
			AppendStormMaterialTexture(EStormMaterialTextureSlot::Shape, TextureSources.Shape, Payload);
			AppendStormMaterialTexture(EStormMaterialTextureSlot::Shape2, TextureSources.Shape2, Payload);
			AppendStormMaterialTexture(EStormMaterialTextureSlot::BottomProfile, RenderData.BottomProfileTexture, Payload);
			AppendStormMaterialTexture(EStormMaterialTextureSlot::TopProfile, RenderData.TopProfileTexture, Payload);
			AppendStormMaterialTexture(EStormMaterialTextureSlot::AnvilProfile, RenderData.AnvilProfileTexture, Payload);
			AppendStormMaterialTexture(EStormMaterialTextureSlot::FlowLower, RenderData.FlowMap.LowerTexture, Payload);
			AppendStormMaterialTexture(EStormMaterialTextureSlot::FlowMiddle, RenderData.FlowMap.MiddleTexture, Payload);
			AppendStormMaterialTexture(EStormMaterialTextureSlot::FlowUpper, RenderData.FlowMap.UpperTexture, Payload);
		}

		void AppendProfileTextures(const FStormRenderData& RenderData, FStormMaterialPayload& Payload)
		{
			AppendStormMaterialTexture(EStormMaterialTextureSlot::BottomProfile, RenderData.BottomProfileTexture, Payload);
			AppendStormMaterialTexture(EStormMaterialTextureSlot::TopProfile, RenderData.TopProfileTexture, Payload);
			AppendStormMaterialTexture(EStormMaterialTextureSlot::AnvilProfile, RenderData.AnvilProfileTexture, Payload);
		}

		void AppendLayerParameters(const FStormRenderData& RenderData, FStormMaterialPayload& Payload)
		{
			using namespace CloudMaterialParams;
			AddVector(Payload, FlowMapLayerHeights, FLinearColor(
				RenderData.FlowMap.LayerHeights.X,
				RenderData.FlowMap.LayerHeights.Y,
				RenderData.FlowMap.LayerHeights.Z,
				0.0f));
			AddVector(Payload, FlowMapUVWStrength, FLinearColor(
				RenderData.FlowMap.UVWStrength.X,
				RenderData.FlowMap.UVWStrength.Y,
				RenderData.FlowMap.UVWStrength.Z,
				FMath::Max(RenderData.FlowMap.CycleDurationSeconds, 0.1f)));
			AddScalar(Payload, FlowMapEnabled, RenderData.FlowMap.bEnabled ? 1.0f : 0.0f);
		}

		void AppendFrameSpatialParameters(
			const FStormRenderData& RenderData,
			FStormMaterialPayload& Payload)
		{
			using namespace CloudMaterialParams;
			const float CurrentRadius = ResolveCurrentRadius(RenderData);
			AddVector(Payload, StormCenterRadius, FLinearColor(
				RenderData.WorldCenter.X,
				RenderData.WorldCenter.Y,
				CurrentRadius,
				0.0f));
			AddVector(Payload, StormCenter, FLinearColor(
				RenderData.WorldCenter.X,
				RenderData.WorldCenter.Y,
				0.0f,
				0.0f));
			AddScalar(Payload, StormCoordinateScale, ResolveCoordinateScale(RenderData));
			AddVector(Payload, StormExtent, MakeStormExtent(RenderData));
		}

		void AppendShapeSettings(
			const FStormRenderData& RenderData,
			FStormMaterialPayload& Payload)
		{
			using namespace CloudMaterialParams;
			const FStormShapeSettings& Shape = RenderData.Shape;
			const float CurrentRadius = ResolveCurrentRadius(RenderData);

			AddVector(Payload, StormCenterRadius, FLinearColor(
				RenderData.WorldCenter.X,
				RenderData.WorldCenter.Y,
				CurrentRadius,
				0.0f));
			AddVector(Payload, StormCenter, FLinearColor(
				RenderData.WorldCenter.X,
				RenderData.WorldCenter.Y,
				0.0f,
				0.0f));
			AddVector(Payload, StormBaseCloudColor, Shape.StormBaseColor);
			AddScalar(Payload, StaticGlowIntensity, FMath::Max(0.0f, Shape.StormStaticGlowIntensity));
			AddVector(Payload, StaticGlowColor, Shape.StormStaticGlowColor);
			AddVector(Payload, GlowExtent, FLinearColor(
				static_cast<float>(Shape.GlowExtent.X),
				static_cast<float>(Shape.GlowExtent.Y),
				static_cast<float>(Shape.GlowExtent.Z),
				0.0f));

			AddVector(Payload, StormReferenceCenter, FLinearColor::Black);
			AddScalar(Payload, StormCoordinateScale, ResolveCoordinateScale(RenderData));
			AddVector(Payload, StormExtent, MakeStormExtent(RenderData));
			AddScalar(Payload, RotationSign, Shape.bClockwise ? 1.0f : -1.0f);
			AddScalar(Payload, AnvilTwistRadians, FMath::DegreesToRadians(
				FMath::Clamp(Shape.AnvilHeightTwistDegrees, -30.0f, 30.0f)));
			AddScalar(Payload, AnvilTwistOrigin, FMath::Clamp(
				Shape.AnvilHeightTwistStart01,
				0.0f,
				0.9f));
			AddScalar(Payload, OuterBrimRadiusScale, Shape.GetAnvilOuterRadiusScale());
			AddScalar(Payload, DensityMultiplier, FMath::Max(0.0f, Shape.Density));
			AddScalar(Payload, DensityGamma, FMath::Clamp(Shape.DensityGamma, 1.0e-3f, 4.0f));
			AddScalar(Payload, HFStrength, FMath::Clamp(Shape.HFStrength, 0.0f, 1.0f));
			AddScalar(Payload, UndersideVisibility, FMath::Clamp(Shape.UndersideVisibility, 0.0f, 1.0f));
			AddVector(Payload, BrimEmissiveColor, Shape.BrimEmissiveColor);
			AddScalar(Payload, AnvilStrength, FMath::Clamp(Shape.AnvilStrength, 0.0f, 1.0f));
			AddScalar(Payload, AnvilDepth01, FMath::Clamp(Shape.AnvilDepth01, 0.0f, 1.0f));
			AddScalar(Payload, AnvilAnchorHeight01, 1.0f);
		}

		void AppendMotionParameters(const FStormRenderData& RenderData, FStormMaterialPayload& Payload)
		{
			using namespace CloudMaterialParams;
			AddVector(Payload, MDRBoundaries0, RenderData.MDRBoundaries0);
			AddVector(Payload, MDRBoundaries1, RenderData.MDRBoundaries1);
			AddVector(Payload, MDRSpeeds0, RenderData.MDRSpeeds0);
			AddVector(Payload, MDRSpeeds1, RenderData.MDRSpeeds1);
			AddVector(Payload, MDRPhases0, RenderData.MDRPhases0);
			AddVector(Payload, MDRPhases1, RenderData.MDRPhases1);
			AddVector(Payload, MDRSkews0, RenderData.MDRSkews0);
			AddVector(Payload, MDRSkews1, RenderData.MDRSkews1);
			AddVector(Payload, MDRControl0, RenderData.MDRControl0);
			AddVector(Payload, MDRControl1, RenderData.MDRControl1);

			float RingInfluenceDebug = 0.0f;
#if WITH_EDITORONLY_DATA
			RingInfluenceDebug = RenderData.Motion.bDebugDrawRingEndRadii ? 1.0f : 0.0f;
#endif
			AddScalar(Payload, MDRDebugRingInfluence, RingInfluenceDebug);
		}

		void AppendPerFrameParameters(const FStormRenderData& RenderData, FStormMaterialPayload& Payload)
		{
			using namespace CloudMaterialParams;
			AddScalar(Payload, StormTimeSeconds, RenderData.TimeSeconds);
			AddScalar(
				Payload,
				DensityMultiplier,
				RenderData.bLifecycleWarmup
					? 0.0f
					: FMath::Max(0.0f, RenderData.Shape.Density));
			AddScalar(Payload, LifecycleControl, EncodeLifecycle(RenderData));
		}
	}

	void FStormMaterialPayload::Reset()
	{
		Scalars.Reset();
		Vectors.Reset();
		Textures.Reset();
	}

	void AppendStormMaterialTexture(
		EStormMaterialTextureSlot Slot,
		UTexture* Texture,
		FStormMaterialPayload& OutPayload)
	{
		using namespace CloudMaterialParams;
		FName ParameterName = NAME_None;
		switch (Slot)
		{
		case EStormMaterialTextureSlot::Shape: ParameterName = ShapeTexture; break;
		case EStormMaterialTextureSlot::Shape2: ParameterName = ShapeTexture2; break;
		case EStormMaterialTextureSlot::BottomProfile: ParameterName = BottomProfileRT; break;
		case EStormMaterialTextureSlot::TopProfile: ParameterName = TopProfileRT; break;
		case EStormMaterialTextureSlot::AnvilProfile: ParameterName = AnvilProfileRT; break;
		case EStormMaterialTextureSlot::FlowLower: ParameterName = FlowMapLower; break;
		case EStormMaterialTextureSlot::FlowMiddle: ParameterName = FlowMapMiddle; break;
		case EStormMaterialTextureSlot::FlowUpper: ParameterName = FlowMapUpper; break;
		default: return;
		}

		AddTexture(OutPayload, Slot, ParameterName, Texture);
	}

	void BuildFullStormMaterialPayload(
		const FStormRenderData& RenderData,
		const FStormMaterialTextureSources& TextureSources,
		FStormMaterialPayload& OutPayload)
	{
		OutPayload.Reset();
		AppendAllTextures(RenderData, TextureSources, OutPayload);
		AppendLayerParameters(RenderData, OutPayload);
		AppendShapeSettings(RenderData, OutPayload);
		AppendMotionParameters(RenderData, OutPayload);
		AppendPerFrameParameters(RenderData, OutPayload);
	}

	void BuildFrameStormMaterialPayload(
		const FStormRenderData& RenderData,
		FStormMaterialPayload& OutPayload)
	{
		OutPayload.Reset();
		AppendFrameSpatialParameters(RenderData, OutPayload);
		AppendMotionParameters(RenderData, OutPayload);
		AppendPerFrameParameters(RenderData, OutPayload);
	}

	void BuildProfileStormMaterialPayload(
		const FStormRenderData& RenderData,
		FStormMaterialPayload& OutPayload)
	{
		OutPayload.Reset();
		AppendProfileTextures(RenderData, OutPayload);
	}

	void BuildTextureStormMaterialPayload(
		EStormTextureDirtyFlags DirtyTextures,
		const FStormRenderData& RenderData,
		const FStormMaterialTextureSources& TextureSources,
		FStormMaterialPayload& OutPayload)
	{
		OutPayload.Reset();
		if (EnumHasAnyFlags(DirtyTextures, EStormTextureDirtyFlags::Shape))
			AppendStormMaterialTexture(EStormMaterialTextureSlot::Shape, TextureSources.Shape, OutPayload);
		if (EnumHasAnyFlags(DirtyTextures, EStormTextureDirtyFlags::Shape2))
			AppendStormMaterialTexture(EStormMaterialTextureSlot::Shape2, TextureSources.Shape2, OutPayload);
		if (EnumHasAnyFlags(DirtyTextures, EStormTextureDirtyFlags::ProfileBottom))
			AppendStormMaterialTexture(EStormMaterialTextureSlot::BottomProfile, RenderData.BottomProfileTexture, OutPayload);
		if (EnumHasAnyFlags(DirtyTextures, EStormTextureDirtyFlags::ProfileTop))
			AppendStormMaterialTexture(EStormMaterialTextureSlot::TopProfile, RenderData.TopProfileTexture, OutPayload);
		if (EnumHasAnyFlags(DirtyTextures, EStormTextureDirtyFlags::ProfileAnvil))
			AppendStormMaterialTexture(EStormMaterialTextureSlot::AnvilProfile, RenderData.AnvilProfileTexture, OutPayload);
		if (EnumHasAnyFlags(DirtyTextures, EStormTextureDirtyFlags::FlowLower))
			AppendStormMaterialTexture(EStormMaterialTextureSlot::FlowLower, RenderData.FlowMap.LowerTexture, OutPayload);
		if (EnumHasAnyFlags(DirtyTextures, EStormTextureDirtyFlags::FlowMiddle))
			AppendStormMaterialTexture(EStormMaterialTextureSlot::FlowMiddle, RenderData.FlowMap.MiddleTexture, OutPayload);
		if (EnumHasAnyFlags(DirtyTextures, EStormTextureDirtyFlags::FlowUpper))
			AppendStormMaterialTexture(EStormMaterialTextureSlot::FlowUpper, RenderData.FlowMap.UpperTexture, OutPayload);
	}

	void BuildLightningStormMaterialPayload(
		const FStormLightningMaterialState& State,
		FStormMaterialPayload& OutPayload)
	{
		using namespace CloudMaterialParams;
		OutPayload.Reset();
		AddVector(OutPayload, LightningCenter, FLinearColor(
			State.CenterN.X,
			State.CenterN.Y,
			State.CenterN.Z,
			0.0f));
		AddVector(OutPayload, LightningFlashExtent, FLinearColor(
			State.FlashExtentN.X,
			State.FlashExtentN.Y,
			State.FlashExtentN.Z,
			0.0f));
		AddVector(OutPayload, LightningHaloExtent, FLinearColor(
			State.HaloExtentN.X,
			State.HaloExtentN.Y,
			State.HaloExtentN.Z,
			0.0f));
		AddVector(OutPayload, LightningHotWhiteColor, State.HotWhiteColor);
		AddVector(OutPayload, LightningFillColor, State.FillColor);
		AddVector(OutPayload, LightningLeakColor, State.LeakColor);
		AddScalar(OutPayload, LightningCorePeak, FMath::Max(0.0f, State.CorePeakHDR));
		AddScalar(OutPayload, LightningFillIntensity, FMath::Max(0.0f, State.FillIntensity));
		AddScalar(OutPayload, LightningLeakIntensity, FMath::Max(0.0f, State.LeakIntensity));
	}

	void BuildLightningPulseStormMaterialPayload(
		float Pulse,
		FStormMaterialPayload& OutPayload)
	{
		OutPayload.Reset();
		AddScalar(OutPayload, CloudMaterialParams::LightningPulse, FMath::Max(0.0f, Pulse));
	}
}
