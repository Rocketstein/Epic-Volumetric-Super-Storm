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
	"SavageSuperStorm.History.LinearPath",
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
	"SavageSuperStorm.History.BranchPrune",
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
	"SavageSuperStorm.History.Checkpoint",
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
	"SavageSuperStorm.History.FlowMapCheckpointSweep",
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
	"SavageSuperStorm.History.ProfilePayloadPrune",
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

#endif
