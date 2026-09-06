#include "Data/Profile/StormProfileUndoState.h"

FStormReplayHistory* UStormProfileUndoState::FindHistoryGraph(
	const FGuid& HistoryId)
{
	if (FStormProfileKeyHistory* History = KeyHistories.Find(HistoryId))
	{
		return &History->Graph;
	}
	return nullptr;
}

FGuid UStormProfileUndoState::GetBookmarkStateId(
	const FGuid& HistoryId) const
{
	const TObjectPtr<UStormProfileKeyUndoState>* State = KeyStates.Find(HistoryId);
	return State && State->Get() ? State->Get()->CurrentStateId : FGuid();
}

void UStormProfileUndoState::OnHistoryPruned(const FGuid& HistoryId)
{
	if (FStormProfileKeyHistory* History = KeyHistories.Find(HistoryId))
	{
		History->SweepPayloadsToGraph();
	}
}
