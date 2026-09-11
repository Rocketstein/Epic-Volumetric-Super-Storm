// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormFlowMapHistory.h
 * @brief Declares the replayable flow-map edit history.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/History/StormReplayHistory.h"
#include "Data/Flowmap/StormFlowmapParams.h"
#include "StormFlowMapHistory.generated.h"

/** Primitive pixel operation replayed against a flow-map layer. */
UENUM()
enum class EStormFlowMapOpType : uint8
{
	Stamp,
	ClearLayer,
};

/** One immutable pixel operation replayed against a flow-map layer. */
USTRUCT()
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormFlowMapOp
{
	GENERATED_BODY()

	UPROPERTY()
	EStormFlowMapOpType Type = EStormFlowMapOpType::Stamp;

	UPROPERTY()
	EStormFlowMapLayer Layer = EStormFlowMapLayer::Lower;

	UPROPERTY()
	FVector2f BrushCenterUV = FVector2f(0.5f, 0.5f);

	UPROPERTY()
	float BrushRadiusUV = 0.05f;

	UPROPERTY()
	FVector3f BrushDirectionUVW = FVector3f(1.0f, 0.0f, 0.0f);

	UPROPERTY()
	FLinearColor BrushEncodedRGBA = FLinearColor(1.0f, 0.5f, 0.5f, 0.75f);

	UPROPERTY()
	float BrushStrength = 0.75f;

	UPROPERTY()
	float BrushOpacity = 0.35f;

	UPROPERTY()
	bool bErase = false;

	UPROPERTY()
	bool bUseEncodedRGBA = false;

	int64 GetReplayCost() const
	{
		return 1;
	}
};

/** One replay-bounded chunk of a user-visible flow-map stroke. */
USTRUCT()
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormFlowMapEdit
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FStormFlowMapOp> Ops;

	int64 GetReplayCost() const
	{
		int64 ReplayCost = 0;
		for (const FStormFlowMapOp& Op : Ops)
		{
			ReplayCost += Op.GetReplayCost();
		}
		return ReplayCost;
	}
};

/** Flow-map pixel-operation payloads composed over the shared history graph. */
USTRUCT()
struct VOLUMETRICSUPERSTORMRUNTIME_API FStormFlowMapHistory
{
	GENERATED_BODY()

	UPROPERTY()
	FStormReplayHistory Graph;

	UPROPERTY()
	TMap<FGuid, FStormFlowMapEdit> Edits;

	const FStormFlowMapEdit* FindEdit(const FStormReplayHistoryNode& Node) const
	{
		return Edits.Find(Node.Edit.EditId);
	}

	FGuid Append(
		const FGuid& ParentStateId,
		const FStormFlowMapEdit& Edit,
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
			FGuid(),
			FGuid(),
			bOutCreatedBranch);
		if (!StateId.IsValid())
		{
			return FGuid();
		}
		Edits.Add(EditId, Edit);
		return StateId;
	}

	bool BuildPath(
		const FGuid& TargetStateId,
		TArray<const FStormReplayHistoryNode*>& OutPath) const
	{
		return Graph.BuildPath(TargetStateId, OutPath);
	}

	void SweepEditsToGraph()
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
	}
};
