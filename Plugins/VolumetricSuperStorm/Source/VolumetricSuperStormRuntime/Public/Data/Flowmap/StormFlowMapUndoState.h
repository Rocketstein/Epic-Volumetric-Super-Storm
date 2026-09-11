// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormFlowMapUndoState.h
 * @brief Declares the transacted undo state for flow-map edits.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/Flowmap/StormFlowMapHistory.h"
#include "Data/History/StormReplayHistorySession.h"
#include "StormFlowMapUndoState.generated.h"

/** Transactional playhead for the component's current flow-map document. */
UCLASS(Transient)
class VOLUMETRICSUPERSTORMRUNTIME_API UStormFlowMapUndoBookmark
	: public UStormReplayHistoryBookmark
{
	GENERATED_BODY()
};

/** Non-transactional flow payload registry coordinated by the shared session. */
UCLASS(Transient)
class VOLUMETRICSUPERSTORMRUNTIME_API UStormFlowMapUndoState
	: public UStormReplayHistorySession
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<UStormFlowMapUndoBookmark> Bookmark = nullptr;

	UPROPERTY(Transient, NonTransactional)
	FStormFlowMapHistory History;

protected:
	virtual FStormReplayHistory* FindHistoryGraph(
		const FGuid& HistoryId) override;
	virtual FGuid GetBookmarkStateId(
		const FGuid& HistoryId) const override;
	virtual void OnHistoryPruned(const FGuid& HistoryId) override;
};
