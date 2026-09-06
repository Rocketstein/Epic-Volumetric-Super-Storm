#pragma once

#include "CoreMinimal.h"
#include "StormReplayEdit.generated.h"

/**
 * Domain-neutral identity and cost for one immutable replay edit.
 *
 * The history graph deliberately stores only this descriptor. The owning domain
 * keeps the strongly typed edit payload in a map addressed by EditId.
 */
USTRUCT()
struct SAVAGESUPERSTORMRUNTIME_API FStormReplayEditRef
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid EditId;

	UPROPERTY()
	int64 ReplayCost = 0;

	bool IsValid() const
	{
		return EditId.IsValid() && ReplayCost >= 0;
	}
};
