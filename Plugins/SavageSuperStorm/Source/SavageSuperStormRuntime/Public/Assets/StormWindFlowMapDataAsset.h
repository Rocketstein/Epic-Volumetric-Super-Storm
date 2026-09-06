/**
 * @file StormWindFlowMapDataAsset.h
 * @brief Declares persistent three-layer storm flow maps.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/StormFlowmapParams.h"
#include "Engine/DataAsset.h"
#include "StormWindFlowMapDataAsset.generated.h"

class UTexture2D;

/**
 * @brief Stores persistent lower, middle, and upper flow-map textures.
 */
UCLASS(BlueprintType)
class SAVAGESUPERSTORMRUNTIME_API UStormWindFlowMapDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map|Layers")
	TObjectPtr<UTexture2D> BottomFlowMap = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map|Layers")
	TObjectPtr<UTexture2D> MiddleFlowMap = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map|Layers")
	TObjectPtr<UTexture2D> TopFlowMap = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Storm|Flow Map", meta = (ShowOnlyInnerProperties))
	FStormFlowmapParams Params;
};
