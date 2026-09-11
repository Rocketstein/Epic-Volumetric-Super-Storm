// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormProfileParams.h
 * @brief Defines vertical profile brush, layer, and render parameters.
 */

#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "StormProfileParams.generated.h"

/** @brief Defines procedural vertical profile generation parameters. */
USTRUCT(BlueprintType)
struct FStormProfileParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Profile", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BottomFade = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Profile", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TopFade = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Profile|Anvil")
	FRuntimeFloatCurve AnvilProfileCurve;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Profile|Top")
	FRuntimeFloatCurve VerticalProfileCurve;
};
