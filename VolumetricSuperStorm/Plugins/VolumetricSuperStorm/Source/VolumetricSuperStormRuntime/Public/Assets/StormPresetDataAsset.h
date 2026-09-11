// Copyright 2026 GoroGoro. All Rights Reserved.

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
	UPROPERTY(EditAnywhere, Category = "Storm|Shape")
	FStormShapeSettings ShapeSettings;

	UPROPERTY(EditAnywhere, Category = "Storm|Motion")
	FStormMotionSettings MotionSettings;

	UPROPERTY(EditAnywhere, Category = "Storm|Formation")
	float FormationDurationSeconds = 13.0f;

	UPROPERTY(EditAnywhere, Category = "Storm|Formation")
	float DissolutionDurationSeconds = 10.0f;

	UPROPERTY(EditAnywhere, Category = "Storm|Profile")
	TObjectPtr<UStormVerticalProfileAsset> VerticalProfile;

	UPROPERTY(EditAnywhere, Category = "Storm|Flow Map")
	TObjectPtr<UStormWindFlowMapDataAsset> FlowMap;

	UPROPERTY(EditAnywhere, Category = "Storm|Flow Map")
	bool bFlowMapEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Storm|Lightning")
	FStormLightningSequenceSettings LightningSequence;

	UPROPERTY(EditAnywhere, Category = "Storm|Lightning")
	float GroundStrikeProbability = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Storm|Lightning")
	float LightningRepeatDelaySeconds = 2.0f;
};

/**
 * @brief Stores a reusable complete storm preset.
 */
UCLASS(BlueprintType)
class VOLUMETRICSUPERSTORMRUNTIME_API UStormPresetDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, Category = "Storm|Preset", meta=(ShowOnlyInnerProperties))
	FStormPresetData PresetData;
};
