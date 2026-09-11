// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormReplayHistorySession.cpp
 * @brief Implements the transacted session that owns a replay history.
 */

#include "Data/History/StormReplayHistorySession.h"

#if WITH_EDITOR
void UStormReplayHistoryBookmark::PostTransacted(
	const FTransactionObjectEvent& TransactionEvent)
{
	Super::PostTransacted(TransactionEvent);
	if (TransactionEvent.GetEventType() !=
		ETransactionObjectEventType::Finalized)
	{
		return;
	}

	if (UStormReplayHistorySession* Session =
			GetTypedOuter<UStormReplayHistorySession>())
	{
		Session->FinalizePendingHistoryPrune(HistoryId, CurrentStateId);
	}
}
#endif

void UStormReplayHistorySession::BeginStandaloneEdit(
	UStormReplayHistoryBookmark& Bookmark)
{
	Bookmark.Modify();
}

bool UStormReplayHistorySession::BeginStroke(
	UStormReplayHistoryBookmark& Bookmark)
{
	if (OpenStrokeHistoryId.IsValid() || !Bookmark.HistoryId.IsValid())
	{
		return false;
	}
	Bookmark.Modify();
	OpenStrokeHistoryId = Bookmark.HistoryId;
	return true;
}

bool UStormReplayHistorySession::IsStrokeOpen(const FGuid& HistoryId) const
{
	return HistoryId.IsValid() && OpenStrokeHistoryId == HistoryId;
}

void UStormReplayHistorySession::EndStroke(const FGuid& HistoryId)
{
	if (OpenStrokeHistoryId == HistoryId)
	{
		OpenStrokeHistoryId.Invalidate();
	}
}

void UStormReplayHistorySession::ResetStroke()
{
	OpenStrokeHistoryId.Invalidate();
}

void UStormReplayHistorySession::MarkBookmarkRepaired(const FGuid& HistoryId)
{
	if (!HistoryId.IsValid())
	{
		return;
	}
	RepairedBookmarks.Add(HistoryId);
	PendingPruneHeads.Remove(HistoryId);
}

void UStormReplayHistorySession::TrackAppendedState(
	const FGuid& HistoryId,
	const FGuid& NewStateId,
	bool bCreatedBranch)
{
	if (!HistoryId.IsValid() || !NewStateId.IsValid())
	{
		return;
	}

	if (RepairedBookmarks.Remove(HistoryId) > 0)
	{
		PendingPruneHeads.Remove(HistoryId);
	}
	else if (bCreatedBranch || PendingPruneHeads.Contains(HistoryId))
	{
		PendingPruneHeads.Add(HistoryId, NewStateId);
	}
}

void UStormReplayHistorySession::FinalizePendingHistoryPrune(
	const FGuid& HistoryId,
	const FGuid& FinalizedStateId)
{
	const FGuid* PendingHead = PendingPruneHeads.Find(HistoryId);
	if (!PendingHead)
	{
		return;
	}

	const FGuid ExpectedHead = *PendingHead;
	PendingPruneHeads.Remove(HistoryId);
	if (ExpectedHead != FinalizedStateId ||
		GetBookmarkStateId(HistoryId) != ExpectedHead)
	{
		return;
	}

	FStormReplayHistory* History = FindHistoryGraph(HistoryId);
	if (!History || !History->PruneToAncestry(ExpectedHead))
	{
		return;
	}
	OnHistoryPruned(HistoryId);
}

void UStormReplayHistorySession::ResetBranchTracking()
{
	PendingPruneHeads.Empty();
	RepairedBookmarks.Empty();
}
