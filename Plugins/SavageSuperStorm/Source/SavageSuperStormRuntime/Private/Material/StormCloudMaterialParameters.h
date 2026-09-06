/**
 * @file StormCloudMaterialParameters.h
 * @brief Defines the cloud material parameter contract.
 */

#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

namespace SavageSuperStorm::CloudMaterialParams
{
	inline const FName ShapeTexture(TEXT("StormShapeRT"));
	inline const FName ShapeTexture2(TEXT("StormShapeRT2"));
	inline const FName BottomProfileRT(TEXT("BottomVerticalProfileRT"));
	inline const FName TopProfileRT(TEXT("TopVerticalProfileRT"));
	inline const FName AnvilProfileRT(TEXT("AnvilVerticalProfileRT"));
	inline const FName FlowMapLower(TEXT("StormFlowMapLower"));
	inline const FName FlowMapMiddle(TEXT("StormFlowMapMiddle"));
	inline const FName FlowMapUpper(TEXT("StormFlowMapUpper"));
	inline const FName FlowMapLayerHeights(TEXT("StormFlowMapLayerHeights"));
	inline const FName FlowMapUVWStrength(TEXT("StormFlowMapUVWStrength"));
	inline const FName FlowMapEnabled(TEXT("StormFlowMapEnabled"));
	inline const FName StormCenterRadius(TEXT("StormCenterRadius"));
	inline const FName StormCenter(TEXT("StormCenter"));
	inline const FName StormExtent(TEXT("StormExtent"));
	inline const FName StormBaseCloudColor(TEXT("StormBaseCloudColor"));
	inline const FName StormReferenceCenter(TEXT("StormReferenceCenter"));
	inline const FName StormCoordinateScale(TEXT("StormCoordinateScale"));
	inline const FName UndersideVisibility(TEXT("StormUndersideVisibility"));
	inline const FName BrimEmissiveColor(TEXT("BrimEmissiveColor"));
	inline const FName LifecycleControl(TEXT("StormLifecycleControl"));
	inline const FName AnvilStrength(TEXT("AnvilStrength"));
	inline const FName AnvilDepth01(TEXT("AnvilDepth01"));
	inline const FName AnvilAnchorHeight01(TEXT("AnvilAnchorHeight01"));
	inline const FName RotationSign(TEXT("RotationSign"));

	inline const FName AnvilTwistRadians(TEXT("AnvilTwistRadians"));
	inline const FName AnvilTwistOrigin(TEXT("AnvilTwistOrigin"));
	inline const FName OuterBrimRadiusScale(TEXT("OuterBrimRadiusScale"));

	inline const FName LFNoiseWorldSize(TEXT("LFNoiseWorldSize"));
	inline const FName HFNoiseWorldSize(TEXT("HFNoiseWorldSize"));
	inline const FName CurlNoiseWorldSize(TEXT("CurlNoiseWorldSize"));
	inline const FName LFUnitsPerBodyRadius(TEXT("LFUnitsPerBodyRadius"));
	inline const FName HFUnitsPerBodyRadius(TEXT("HFUnitsPerBodyRadius"));
	inline const FName CurlUnitsPerBodyRadius(TEXT("CurlUnitsPerBodyRadius"));
	inline const FName DensityMultiplier(TEXT("DensityMultiplier"));
	inline const FName DensityGamma(TEXT("DensityGamma"));
	inline const FName HFStrength(TEXT("HFStrength"));
	inline const FName StormTimeSeconds(TEXT("StormTimeSeconds"));

	inline const FName MDRBoundaries0(TEXT("StormMDRBoundaries0"));
	inline const FName MDRBoundaries1(TEXT("StormMDRBoundaries1"));
	inline const FName MDRSpeeds0(TEXT("StormMDRSpeeds0"));
	inline const FName MDRSpeeds1(TEXT("StormMDRSpeeds1"));
	inline const FName MDRPhases0(TEXT("StormMDRPhases0"));
	inline const FName MDRPhases1(TEXT("StormMDRPhases1"));
	inline const FName MDRSkews0(TEXT("StormMDRSkews0"));
	inline const FName MDRSkews1(TEXT("StormMDRSkews1"));
	inline const FName MDRControl0(TEXT("StormMDRControl0"));
	inline const FName MDRControl1(TEXT("StormMDRControl1"));
	inline const FName MDRDebugRingInfluence(TEXT("StormMDRDebugRingInfluence"));

	inline const FName LightningPulse(TEXT("StormFlashPulse"));
	inline const FName LightningCenter(TEXT("StormFlashCenterN"));
	inline const FName LightningFlashExtent(TEXT("StormFlashExtentN"));
	inline const FName LightningHaloExtent(TEXT("StormHaloExtentMaxN"));
	inline const FName LightningHotWhiteColor(TEXT("StormHotWhiteColor"));
	inline const FName LightningFillColor(TEXT("StormFlashFillColor"));
	inline const FName LightningLeakColor(TEXT("StormFlashLeakColor"));
	inline const FName LightningCorePeak(TEXT("StormFlashCorePeakHDR"));
	inline const FName LightningFillIntensity(TEXT("StormFlashFillIntensity"));
	inline const FName LightningLeakIntensity(TEXT("StormFlashLeakIntensity"));

	inline constexpr float KilometersToCentimeters = 100000.0f;

	bool HasStormMaterialContract(const UMaterialInterface* Material);
}
