// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormProfileUndoState.h
 * @brief Declares the transacted undo state for vertical profile edits.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/History/StormReplayHistorySession.h"
#include "Data/Profile/StormProfileHistory.h"
#include "StormProfileUndoState.generated.h"

/** Profile-compatible bookmark type over the shared transactional bookmark. */
UCLASS(Transient)
class VOLUMETRICSUPERSTORMRUNTIME_API UStormProfileKeyUndoState
	: public UStormReplayHistoryBookmark
{
	GENERATED_BODY()
};

/**
 * Non-transactional registry of immutable histories and transactional bookmarks.
 *
 * Adding history nodes or payloads never enters Unreal's transaction snapshots.
 * A stroke calls Modify only on its UStormProfileKeyUndoState, so the transaction
 * stores one current-state GUID rather than the growing command graph.
 */
UCLASS(Transient)
class VOLUMETRICSUPERSTORMRUNTIME_API UStormProfileUndoState
	: public UStormReplayHistorySession
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient, NonTransactional)
	TMap<FGuid, TObjectPtr<UStormProfileKeyUndoState>> KeyStates;

	UPROPERTY(Transient, NonTransactional)
	TMap<FGuid, FStormProfileKeyHistory> KeyHistories;

protected:
	virtual FStormReplayHistory* FindHistoryGraph(
		const FGuid& HistoryId) override;
	virtual FGuid GetBookmarkStateId(
		const FGuid& HistoryId) const override;
	virtual void OnHistoryPruned(const FGuid& HistoryId) override;
};
