// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormFlowMapUndoState.cpp
 * @brief Implements the transacted undo state for flow-map edits.
 */

#include "Data/Flowmap/StormFlowMapUndoState.h"

FStormReplayHistory* UStormFlowMapUndoState::FindHistoryGraph(
	const FGuid& HistoryId)
{
	return Bookmark && Bookmark->HistoryId == HistoryId
		? &History.Graph
		: nullptr;
}

FGuid UStormFlowMapUndoState::GetBookmarkStateId(
	const FGuid& HistoryId) const
{
	return Bookmark && Bookmark->HistoryId == HistoryId
		? Bookmark->CurrentStateId
		: FGuid();
}

void UStormFlowMapUndoState::OnHistoryPruned(const FGuid& HistoryId)
{
	if (Bookmark && Bookmark->HistoryId == HistoryId)
	{
		History.SweepEditsToGraph();
	}
}
