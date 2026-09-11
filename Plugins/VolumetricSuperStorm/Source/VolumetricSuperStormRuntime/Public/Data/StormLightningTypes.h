// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormLightningTypes.h
 * @brief Defines lightning cues, sequence settings, context, and material state.
 */

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "StormLightningTypes.generated.h"

/**
 * @brief Defines material parameters for one lightning flash.
 */
USTRUCT(BlueprintType)
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormLightningMaterialState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning", meta = ( DisplayName = "Flash Center (Normalized)", ToolTip = "Storm-relative normalized flash center. X/Y use storm radius; Z uses cloud-layer height."))
	FVector CenterN = FVector(0.0f, 0.0f, 0.204621f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning", meta = (DisplayName = "Flash Extent (Normalized)"))
	FVector FlashExtentN = FVector(0.15f, 0.15f, 0.15f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning", meta = (DisplayName = "Halo Extent (Normalized)"))
	FVector HaloExtentN = FVector(0.50f, 0.50f, 0.16f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning")
	FLinearColor HotWhiteColor = FLinearColor(0.931261f, 0.927993f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning")
	FLinearColor FillColor = FLinearColor(1.0f, 0.050082f, 0.137594f, 0.20f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning")
	FLinearColor LeakColor = FLinearColor(1.0f, 0.02f, 0.05f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float CorePeakHDR = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float FillIntensity = 0.006f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float LeakIntensity = 0.10f;
};

/**
 * @brief Defines a timed lightning cue.
 */
USTRUCT(BlueprintType)
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormLightningCue
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning", meta = ( DisplayName = "Time (Normalized)", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float Time01 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning", meta = ( DisplayName = "Cue ID", ToolTip = "Free-form identifier delivered through On Custom Lightning Cue."))
	FName CueId = NAME_None;
};

/**
 * @brief Defines the authored lightning timeline and repetition behavior.
 */
USTRUCT(BlueprintType)
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormLightningSequenceSettings
{
	GENERATED_BODY()

	FStormLightningSequenceSettings()
	{
		FRichCurve* Curve        = FlashPulseCurve.GetRichCurve();
		const auto  AddLinearKey = [Curve](const float Time, const float Value)
		{
			const FKeyHandle Handle = Curve->AddKey(Time, Value);
			Curve->SetKeyInterpMode(Handle, RCIM_Linear);
		};

		AddLinearKey(0.00f, 1.00f);
		AddLinearKey(0.12f, 1.00f);
		AddLinearKey(0.25f, 0.18f);
		AddLinearKey(0.42f, 0.72f);
		AddLinearKey(0.65f, 0.08f);
		AddLinearKey(1.00f, 0.00f);

		GroundStrikeTimes01.Add(0.20f);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning|Playback", meta = ( DisplayName = "Enabled", ToolTip = "Disables automatic and manual lightning playback when false."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning|Playback", meta = ( DisplayName = "Auto Play", EditCondition = "bEnabled"))
	bool bAutoPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning|Playback", meta = ( DisplayName = "Loop", EditCondition = "bEnabled"))
	bool bLoop = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning|Sequence", meta = ( DisplayName = "Flash Pulse Curve", EditCondition = "bEnabled"))
	FRuntimeFloatCurve FlashPulseCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning|Sequence", meta = ( DisplayName = "Ground Strike Times", EditCondition = "bEnabled"))
	TArray<float> GroundStrikeTimes01;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning|Sequence", meta = ( DisplayName = "Custom Cues", EditCondition = "bEnabled"))
	TArray<FStormLightningCue> CustomCues;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning|Placement", meta = ( DisplayName = "Strike Radius Min", ToolTip = "Inner edge of the ring that flashes and bolts are scattered across, as a fraction of the storm radius.", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnabled"))
	float StrikeRadiusMinN = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning|Placement", meta = ( DisplayName = "Strike Radius Max", ToolTip = "Outer edge of the ring that flashes and bolts are scattered across, as a fraction of the storm radius.", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", EditCondition = "bEnabled"))
	float StrikeRadiusMaxN = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning|Placement", meta = ( DisplayName = "Root Height Offset", ToolTip = "Raises the bolt root above the flash center, in normalized cloud-layer height. The root stays inside the storm body.", ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-1.0", UIMax = "1.0", EditCondition = "bEnabled"))
	float RootHeightOffsetN = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning|Material", meta = ( DisplayName = "Flash Material", ShowOnlyInnerProperties, EditCondition = "bEnabled"))
	FStormLightningMaterialState MaterialState;
};

/**
 * @brief Describes the evaluated state of a running lightning sequence.
 */
USTRUCT(BlueprintType)
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormLightningContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Storm|Lightning")
	int32 SequenceIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Storm|Lightning")
	int32 SequenceSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Storm|Lightning", meta = (Units = "s"))
	float SequenceTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Storm|Lightning")
	float NormalizedTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Storm|Lightning")
	float Pulse = 0.0f;
};