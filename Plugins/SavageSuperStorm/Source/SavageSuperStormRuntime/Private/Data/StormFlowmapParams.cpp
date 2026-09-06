/**
 * @file StormFlowmapParams.cpp
 * @brief Normalizes flow-map layer and playback parameters.
 */

#include "Data/StormFlowmapParams.h"

namespace SavageSuperStorm::FlowMap
{
	FVector3f SanitizeLayerHeights(const FVector3f& LayerHeights)
	{
		const float LowerHeight  = FMath::Clamp(LayerHeights.X, 0.0f, 1.0f - 2.0f * MinLayerHeightGap);
		const float MiddleHeight = FMath::Clamp(LayerHeights.Y, LowerHeight + MinLayerHeightGap, 1.0f - MinLayerHeightGap);
		const float UpperHeight  = FMath::Clamp(LayerHeights.Z, MiddleHeight + MinLayerHeightGap, 1.0f);
		return FVector3f(LowerHeight, MiddleHeight, UpperHeight);
	}

	FVector3f EvaluateLayerWeights(float HLocal, const FVector3f& LayerHeights)
	{
		const FVector3f Heights = SanitizeLayerHeights(LayerHeights);
		const float     H       = FMath::Clamp(HLocal, 0.0f, 1.0f);

		const float LowerToMiddle = FMath::Clamp((H - Heights.X) / FMath::Max(Heights.Y - Heights.X, MinLayerHeightGap), 0.0f, 1.0f);
		const float MiddleToUpper = FMath::Clamp((H - Heights.Y) / FMath::Max(Heights.Z - Heights.Y, MinLayerHeightGap), 0.0f, 1.0f);

		return H <= Heights.Y ? FVector3f(1.0f - LowerToMiddle, LowerToMiddle, 0.0f) : FVector3f(0.0f, 1.0f - MiddleToUpper, MiddleToUpper);
	}

	FVector3f GetLayerHeights(const FStormFlowmapParams& Params)
	{
		return FVector3f(Params.LowerLayerHeight, Params.MiddleLayerHeight, Params.UpperLayerHeight);
	}
}