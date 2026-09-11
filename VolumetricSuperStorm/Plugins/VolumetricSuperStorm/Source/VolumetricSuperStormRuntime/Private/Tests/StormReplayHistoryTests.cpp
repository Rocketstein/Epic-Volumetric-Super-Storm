// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormReplayHistoryTests.cpp
 * @brief Automation tests for the branching replay edit history.
 */

#if WITH_DEV_AUTOMATION_TESTS

#include "Data/History/StormReplayHistory.h"
#include "Data/Flowmap/StormFlowMapHistory.h"
#include "Data/Profile/StormProfileHistory.h"
#include "Misc/AutomationTest.h"

namespace
{
	FStormReplayEditRef MakeEditRef(int64 ReplayCost)
	{
		FStormReplayEditRef Edit;
		Edit.EditId = FGuid::NewGuid();
		Edit.ReplayCost = ReplayCost;
		return Edit;
	}

	FStormReplayHistory MakeHistory()
	{
		FStormReplayHistory History;
		History.OriginStateId = FGuid::NewGuid();
		History.OriginContentId = History.OriginStateId;
		return History;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStormReplayHistoryLinearPathTest,
	"VolumetricSuperStorm.History.LinearPath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormReplayHistoryLinearPathTest::RunTest(const FString&)
{
	FStormReplayHistory History = MakeHistory();
	const FGuid Root = History.OriginStateId;
	const FGuid First = History.Append(Root, MakeEditRef(2));
	const FGuid Second = History.Append(First, MakeEditRef(3));

	TestTrue(TEXT("First state is appended"), First.IsValid());
	TestTrue(TEXT("Second state is appended"), Second.IsValid());
	TestEqual(TEXT("Depth counts graph edges"), History.GetDepth(Second), 2);
	TestEqual(TEXT("Replay cost accumulates"), History.GetReplayCost(Second), int64(5));

	TArray<const FStormReplayHistoryNode*> Path;
	TestTrue(TEXT("Path is valid"), History.BuildPath(Second, Path));
	TestEqual(TEXT("Path contains both edits"), Path.Num(), 2);
	if (Path.Num() == 2)
	{
		TestEqual(TEXT("Path begins at first state"), Path[0]->StateId, First);
		TestEqual(TEXT("Path ends at second state"), Path[1]->StateId, Second);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStormReplayHistoryBranchPruneTest,
	"VolumetricSuperStorm.History.BranchPrune",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormReplayHistoryBranchPruneTest::RunTest(const FString&)
{
	FStormReplayHistory History = MakeHistory();
	const FGuid Root = History.OriginStateId;
	const FGuid First = History.Append(Root, MakeEditRef(1));
	const FGuid Abandoned = History.Append(First, MakeEditRef(1));

	bool bCreatedBranch = false;
	const FGuid Replacement = History.Append(
		First,
		MakeEditRef(4),
		FGuid(),
		FGuid(),
		&bCreatedBranch);
	TestTrue(TEXT("Sibling append is detected as a branch"), bCreatedBranch);
	TestTrue(TEXT("Abandoned state exists before finalization"),
		History.ContainsState(Abandoned));

	int32 RemovedNodes = 0;
	TestTrue(TEXT("Replacement ancestry can be pruned"),
		History.PruneToAncestry(Replacement, &RemovedNodes));
	TestEqual(TEXT("One abandoned node is removed"), RemovedNodes, 1);
	TestFalse(TEXT("Abandoned state is unreachable after prune"),
		History.ContainsState(Abandoned));
	TestTrue(TEXT("Replacement remains reachable"),
		History.ContainsState(Replacement));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStormReplayHistoryCheckpointTest,
	"VolumetricSuperStorm.History.Checkpoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormReplayHistoryCheckpointTest::RunTest(const FString&)
{
	FStormReplayHistory History = MakeHistory();
	const FGuid First = History.Append(History.OriginStateId, MakeEditRef(2));
	const FGuid Head = History.Append(First, MakeEditRef(3));
	const FGuid HeadContentId = History.ResolveContentId(Head);

	const FGuid NewRoot = History.RebaseToCheckpoint(Head);
	TestTrue(TEXT("Checkpoint creates a new graph root"), NewRoot.IsValid());
	TestNotEqual(TEXT("Checkpoint root has fresh state identity"), NewRoot, Head);
	TestEqual(TEXT("Checkpoint preserves content identity"),
		History.ResolveContentId(NewRoot), HeadContentId);
	TestEqual(TEXT("Checkpoint has no relative depth"), History.GetDepth(NewRoot), 0);
	TestEqual(TEXT("Checkpoint has no relative replay cost"),
		History.GetReplayCost(NewRoot), int64(0));
	TestEqual(TEXT("Checkpoint preserves absolute depth"),
		History.GetAbsoluteDepth(NewRoot), int64(2));
	TestEqual(TEXT("Checkpoint preserves cumulative cost"),
		History.GetCumulativeReplayCost(NewRoot), int64(5));
	TestTrue(TEXT("Checkpoint removes prior nodes"), History.Nodes.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStormFlowMapHistoryCheckpointSweepTest,
	"VolumetricSuperStorm.History.FlowMapCheckpointSweep",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormFlowMapHistoryCheckpointSweepTest::RunTest(const FString&)
{
	FStormFlowMapHistory History;
	History.Graph = MakeHistory();

	FStormFlowMapEdit FirstEdit;
	FirstEdit.Ops.AddDefaulted();
	const FGuid First = History.Append(
		History.Graph.OriginStateId,
		FirstEdit);

	FStormFlowMapEdit SecondEdit;
	SecondEdit.Ops.AddDefaulted();
	const FGuid Head = History.Append(First, SecondEdit);
	const FGuid HeadContentId = History.Graph.ResolveContentId(Head);
	TestEqual(TEXT("Flow-map edits exist before checkpoint"), History.Edits.Num(), 2);

	const FGuid NewRoot = History.Graph.RebaseToCheckpoint(Head);
	History.SweepEditsToGraph();
	TestTrue(TEXT("Flow-map checkpoint creates a new root"), NewRoot.IsValid());
	TestEqual(
		TEXT("Flow-map checkpoint preserves content identity"),
		History.Graph.ResolveContentId(NewRoot),
		HeadContentId);
	TestTrue(TEXT("Checkpoint sweeps all replayed flow-map edits"), History.Edits.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStormProfileHistoryPayloadPruneTest,
	"VolumetricSuperStorm.History.ProfilePayloadPrune",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormProfileHistoryPayloadPruneTest::RunTest(const FString&)
{
	FStormProfileKeyHistory History;
	History.Graph.OriginStateId = FGuid::NewGuid();
	History.Graph.OriginContentId = History.Graph.OriginStateId;

	FStormProfileEdit FirstEdit;
	FirstEdit.Ops.AddDefaulted();
	const FGuid First = History.Append(
		History.Graph.OriginStateId,
		FirstEdit);

	FStormProfileEdit AbandonedEdit;
	FStormProfileOp& Reseed = AbandonedEdit.Ops.AddDefaulted_GetRef();
	Reseed.Type = EStormProfileOpType::ReseedFromParams;
	Reseed.PayloadId = History.AddParamsPayload(FStormProfileParams());
	const FGuid Abandoned = History.Append(First, AbandonedEdit);

	FStormProfileEdit ReplacementEdit;
	ReplacementEdit.Ops.AddDefaulted();
	const FGuid Replacement = History.Append(First, ReplacementEdit);
	TestTrue(TEXT("Profile branch states are created"),
		Abandoned.IsValid() && Replacement.IsValid());
	TestEqual(TEXT("Profile payload exists before prune"),
		History.ParamsPayloads.Num(), 1);

	TestTrue(TEXT("Profile history prunes through the shared graph"),
		History.PruneToAncestry(Replacement));
	TestFalse(TEXT("Abandoned profile state is removed"),
		History.ContainsState(Abandoned));
	TestEqual(TEXT("Only surviving typed edits remain"), History.Edits.Num(), 2);
	TestTrue(TEXT("Unreferenced profile payload is swept"),
		History.ParamsPayloads.IsEmpty());
	return true;
}

// Covers the legacy op that Clear Canvas superseded. Nothing records it any more,
// but histories saved before that change still replay through it, so its
// reset-boundary semantics have to keep working.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStormProfileHistoryTargetedReseedTest,
	"VolumetricSuperStorm.History.ProfileTargetedReseed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormProfileHistoryTargetedReseedTest::RunTest(const FString&)
{
	FStormProfileKeyHistory History;
	History.Graph.OriginStateId = FGuid::NewGuid();
	History.Graph.OriginContentId = History.Graph.OriginStateId;

	FStormProfileEdit PaintEdit;
	FStormProfileOp& TopStamp = PaintEdit.Ops.AddDefaulted_GetRef();
	TopStamp.Type = EStormProfileOpType::Stamp;
	TopStamp.bTargetAnvil = false;
	FStormProfileOp& AnvilStamp = PaintEdit.Ops.AddDefaulted_GetRef();
	AnvilStamp.Type = EStormProfileOpType::Stamp;
	AnvilStamp.bTargetAnvil = true;
	const FGuid Painted = History.Append(
		History.Graph.OriginStateId,
		PaintEdit);
	TestTrue(TEXT("Top is painted before its targeted reseed"),
		History.HasPaintAfterLastReseed(Painted, false));
	TestTrue(TEXT("Anvil is painted before its targeted reseed"),
		History.HasPaintAfterLastReseed(Painted, true));

	FStormProfileEdit ReseedTopEdit;
	FStormProfileOp& ReseedTop = ReseedTopEdit.Ops.AddDefaulted_GetRef();
	ReseedTop.Type = EStormProfileOpType::ReseedSurfaceFromParams;
	ReseedTop.bTargetAnvil = false;
	ReseedTop.PayloadId = History.AddParamsPayload(FStormProfileParams());
	const FGuid TopReseeded = History.Append(Painted, ReseedTopEdit);

	TestFalse(TEXT("Top reseed removes only Top paint state"),
		History.HasPaintAfterLastReseed(TopReseeded, false));
	TestTrue(TEXT("Top reseed preserves Anvil paint state"),
		History.HasPaintAfterLastReseed(TopReseeded, true));
	TestTrue(TEXT("Aggregate paint state remains while Anvil is painted"),
		History.HasPaintAfterLastReseed(TopReseeded));
	TestEqual(TEXT("Targeted reseed costs one render pass"),
		ReseedTop.GetReplayCost(), int64(1));

	FStormProfileEdit ReseedAnvilEdit;
	FStormProfileOp& ReseedAnvil = ReseedAnvilEdit.Ops.AddDefaulted_GetRef();
	ReseedAnvil.Type = EStormProfileOpType::ReseedSurfaceFromParams;
	ReseedAnvil.bTargetAnvil = true;
	ReseedAnvil.PayloadId = History.AddParamsPayload(FStormProfileParams());
	const FGuid BothReseeded = History.Append(TopReseeded, ReseedAnvilEdit);
	TestFalse(TEXT("Reseeding both surfaces removes aggregate paint state"),
		History.HasPaintAfterLastReseed(BothReseeded));

	// A reseeded surface is back to its parametric macro, which is content a wipe
	// would still remove.
	TestTrue(TEXT("A reseeded Top surface is clearable"),
		History.HasClearableContent(BothReseeded, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FStormProfileHistoryClearCanvasTest,
	"VolumetricSuperStorm.History.ProfileClearCanvas",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FStormProfileHistoryClearCanvasTest::RunTest(const FString&)
{
	FStormProfileKeyHistory History;
	History.Graph.OriginStateId = FGuid::NewGuid();
	History.Graph.OriginContentId = History.Graph.OriginStateId;

	// A pristine profile still holds its parametric seed, so it can be wiped even
	// though nothing has been painted on it.
	TestTrue(TEXT("An untouched Top surface is clearable"),
		History.HasClearableContent(History.Graph.OriginStateId, false));
	TestFalse(TEXT("An untouched Top surface is not painted"),
		History.HasPaintAfterLastReseed(History.Graph.OriginStateId, false));

	FStormProfileEdit PaintEdit;
	FStormProfileOp& TopStamp = PaintEdit.Ops.AddDefaulted_GetRef();
	TopStamp.Type = EStormProfileOpType::Stamp;
	TopStamp.bTargetAnvil = false;
	FStormProfileOp& AnvilStamp = PaintEdit.Ops.AddDefaulted_GetRef();
	AnvilStamp.Type = EStormProfileOpType::Stamp;
	AnvilStamp.bTargetAnvil = true;
	const FGuid Painted = History.Append(
		History.Graph.OriginStateId,
		PaintEdit);

	FStormProfileEdit ClearTopEdit;
	ClearTopEdit.Type = EStormProfileEditType::ClearCanvas;
	FStormProfileOp& ClearTop = ClearTopEdit.Ops.AddDefaulted_GetRef();
	ClearTop.Type = EStormProfileOpType::FillSurface;
	ClearTop.bTargetAnvil = false;
	ClearTop.Value = 0.f;
	const FGuid TopCleared = History.Append(Painted, ClearTopEdit);

	// The two predicates deliberately disagree about an emptied surface: it holds
	// nothing left to wipe, yet entering Parameterize mode would still discard it.
	TestFalse(TEXT("A cleared Top surface has nothing left to clear"),
		History.HasClearableContent(TopCleared, false));
	TestTrue(TEXT("A cleared Top surface still counts as painted"),
		History.HasPaintAfterLastReseed(TopCleared, false));

	TestTrue(TEXT("Clearing Top leaves Anvil clearable"),
		History.HasClearableContent(TopCleared, true));
	TestTrue(TEXT("Clearing Top preserves Anvil paint state"),
		History.HasPaintAfterLastReseed(TopCleared, true));
	TestEqual(TEXT("A surface fill costs one render pass"),
		ClearTop.GetReplayCost(), int64(1));

	// Painting over an emptied surface makes it clearable again.
	FStormProfileEdit RepaintEdit;
	FStormProfileOp& Repaint = RepaintEdit.Ops.AddDefaulted_GetRef();
	Repaint.Type = EStormProfileOpType::Stamp;
	Repaint.bTargetAnvil = false;
	const FGuid Repainted = History.Append(TopCleared, RepaintEdit);
	TestTrue(TEXT("Painting after a clear restores clearable content"),
		History.HasClearableContent(Repainted, false));

	// An aggregate reseed discards every surface fill, so both are clearable and
	// neither reads as painted.
	FStormProfileEdit ReseedEdit;
	ReseedEdit.Type = EStormProfileEditType::ReseedFromParams;
	FStormProfileOp& Reseed = ReseedEdit.Ops.AddDefaulted_GetRef();
	Reseed.Type = EStormProfileOpType::ReseedFromParams;
	Reseed.PayloadId = History.AddParamsPayload(FStormProfileParams());
	const FGuid Reseeded = History.Append(Repainted, ReseedEdit);
	TestTrue(TEXT("An aggregate reseed leaves Top clearable"),
		History.HasClearableContent(Reseeded, false));
	TestFalse(TEXT("An aggregate reseed clears aggregate paint state"),
		History.HasPaintAfterLastReseed(Reseeded));
	return true;
}

#endif
