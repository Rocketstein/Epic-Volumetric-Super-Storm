// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormReplayHistory.h
 * @brief Declares the domain-neutral branching replay history.
 */

#pragma once

#include "Algo/Reverse.h"
#include "CoreMinimal.h"
#include "Data/History/StormReplayEdit.h"
#include "StormReplayHistory.generated.h"

/** One state transition in a domain-neutral branching replay history. */
USTRUCT()
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormReplayHistoryNode
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid StateId;

	UPROPERTY()
	FGuid ContentId;

	UPROPERTY()
	FGuid ParentStateId;

	UPROPERTY()
	FStormReplayEditRef Edit;

	UPROPERTY()
	int64 AbsoluteDepth = INDEX_NONE;

	UPROPERTY()
	int64 CumulativeReplayCost = INDEX_NONE;
};

/**
 * Immutable branching graph shared by replay-driven editor domains.
 *
 * Domain payloads and replay surfaces live outside this type. Consequently the
 * graph can manage identity, branching, cost, pruning, and checkpoints for both
 * profile and flow-map histories without depending on either domain.
 */
USTRUCT()
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormReplayHistory
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid OriginStateId;

	UPROPERTY()
	FGuid OriginContentId;

	UPROPERTY()
	TMap<FGuid, FStormReplayHistoryNode> Nodes;

	UPROPERTY()
	int64 OriginAbsoluteDepth = 0;

	UPROPERTY()
	int64 OriginCumulativeReplayCost = 0;

	/** Derived index; it is rebuilt after load or pruning. */
	TSet<FGuid> ParentsWithChildren;

	bool ContainsState(const FGuid& StateId) const
	{
		return StateId.IsValid() &&
			(StateId == OriginStateId || Nodes.Contains(StateId));
	}

	const FStormReplayHistoryNode* FindNode(const FGuid& StateId) const
	{
		return Nodes.Find(StateId);
	}

	FGuid ResolveContentId(const FGuid& StateId) const
	{
		if (StateId == OriginStateId)
		{
			return OriginContentId.IsValid() ? OriginContentId : OriginStateId;
		}
		if (const FStormReplayHistoryNode* Node = Nodes.Find(StateId))
		{
			return Node->ContentId.IsValid() ? Node->ContentId : Node->StateId;
		}
		return FGuid();
	}

	FGuid Append(
		const FGuid& ParentStateId,
		const FStormReplayEditRef& Edit,
		const FGuid& RequestedStateId = FGuid(),
		const FGuid& RequestedContentId = FGuid(),
		bool* bOutCreatedBranch = nullptr)
	{
		if (bOutCreatedBranch)
		{
			*bOutCreatedBranch = false;
		}
		if (!ContainsState(ParentStateId) || !Edit.IsValid())
		{
			return FGuid();
		}
		if (ParentsWithChildren.IsEmpty() && !Nodes.IsEmpty())
		{
			RebuildChildIndex();
		}

		FGuid StateId = RequestedStateId;
		if (StateId.IsValid())
		{
			if (ContainsState(StateId))
			{
				return FGuid();
			}
		}
		else
		{
			do
			{
				StateId = FGuid::NewGuid();
			}
			while (ContainsState(StateId));
		}

		FStormReplayHistoryNode& Node = Nodes.Add(StateId);
		Node.StateId = StateId;
		Node.ContentId = RequestedContentId.IsValid()
			? RequestedContentId
			: StateId;
		Node.ParentStateId = ParentStateId;
		Node.Edit = Edit;
		if (ParentStateId == OriginStateId)
		{
			Node.AbsoluteDepth = OriginAbsoluteDepth + 1;
			Node.CumulativeReplayCost =
				OriginCumulativeReplayCost + Edit.ReplayCost;
		}
		else
		{
			const FStormReplayHistoryNode* Parent = Nodes.Find(ParentStateId);
			check(Parent);
			Node.AbsoluteDepth = Parent->AbsoluteDepth + 1;
			Node.CumulativeReplayCost =
				Parent->CumulativeReplayCost + Edit.ReplayCost;
		}

		const bool bCreatedBranch = ParentsWithChildren.Contains(ParentStateId);
		ParentsWithChildren.Add(ParentStateId);
		if (bOutCreatedBranch)
		{
			*bOutCreatedBranch = bCreatedBranch;
		}
		return StateId;
	}

	bool BuildPath(
		const FGuid& TargetStateId,
		TArray<const FStormReplayHistoryNode*>& OutPath) const
	{
		OutPath.Reset();
		if (!ContainsState(TargetStateId))
		{
			return false;
		}

		FGuid Cursor = TargetStateId;
		int32 Remaining = Nodes.Num() + 1;
		while (Cursor != OriginStateId && Remaining-- > 0)
		{
			const FStormReplayHistoryNode* Node = Nodes.Find(Cursor);
			if (!Node)
			{
				OutPath.Reset();
				return false;
			}
			OutPath.Add(Node);
			Cursor = Node->ParentStateId;
		}
		if (Cursor != OriginStateId)
		{
			OutPath.Reset();
			return false;
		}

		Algo::Reverse(OutPath);
		return true;
	}

	int32 GetDepth(const FGuid& StateId) const
	{
		if (StateId == OriginStateId)
		{
			return 0;
		}
		if (const FStormReplayHistoryNode* Node = Nodes.Find(StateId))
		{
			if (Node->AbsoluteDepth >= OriginAbsoluteDepth)
			{
				return static_cast<int32>(FMath::Min<int64>(
					Node->AbsoluteDepth - OriginAbsoluteDepth,
					MAX_int32));
			}
		}
		return INDEX_NONE;
	}

	int64 GetReplayCost(const FGuid& StateId) const
	{
		if (StateId == OriginStateId)
		{
			return 0;
		}
		if (const FStormReplayHistoryNode* Node = Nodes.Find(StateId))
		{
			if (Node->CumulativeReplayCost >= OriginCumulativeReplayCost)
			{
				return Node->CumulativeReplayCost - OriginCumulativeReplayCost;
			}
		}
		return INDEX_NONE;
	}

	int64 GetAbsoluteDepth(const FGuid& StateId) const
	{
		if (StateId == OriginStateId)
		{
			return OriginAbsoluteDepth;
		}
		if (const FStormReplayHistoryNode* Node = Nodes.Find(StateId))
		{
			return Node->AbsoluteDepth;
		}
		return INDEX_NONE;
	}

	int64 GetCumulativeReplayCost(const FGuid& StateId) const
	{
		if (StateId == OriginStateId)
		{
			return OriginCumulativeReplayCost;
		}
		if (const FStormReplayHistoryNode* Node = Nodes.Find(StateId))
		{
			return Node->CumulativeReplayCost;
		}
		return INDEX_NONE;
	}

	FGuid RebaseToCheckpoint(const FGuid& HeadStateId)
	{
		if (!ContainsState(HeadStateId))
		{
			return FGuid();
		}

		const FGuid CheckpointContentId = ResolveContentId(HeadStateId);
		const int64 CheckpointAbsoluteDepth = GetAbsoluteDepth(HeadStateId);
		const int64 CheckpointReplayCost = GetCumulativeReplayCost(HeadStateId);
		if (!CheckpointContentId.IsValid() ||
			CheckpointAbsoluteDepth == INDEX_NONE ||
			CheckpointReplayCost == INDEX_NONE)
		{
			return FGuid();
		}

		FGuid NewOriginStateId;
		do
		{
			NewOriginStateId = FGuid::NewGuid();
		}
		while (NewOriginStateId == OriginStateId || Nodes.Contains(NewOriginStateId));

		Nodes.Empty();
		ParentsWithChildren.Empty();
		OriginStateId = NewOriginStateId;
		OriginContentId = CheckpointContentId;
		OriginAbsoluteDepth = CheckpointAbsoluteDepth;
		OriginCumulativeReplayCost = CheckpointReplayCost;
		return OriginStateId;
	}

	bool PruneToAncestry(
		const FGuid& TargetStateId,
		int32* OutRemovedNodes = nullptr)
	{
		if (OutRemovedNodes)
		{
			*OutRemovedNodes = 0;
		}
		if (!ContainsState(TargetStateId))
		{
			return false;
		}

		TSet<FGuid> KeptStates;
		KeptStates.Reserve(Nodes.Num() + 1);
		KeptStates.Add(OriginStateId);
		FGuid Cursor = TargetStateId;
		int32 Remaining = Nodes.Num() + 1;
		while (Cursor != OriginStateId && Remaining-- > 0)
		{
			const FStormReplayHistoryNode* Node = Nodes.Find(Cursor);
			if (!Node)
			{
				return false;
			}
			KeptStates.Add(Cursor);
			Cursor = Node->ParentStateId;
		}
		if (Cursor != OriginStateId)
		{
			return false;
		}

		int32 RemovedNodes = 0;
		for (auto It = Nodes.CreateIterator(); It; ++It)
		{
			if (!KeptStates.Contains(It.Key()))
			{
				It.RemoveCurrent();
				++RemovedNodes;
			}
		}
		RebuildChildIndex();
		if (OutRemovedNodes)
		{
			*OutRemovedNodes = RemovedNodes;
		}
		return true;
	}

	void RebuildChildIndex()
	{
		ParentsWithChildren.Reset();
		ParentsWithChildren.Reserve(Nodes.Num());
		for (const TPair<FGuid, FStormReplayHistoryNode>& Pair : Nodes)
		{
			ParentsWithChildren.Add(Pair.Value.ParentStateId);
		}
	}

	void GetReferencedEditIds(TSet<FGuid>& OutEditIds) const
	{
		OutEditIds.Reset();
		OutEditIds.Reserve(Nodes.Num());
		for (const TPair<FGuid, FStormReplayHistoryNode>& Pair : Nodes)
		{
			if (Pair.Value.Edit.EditId.IsValid())
			{
				OutEditIds.Add(Pair.Value.Edit.EditId);
			}
		}
	}
};
