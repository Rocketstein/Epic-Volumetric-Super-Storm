// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormCloudMaterialParameters.cpp
 * @brief Defines the cloud material parameter contract.
 */

#include "Material/StormCloudMaterialParameters.h"

#include "Materials/MaterialInterface.h"

namespace VolumetricSuperStorm::CloudMaterialParams
{
	namespace
	{
		template <typename TInfo>
		bool ContainsParameter(const TArray<TInfo>& Infos, const FName Name)
		{
			return Infos.ContainsByPredicate(
				[Name](const TInfo& Info)
				{
					return Info.Name == Name;
				}
			);
		}
	}

	bool HasStormMaterialContract(const UMaterialInterface* Material)
	{
		if (!Material)
		{
			return false;
		}

		TArray<FMaterialParameterInfo> TextureInfos;
		TArray<FGuid>                  TextureIds;
		Material->GetAllTextureParameterInfo(TextureInfos, TextureIds);

		TArray<FMaterialParameterInfo> ScalarInfos;
		TArray<FGuid>                  ScalarIds;
		Material->GetAllScalarParameterInfo(ScalarInfos, ScalarIds);

		TArray<FMaterialParameterInfo> VectorInfos;
		TArray<FGuid>                  VectorIds;
		Material->GetAllVectorParameterInfo(VectorInfos, VectorIds);

		const FName RequiredTextures[] = { ShapeTexture, ShapeTexture2, BottomProfileRT, TopProfileRT, AnvilProfileRT, FlowMapLower, FlowMapMiddle, FlowMapUpper };
		const FName RequiredScalars[]  = { FlowMapEnabled, StormCoordinateScale, UndersideVisibility, StaticGlowIntensity, LifecycleControl, AnvilStrength, AnvilDepth01, AnvilAnchorHeight01, RotationSign, AnvilTwistRadians, AnvilTwistOrigin, OuterBrimRadiusScale, DensityMultiplier, DensityGamma, HFStrength, StormTimeSeconds, LightningPulse, LightningCorePeak, LightningFillIntensity, LightningLeakIntensity };
		const FName RequiredVectors[]  = { FlowMapLayerHeights, FlowMapUVWStrength, StormCenterRadius, StormCenter, StormExtent, StormBaseCloudColor, GlowExtent, StormReferenceCenter, BrimEmissiveColor, MDRBoundaries0, MDRBoundaries1, MDRSpeeds0, MDRSpeeds1, MDRPhases0, MDRPhases1, MDRSkews0, MDRSkews1, MDRControl0, MDRControl1, LightningCenter, LightningFlashExtent, LightningHaloExtent, LightningHotWhiteColor, LightningFillColor, LightningLeakColor };

		for (const FName Name : RequiredTextures)
		{
			if (!ContainsParameter(TextureInfos, Name))
			{
				return false;
			}
		}
		for (const FName Name : RequiredScalars)
		{
			if (!ContainsParameter(ScalarInfos, Name))
			{
				return false;
			}
		}
		for (const FName Name : RequiredVectors)
		{
			if (!ContainsParameter(VectorInfos, Name))
			{
				return false;
			}
		}

		return true;
	}
}
