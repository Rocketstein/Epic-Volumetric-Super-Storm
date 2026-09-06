/**
 * @file StormPresetDataAsset.h
 * @brief Declares reusable storm preset assets.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/StormLightningTypes.h"
#include "Data/StormRenderData.h"
#include "Data/StormTypes.h"
#include "Engine/DataAsset.h"
#include "StormPresetDataAsset.generated.h"

class UStormVerticalProfileAsset;
class UStormWindFlowMapDataAsset;

/**
 * @brief Stores serializable storm configuration used by preset assets.
 */
USTRUCT(BlueprintType)
struct FStormPresetData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FStormShapeSettings ShapeSettings;

	UPROPERTY(EditAnywhere)
	FStormMotionSettings MotionSettings;

	UPROPERTY(EditAnywhere)
	float FormationDurationSeconds = 13.0f;

	UPROPERTY(EditAnywhere)
	float DissolutionDurationSeconds = 10.0f;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UStormVerticalProfileAsset> VerticalProfile;

	UPROPERTY(EditAnywhere)
	TObjectPtr<UStormWindFlowMapDataAsset> FlowMap;

	UPROPERTY(EditAnywhere)
	bool bFlowMapEnabled = true;

	UPROPERTY(EditAnywhere)
	FStormLightningSequenceSettings LightningSequence;

	UPROPERTY(EditAnywhere)
	float GroundStrikeProbability = 1.0f;

	UPROPERTY(EditAnywhere)
	float LightningRepeatDelaySeconds = 2.0f;
};

/**
 * @brief Stores a reusable complete storm preset.
 */
UCLASS(BlueprintType)
class SAVAGESUPERSTORMRUNTIME_API UStormPresetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties))
	FStormPresetData PresetData;
};
