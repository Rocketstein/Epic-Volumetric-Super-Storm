// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormReplayHistorySession.h
 * @brief Declares the transacted session that owns a replay history.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/History/StormReplayHistory.h"
#include "UObject/Object.h"
#include "StormReplayHistorySession.generated.h"

/** Transactional bookmark for one subject in an immutable replay graph. */
UCLASS(Transient)
class VOLUMETRICSUPERSTORMRUNTIME_API UStormReplayHistoryBookmark : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient, NonTransactional)
	FGuid HistoryId;

	UPROPERTY()
	FGuid CurrentStateId;

#if WITH_EDITOR
	virtual void PostTransacted(
		const FTransactionObjectEvent& TransactionEvent) override;
#endif
};

/**
 * Shared transaction/branch coordinator for domain-owned replay histories.
 *
 * Derived sessions provide graph and bookmark lookup. This class owns the subtle
 * transaction-finalization rules, so profile and flow-map domains do not duplicate
 * redo-branch lifetime management.
 */
UCLASS(Abstract, Transient)
class VOLUMETRICSUPERSTORMRUNTIME_API UStormReplayHistorySession : public UObject
{
	GENERATED_BODY()

public:
	/** Snapshots a bookmark for one non-gesture edit. */
	void BeginStandaloneEdit(UStormReplayHistoryBookmark& Bookmark);
	/** Opens one gesture and snapshots its bookmark exactly once. */
	bool BeginStroke(UStormReplayHistoryBookmark& Bookmark);
	bool IsStrokeOpen(const FGuid& HistoryId) const;
	void EndStroke(const FGuid& HistoryId);
	void ResetStroke();

	void MarkBookmarkRepaired(const FGuid& HistoryId);
	void TrackAppendedState(
		const FGuid& HistoryId,
		const FGuid& NewStateId,
		bool bCreatedBranch);
	void FinalizePendingHistoryPrune(
		const FGuid& HistoryId,
		const FGuid& FinalizedStateId);
	void ResetBranchTracking();

protected:
	virtual FStormReplayHistory* FindHistoryGraph(const FGuid& HistoryId)
		PURE_VIRTUAL(UStormReplayHistorySession::FindHistoryGraph, return nullptr;);
	virtual FGuid GetBookmarkStateId(const FGuid& HistoryId) const
		PURE_VIRTUAL(UStormReplayHistorySession::GetBookmarkStateId, return FGuid(););
	virtual void OnHistoryPruned(const FGuid& HistoryId) {}

private:
	FGuid OpenStrokeHistoryId;
	TMap<FGuid, FGuid> PendingPruneHeads;
	TSet<FGuid> RepairedBookmarks;
};
