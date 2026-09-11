// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormProfileHistory.h
 * @brief Declares the replayable vertical profile edit history.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/History/StormReplayHistory.h"
#include "Data/Profile/StormProfileParams.h"
#include "StormProfileHistory.generated.h"

class UTexture2D;

/** The kind of primitive replay operation contained by a profile edit. */
UENUM()
enum class EStormProfileOpType : uint8
{
	/** A single brush dab, replayed through the profile brush compute pass. */
	Stamp,
	/** Rebuilds Top and Anvil from a params snapshot, discarding brushwork. */
	ReseedFromParams,
	/** Blits a baked texture over the paintable surfaces. */
	LoadFromTexture,
	/**
	 * Rebuilds only the targeted paintable surface from a params snapshot.
	 * No longer recorded: Clear Canvas superseded it. Replay still honours it so
	 * histories saved before that change reproduce their surfaces exactly.
	 */
	ReseedSurfaceFromParams,
	/** Overwrites the targeted surface with the uniform level held in Op.Value. */
	FillSurface,
};

/** One primitive replay operation on a key's paintable surfaces. */
USTRUCT()
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormProfileOp
{
	GENERATED_BODY()

	UPROPERTY()
	EStormProfileOpType Type = EStormProfileOpType::Stamp;

	/** Surface target for single-surface operations: Anvil when true, otherwise Top. */
	UPROPERTY()
	bool bTargetAnvil = false;

	UPROPERTY()
	FVector2f BrushUV = FVector2f::ZeroVector;

	UPROPERTY()
	float BrushRadiusUV = 0.f;

	UPROPERTY()
	float Strength = 0.f;

	UPROPERTY()
	float Value = 0.f;

	UPROPERTY()
	bool bErase = false;

	UPROPERTY()
	bool bOverwrite = false;

	/**
	 * Immutable payload identity for a reseed or LoadFromTexture.
	 * Stamps do not use a payload.
	 */
	UPROPERTY()
	FGuid PayloadId;

	/** Estimated render-pass submissions required to replay this operation. */
	int64 GetReplayCost() const
	{
		// An aggregate reseed rebuilds both paintable surfaces. Every other
		// operation submits one pass against one surface.
		return Type == EStormProfileOpType::ReseedFromParams ? 2 : 1;
	}
};

/** The user-visible action represented by one history graph edge. */
UENUM()
enum class EStormProfileEditType : uint8
{
	/** One replay-bounded chunk of a paint gesture containing one or more dabs. */
	Stroke,
	/** One absolute parameter snapshot and surface reseed. */
	ReseedFromParams,
	/** One composite restoration of params, Top, and Anvil from the asset. */
	RestoreFromAsset,
	/** One restoration of an unsaved scratch profile to its parametric origin. */
	RestoreOrigin,
	/** One wipe of the selected paintable surface to an empty canvas. */
	ClearCanvas,
};

/**
 * One immutable profile replay edit.
 *
 * A graph node stores one edit rather than one primitive replay operation. An
 * edit normally matches one Unreal undo step. The exception is an exceptionally
 * long stroke, which may span several replay-bounded edits while its bookmark is
 * still transacted only once. An asset restore owns its reseed and two blits.
 */
USTRUCT()
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormProfileEdit
{
	GENERATED_BODY()

	UPROPERTY()
	EStormProfileEditType Type = EStormProfileEditType::Stroke;

	/** Primitive operations replayed in order to apply this action. */
	UPROPERTY()
	TArray<FStormProfileOp> Ops;

	int64 GetReplayCost() const
	{
		int64 ReplayCost = 0;
		for (const FStormProfileOp& Op : Ops)
		{
			ReplayCost += Op.GetReplayCost();
		}
		return ReplayCost;
	}
};

/**
 * Profile-specific payloads composed over the shared replay-history graph.
 *
 * The graph owns identities, branching, cost, and checkpoint counters. This
 * wrapper owns only data required to interpret profile edit ids.
 */
USTRUCT()
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormProfileKeyHistory
{
	GENERATED_BODY()

	UPROPERTY()
	FStormReplayHistory Graph;

	/** Params in effect at the root, before any reseed node applies. */
	UPROPERTY()
	FStormProfileParams OriginParams;

	/** Immutable typed edits addressed by the opaque ids stored in Graph. */
	UPROPERTY()
	TMap<FGuid, FStormProfileEdit> Edits;

	/** Immutable heavyweight payloads, addressed by operation payload id. */
	UPROPERTY()
	TMap<FGuid, FStormProfileParams> ParamsPayloads;

	/** UPROPERTY storage keeps texture payloads alive for every reachable branch. */
	UPROPERTY()
	TMap<FGuid, TObjectPtr<UTexture2D>> TexturePayloads;

	bool ContainsState(const FGuid& StateId) const
	{
		return Graph.ContainsState(StateId);
	}

	const FStormReplayHistoryNode* FindNode(const FGuid& StateId) const
	{
		return Graph.FindNode(StateId);
	}

	const FStormProfileEdit* FindEdit(const FStormReplayHistoryNode& Node) const
	{
		return Edits.Find(Node.Edit.EditId);
	}

	FGuid ResolveContentId(const FGuid& StateId) const
	{
		return Graph.ResolveContentId(StateId);
	}

	/** Adds a new immutable child and returns its state identity. */
	FGuid Append(
		const FGuid& ParentStateId,
		const FStormProfileEdit& Edit,
		const FGuid& RequestedStateId = FGuid(),
		const FGuid& RequestedContentId = FGuid(),
		bool* bOutCreatedBranch = nullptr)
	{
		if (Edit.Ops.IsEmpty())
		{
			return FGuid();
		}

		FGuid EditId;
		do
		{
			EditId = FGuid::NewGuid();
		}
		while (Edits.Contains(EditId));

		FStormReplayEditRef EditRef;
		EditRef.EditId = EditId;
		EditRef.ReplayCost = Edit.GetReplayCost();
		const FGuid StateId = Graph.Append(
			ParentStateId,
			EditRef,
			RequestedStateId,
			RequestedContentId,
			bOutCreatedBranch);
		if (!StateId.IsValid())
		{
			return FGuid();
		}
		Edits.Add(EditId, Edit);
		return StateId;
	}

	FGuid AddParamsPayload(const FStormProfileParams& Params)
	{
		FGuid PayloadId;
		do
		{
			PayloadId = FGuid::NewGuid();
		}
		while (ParamsPayloads.Contains(PayloadId) ||
			TexturePayloads.Contains(PayloadId));
		ParamsPayloads.Add(PayloadId, Params);
		return PayloadId;
	}

	FGuid AddTexturePayload(UTexture2D* Texture)
	{
		FGuid PayloadId;
		do
		{
			PayloadId = FGuid::NewGuid();
		}
		while (ParamsPayloads.Contains(PayloadId) ||
			TexturePayloads.Contains(PayloadId));
		TexturePayloads.Add(PayloadId, Texture);
		return PayloadId;
	}

	/**
	 * Builds the root-to-target node path. The root itself produces an empty path.
	 * Missing parents or a cycle fail rather than replaying an incomplete history.
	 */
	bool BuildPath(
		const FGuid& TargetStateId,
		TArray<const FStormReplayHistoryNode*>& OutPath) const
	{
		return Graph.BuildPath(TargetStateId, OutPath);
	}

	/** Distance in edit nodes from the current origin to StateId. */
	int32 GetDepth(const FGuid& StateId) const
	{
		return Graph.GetDepth(StateId);
	}

	/** Estimated edit replay passes from the current origin to StateId. */
	int64 GetReplayCost(const FGuid& StateId) const
	{
		return Graph.GetReplayCost(StateId);
	}

	/** Absolute edit position retained across checkpoint rebases. */
	int64 GetAbsoluteDepth(const FGuid& StateId) const
	{
		return Graph.GetAbsoluteDepth(StateId);
	}

	/** Absolute replay-cost position retained across checkpoint rebases. */
	int64 GetCumulativeReplayCost(const FGuid& StateId) const
	{
		return Graph.GetCumulativeReplayCost(StateId);
	}

	/**
	 * Makes HeadStateId the content represented by a fresh empty origin. The caller
	 * must first preserve the head's pixels as the new external checkpoint surfaces.
	 */
	FGuid RebaseToCheckpoint(const FGuid& HeadStateId)
	{
		if (!ContainsState(HeadStateId))
		{
			return FGuid();
		}

		const FStormProfileParams CheckpointParams = ResolveParamsAt(HeadStateId);
		const FGuid NewOriginStateId = Graph.RebaseToCheckpoint(HeadStateId);
		if (!NewOriginStateId.IsValid())
		{
			return FGuid();
		}
		Edits.Empty();
		ParamsPayloads.Empty();
		TexturePayloads.Empty();
		OriginParams = CheckpointParams;
		return NewOriginStateId;
	}

	/**
	 * Keeps only TargetStateId and its ancestry. This is intentionally a parent
	 * walk plus a full sweep: histories are deep and narrow, and no child lists
	 * are required for the reachability question.
	 */
	bool PruneToAncestry(
		const FGuid& TargetStateId,
		int32* OutRemovedNodes = nullptr)
	{
		if (!Graph.PruneToAncestry(TargetStateId, OutRemovedNodes))
		{
			return false;
		}
		SweepPayloadsToGraph();
		return true;
	}

	/** Removes typed edits and heavyweight payloads no longer named by Graph. */
	void SweepPayloadsToGraph()
	{
		TSet<FGuid> ReferencedEditIds;
		Graph.GetReferencedEditIds(ReferencedEditIds);
		for (auto It = Edits.CreateIterator(); It; ++It)
		{
			if (!ReferencedEditIds.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
		}

		/**
		 * Sweep payload maps from the surviving edits. This remains correct if a
		 * future edit deliberately shares a payload with another node.
		 */
		TSet<FGuid> ReferencedParamsPayloads;
		TSet<FGuid> ReferencedTexturePayloads;
		for (const TPair<FGuid, FStormProfileEdit>& Pair : Edits)
		{
			for (const FStormProfileOp& Op : Pair.Value.Ops)
			{
				switch (Op.Type)
				{
				case EStormProfileOpType::ReseedFromParams:
				case EStormProfileOpType::ReseedSurfaceFromParams:
					ReferencedParamsPayloads.Add(Op.PayloadId);
					break;
				case EStormProfileOpType::LoadFromTexture:
					ReferencedTexturePayloads.Add(Op.PayloadId);
					break;
				default:
					break;
				}
			}
		}
		for (auto It = ParamsPayloads.CreateIterator(); It; ++It)
		{
			if (!ReferencedParamsPayloads.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
		}
		for (auto It = TexturePayloads.CreateIterator(); It; ++It)
		{
			if (!ReferencedTexturePayloads.Contains(It.Key()))
			{
				It.RemoveCurrent();
			}
		}
	}

	/** Returns the params in effect at StateId. */
	const FStormProfileParams& ResolveParamsAt(const FGuid& StateId) const
	{
		FGuid Cursor = StateId;
		int32 Remaining = Graph.Nodes.Num() + 1;
		while (Cursor != Graph.OriginStateId && Remaining-- > 0)
		{
			const FStormReplayHistoryNode* Node = Graph.Nodes.Find(Cursor);
			if (!Node)
			{
				break;
			}
			const FStormProfileEdit* Edit = FindEdit(*Node);
			if (!Edit)
			{
				break;
			}
			for (int32 OpIndex = Edit->Ops.Num() - 1;
				OpIndex >= 0;
				--OpIndex)
			{
				const FStormProfileOp& Op = Edit->Ops[OpIndex];
				if (Op.Type != EStormProfileOpType::ReseedFromParams)
				{
					continue;
				}
				if (const FStormProfileParams* Params =
						ParamsPayloads.Find(Op.PayloadId))
				{
					return *Params;
				}
			}
			Cursor = Node->ParentStateId;
		}
		return OriginParams;
	}

	/** True when either paintable surface has detail above its latest reset. */
	bool HasPaintAfterLastReseed(const FGuid& StateId) const
	{
		return HasPaintAfterLastReseed(StateId, false) ||
			HasPaintAfterLastReseed(StateId, true);
	}

	/** True when the selected surface has detail above its latest applicable reset. */
	bool HasPaintAfterLastReseed(
		const FGuid& StateId,
		bool bTargetAnvil) const
	{
		FGuid Cursor = StateId;
		int32 Remaining = Graph.Nodes.Num() + 1;
		while (Cursor != Graph.OriginStateId && Remaining-- > 0)
		{
			const FStormReplayHistoryNode* Node = Graph.Nodes.Find(Cursor);
			if (!Node)
			{
				return false;
			}
			const FStormProfileEdit* Edit = FindEdit(*Node);
			if (!Edit)
			{
				return false;
			}
			for (int32 OpIndex = Edit->Ops.Num() - 1;
				OpIndex >= 0;
				--OpIndex)
			{
				const FStormProfileOp& Op = Edit->Ops[OpIndex];
				switch (Op.Type)
				{
				case EStormProfileOpType::Stamp:
				case EStormProfileOpType::LoadFromTexture:
				// A wiped surface counts as detail: it is a deliberate departure
				// from the parametric macro, so entering Parameterize mode would
				// discard it just as it would discard brushwork.
				case EStormProfileOpType::FillSurface:
					if (Op.bTargetAnvil == bTargetAnvil)
					{
						return true;
					}
					break;
				case EStormProfileOpType::ReseedSurfaceFromParams:
					if (Op.bTargetAnvil == bTargetAnvil)
					{
						return false;
					}
					break;
				case EStormProfileOpType::ReseedFromParams:
					return false;
				default:
					break;
				}
			}
			Cursor = Node->ParentStateId;
		}
		return false;
	}

	/**
	 * True when the selected surface holds content a wipe would remove.
	 *
	 * Deliberately not the inverse of HasPaintAfterLastReseed. An emptied surface
	 * reads as painted there, because it deviates from the parametric macro; it
	 * reads as empty here, so the button that emptied it goes quiet instead of
	 * appending fills that change nothing. A surface still holding its parametric
	 * seed reads as clearable: wiping a pristine profile is a real edit.
	 */
	bool HasClearableContent(
		const FGuid& StateId,
		bool bTargetAnvil) const
	{
		FGuid Cursor = StateId;
		int32 Remaining = Graph.Nodes.Num() + 1;
		while (Cursor != Graph.OriginStateId && Remaining-- > 0)
		{
			const FStormReplayHistoryNode* Node = Graph.Nodes.Find(Cursor);
			if (!Node)
			{
				break;
			}
			const FStormProfileEdit* Edit = FindEdit(*Node);
			if (!Edit)
			{
				break;
			}
			// The most recent op touching this surface decides the answer on its
			// own: whatever came before it was overwritten.
			for (int32 OpIndex = Edit->Ops.Num() - 1;
				OpIndex >= 0;
				--OpIndex)
			{
				const FStormProfileOp& Op = Edit->Ops[OpIndex];
				switch (Op.Type)
				{
				case EStormProfileOpType::FillSurface:
					if (Op.bTargetAnvil == bTargetAnvil)
					{
						// Only an empty fill leaves nothing behind to clear.
						return Op.Value > 0.f;
					}
					break;
				case EStormProfileOpType::Stamp:
				case EStormProfileOpType::LoadFromTexture:
				case EStormProfileOpType::ReseedSurfaceFromParams:
					if (Op.bTargetAnvil == bTargetAnvil)
					{
						return true;
					}
					break;
				case EStormProfileOpType::ReseedFromParams:
					return true;
				default:
					break;
				}
			}
			Cursor = Node->ParentStateId;
		}
		// Reaching the origin, or losing the trail to a pruned node, both leave a
		// surface that holds its parametric seed. Allow the wipe: it is undoable,
		// where wrongly greying the button out is a dead end.
		return true;
	}
};
