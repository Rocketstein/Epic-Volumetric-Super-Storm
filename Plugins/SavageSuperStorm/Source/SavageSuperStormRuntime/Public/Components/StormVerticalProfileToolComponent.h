#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "Data/Profile/StormProfileUndoState.h"
#include "Data/Profile/StormProfileParams.h"
#include "StormVerticalProfileToolComponent.generated.h"

class UTexture;
class UTextureRenderTarget2D;
class UTexture2D;
class UStormVerticalProfileAsset;
struct FStormProfileKey;

/** Playback state of the active vertical-profile sequence. */
UENUM(BlueprintType)
enum class EStormProfileSequencePlaybackState : uint8
{
	/** No sequence preview is active. */
	Stopped,
	/** The playhead advances each tick in the selected direction. */
	Playing,
	/** The playhead is held at its current time while the composed preview remains visible. */
	Paused,
};

/** Direction in which the vertical-profile sequence playhead advances. */
UENUM(BlueprintType)
enum class EStormProfileSequencePlaybackDirection : uint8
{
	/** Advances toward later keys and the sequence duration. */
	Forward,
	/** Advances toward earlier keys and time zero. */
	Reverse,
};


/**
 * The profile surfaces currently displayed by the storm actor.
 *
 * During sequence preview these point to the pre-composed current render
 * targets; otherwise they point to the live authoring surfaces. Sequence
 * composition happens before this component/actor boundary.
 */
struct FStormProfileRenderData
{
	UTexture* BottomProfile = nullptr;
	UTexture* TopProfile = nullptr;
	UTexture* AnvilProfile = nullptr;
};

/**
 * Editor-only working surfaces and checkpoints owned by one timeline key.
 *
 * These render targets preserve unbaked edits while another key is active;
 * runtime playback resolves the active key from these surfaces when they are
 * available, and otherwise falls back to the key's baked textures.
 */
USTRUCT()
struct SAVAGESUPERSTORMRUNTIME_API FStormProfileKeyRenderTargetSet
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> Bottom = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> Top = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> Anvil = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> StashTop = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTarget2D> StashAnvil = nullptr;

	UPROPERTY(Transient)
	FStormProfileParams Params;
};

/**
 * Non-transactional record of the state currently baked into one asset key.
 *
 * ContentId is the source of truth. ReplayDepthHint is the saved node's depth and
 * is diagnostic only; branching means depth is not a stable address.
 */
struct FStormProfileSavedState
{
	FGuid ContentId;
	int32 ReplayDepthHint = INDEX_NONE;
};

UCLASS(ClassGroup = (Storm), BlueprintType)
class SAVAGESUPERSTORMRUNTIME_API UStormVerticalProfileToolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient, BlueprintReadOnly, NonTransactional, Category = "Profile")
	TObjectPtr<UTextureRenderTarget2D> BottomTypeProfileRT = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, NonTransactional, Category = "Profile")
	TObjectPtr<UTextureRenderTarget2D> TopTypeProfileRT = nullptr;
	UPROPERTY(Transient, BlueprintReadOnly, NonTransactional, Category = "Profile")
	TObjectPtr<UTextureRenderTarget2D> AnvilProfileRT = nullptr;


	// Immutable replay origins for the paintable Top and Anvil surfaces.
	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<UTextureRenderTarget2D> StashTopRT = nullptr;

	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<UTextureRenderTarget2D> StashAnvilRT = nullptr;

	// Pre-composed surfaces sampled during multi-key sequence playback.
	// Sequence playback resolves each endpoint from
	// a resident editor working RT when available, otherwise from its baked texture.
	// Sampling one composed RT keeps the storm material's single-profile contract, so no
	// material or shader-signature change is needed. Idle, the cloud samples the live
	// authoring surfaces directly.
	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<UTextureRenderTarget2D> BottomCurrentRT = nullptr;

	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<UTextureRenderTarget2D> TopCurrentRT = nullptr;
	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<UTextureRenderTarget2D> AnvilCurrentRT = nullptr;


	// The persistent side of the profile: seeds the live RTs on init and is the target
	// of Save / Save As. It is the profile's
	// initial condition, NOT its result -- the material never samples these textures, it
	// samples the RTs above (see GetTop/BottomTypeProfileTexture on the actor).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, NonTransactional, Category = "Storm|Profile|Loaded")
	TObjectPtr<UStormVerticalProfileAsset> PersistentProfileAsset = nullptr;

	// Properties that drive the parametric generation of the storm profiles.
	// Reflected mirror of the active key's params. Undoable identity lives in the
	// per-key state bookmark; excluding this mirror prevents transactions from
	// a released profile session from overwriting a newly-created scratch profile.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, NonTransactional, Category = "Storm|Profile", meta = (EditCondition = "bParameterizeMode", HideEditConditionToggle))
	FStormProfileParams StormProfileParams;

	// Authoring mode. true = Parameterize (params editable, top rebuilds live from them);
	// false = Paint (params locked via EditCondition, brush enabled). Driven only by the
	// painter's mode toggle -- intentionally not shown in the panel, but referenced by the
	// authoring params' EditCondition so toggling it greys them out.
	UPROPERTY(Transient)
	bool bParameterizeMode = true;

	// Hidden reflected reference that keeps the template asset alive
	UPROPERTY()
	TObjectPtr<UCurveFloat> DefaultProfileCurveTemplate = nullptr;

public:
	UStormVerticalProfileToolComponent();

	// Applies the persistent profile asset. Explicit application discards live
	// preview pixels, resets sequence playback, and forces a reseed. A null asset
	// rebuilds from the current params. Invalid assets leave state unchanged. In
	// editor code, callers that replace an authored document must establish the
	// transaction and undo barrier around this operation.
	bool ApplyProfileConfiguration(
		UStormVerticalProfileAsset* InProfileAsset,
		bool bNotifyOwner = true);

	// Ensures this component owns initialized live render targets. PIE now re-seeds from
	// the persistent asset. The former handoff read editor RTs back to CPU arrays and
	// rebuilt transient textures in PIE; it was removed to reduce transfer/serialization
	// complexity and make PIE match packaged runtime. Parameters remain the last fallback.
	bool EnsureProfilesInitialized();

	UFUNCTION(Category = "Storm|Profile")
	void RebuildVerticalProfiles();

	/**
	 * Starts or resumes forward playback of the keys in PersistentProfileAsset.
	 *
	 * The component blends the three profile surfaces into its Current render
	 * targets so the storm material can continue sampling one profile per surface.
	 * At the end of a non-looping sequence, calling Play again starts at time zero.
	 *
	 * @param bRestart If true, always start at time zero instead of resuming.
	 * @return true when the active asset is complete and playback was started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	bool PlayProfileSequence(bool bRestart = false);

	/**
	 * Starts or resumes reverse playback of the active profile sequence.
	 *
	 * At time zero, reverse playback restarts from the sequence duration.
	 * Authored easing is evaluated using the absolute timeline position, so the
	 * same segment has the same shape in either playback direction.
	 *
	 * @param bRestart If true, always start at the sequence duration.
	 * @return true when the active asset is complete and playback was started.
	 */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	bool PlayProfileSequenceReverse(bool bRestart = false);

	/** Pauses playback without discarding the currently composed preview. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	void PauseProfileSequence();

	/** Stops playback, hides the composed preview, and restores the live authoring surfaces. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	void StopProfileSequence();

	/**
	 * Scrubs to an exact timeline position and keeps the composed preview active.
	 *
	 * The position is clamped to the sequence duration and the sequence is left
	 * paused, which makes this useful for editor preview and deterministic runtime
	 * sampling.
	 *
	 * @param InTimeSeconds Timeline position to display, in seconds.
	 * @return true when the position was valid and the profile surfaces were composed.
	 */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	bool SetProfileSequenceTime(float InTimeSeconds);

	/**
	 * Sets the runtime looping override used by subsequent playback ticks.
	 *
	 * @param bLooping If true, wrap at the sequence boundaries; otherwise pause at the end.
	 */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	void SetProfileSequenceLooping(bool bLooping);

	/** Returns the clamped current sequence playhead position in seconds. */
	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	float GetProfileSequenceTime() const;

	/** Returns the final key time, which defines the sequence duration in seconds. */
	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	float GetProfileSequenceDuration() const;

	/** Returns whether the sequence is stopped, playing, or paused. */
	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	EStormProfileSequencePlaybackState GetProfileSequencePlaybackState() const;

	/** Returns the direction used by the active or most recently selected playback. */
	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	EStormProfileSequencePlaybackDirection GetProfileSequencePlaybackDirection() const;

	/** Returns true only while the playhead is actively advancing. */
	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	bool IsProfileSequencePlaying() const;

	/** Returns true while the storm is sampling the composed sequence render targets. */
	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	bool IsProfileSequencePreviewActive() const;

	/**
	 * Reports whether the active asset has enough valid keys, surfaces, and duration
	 * for sequence playback.
	 */
	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	bool CanPlayProfileSequence() const;

	/** Supplies the actor with the surfaces currently displayed by the storm material. */
	void GetProfileRenderData(FStormProfileRenderData& OutRenderData) const;

#if WITH_EDITOR
	/**
	 * Advances editor-only sequence preview from Slate's viewport update path.
	 *
	 * Editor-world actor components do not necessarily tick while only a viewport
	 * is open, so this provides the equivalent of TickComponent for that case.
	 */
	void AdvanceProfileSequencePreview(float DeltaTime);
#endif

	// Paints a single brush dab onto the live Top or Anvil surface. Blend mode
	// lerps covered texels toward Value; overwrite mode replaces covered texels
	// with Value. Erasing targets zero density and ignores overwrite mode.
	// BrushUV / BrushRadiusUV use the selected profile's native UV space. For Anvil,
	// V=0 is the anchor and V=1 is the cloud-layer bottom.
	UFUNCTION(Category = "Storm|Profile|Paint")
	void StampBrush(
		FVector2D BrushUV,
		float BrushRadiusUV,
		float Strength,
		float Value,
		bool bErase,
		bool bOverwrite,
		bool bPaintAnvil);

	// Seeds the live render targets from PersistentProfileAsset, so a previously saved
	// profile can be reopened and edited. Returns false when the asset is incomplete or
	// the blit could not be issued yet.
	bool SeedSurfacesFromAsset();

#if WITH_EDITOR
	// Activates a key's persistent editor working set. The first activation seeds
	// five RTs (three surfaces plus Top/Anvil replay origins) from the baked key;
	// subsequent activations only swap the active pointers.
	bool SeedSurfacesFromKey(const FStormProfileKey& ProfileKey);

	// Creates an independently editable working set from the source key's live
	// surfaces. Used when Add Key duplicates a key with edits not baked yet.
	bool DuplicateKeyRenderTargets(
		const FGuid& SourceKeyId,
		const FStormProfileKey& NewKey);

	// Moves a structurally removed key's live workspace into non-transactional
	// tombstone storage. Undo can reactivate the exact RTs and history by KeyId.
	void TombstoneKeyRenderTargets(const FGuid& KeyId);

	// Reconciles non-transactional workspaces after an asset undo/redo. Missing
	// keys move to tombstones; matching tombstones move back to the live registry.
	// Tombstoned keys are excluded from save and dirty traversal but retain their
	// exact RTs, history bookmark, immutable graph, and save/replay identities.
	// Returns true when the active live set disappeared, so the caller can select a
	// surviving key. OutRestoredKeyIds receives keys restored from tombstones.
	bool ReconcileKeyRenderTargets(
		const UStormVerticalProfileAsset* OwningAsset,
		TArray<FGuid>& OutRestoredKeyIds);

	void ResetKeyRenderTargets();

	// Starts a clean, unsaved profile editing session. This is a document-lifecycle
	// reset rather than an edit: it releases the targeted asset, discards every key
	// workspace and history, restores canonical params, and creates a new scratch
	// origin. The caller deliberately does not wrap this in an editor transaction.
	bool ResetToNewProfile();

	// Makes a profile-document replacement a real transaction so Unreal drops any
	// redo tail before the caller places an undo barrier. This must be called inside
	// the boundary transaction; any non-transactional reset remains outside it.
	void AdvanceProfileEditSession();

	// True when any current key head has reached the configured depth or replay-
	// cost limit. The editor responds after the active transaction has finalized.
	bool IsProfileHistoryCheckpointRequired() const;

	// Prepares replacement origin surfaces for every live key, invokes the supplied
	// editor callback to establish an irreversible undo boundary, then atomically
	// swaps the origins and releases every old prefix and tombstoned workspace.
	// No history changes when preparation or boundary creation fails.
	bool RebaseProfileHistoriesWithUndoBoundary(
		TFunctionRef<bool()> EstablishUndoBoundary);

	bool GetKeyRenderTargetState(
		const FGuid& KeyId,
		UTextureRenderTarget2D*& OutBottom,
		UTextureRenderTarget2D*& OutTop,
		UTextureRenderTarget2D*& OutAnvil,
		FStormProfileParams& OutParams) const;

	bool HasKeyRenderTargets(const FGuid& KeyId) const;
	bool HasAnyKeyRenderTargets() const;
	void MarkKeyRenderTargetsSaved(const FGuid& KeyId);
	// Diagnostic hint for profile.VerifyReplay output; content identity, not this
	// depth, is authoritative for dirty-state comparisons.
	int32 GetSavedReplayDepthHint(const FGuid& KeyId) const;

	// Appends an atomic edit that reproduces the state last baked into the asset.
	// Its graph state is new but its content identity matches the saved content, so
	// Revert is clean while Undo returns to the pre-revert head. A scratch key
	// appends an equivalent restoration of its parametric origin.
	bool RevertActiveKeyToSaved();

	// Keys whose logs have moved out from under their pixels -- the set an undo or
	// redo just invalidated. Empty when everything is already coherent.
	void CollectStaleKeys(TArray<FGuid>& OutStaleKeys) const;

	// Brings the active key's surfaces back in line with its log. Cheap no-op when
	// they already agree, so it is safe to call unconditionally after an undo.
	bool ReplayActiveKeyIfStale();

	// The active key differs from what is baked for it (drives the "uncommitted"
	// indicator).
	bool IsActiveKeyModified() const;

	// Any key working set differs from what is baked into PersistentProfileAsset.
	bool IsUnsavedToAsset() const;

	// Flags the working state as changed since the checkpoint (called on paint / param edit).
	void MarkProfileDirty();

	// Entering Parameterize rebuilds Top and Anvil from params, discarding paint.
	// Entering Paint freezes the current surfaces as the paint base.
	void SetParameterizeMode(bool bEnable);
	bool IsParameterizeMode() const { return bParameterizeMode; }

	// Brushwork or loaded detail sits on top of the current parametric macro, so
	// entering Parameterize mode would discard it. Derived from the active key's
	// log rather than tracked in a flag.
	bool HasPaintedProfiles() const;

	// --- Per-key edit history ------------------------------------------------
	// The non-transactional graph describes every replayable surface state. Sealed
	// edits are immutable; Unreal transacts only each key's current-state bookmark,
	// while saved asset identities remain outside the transaction system.

	// Opens one atomic stroke transaction. Stamps accumulate in replay-bounded edit
	// chunks; EndStroke seals the final chunk. The chunks remain one Unreal undo
	// step because the bookmark is modified only at BeginStroke. Nesting is not
	// supported; a second BeginStroke without EndStroke continues the gesture.
	void BeginStroke();
	void EndStroke();

	// Appends a params reseed to the active key's log. Call after the surfaces have
	// been rebuilt, so the log and the pixels always agree. Interactive property
	// events are ignored by the caller; each completed change appends forward.
	void RecordReseedFromParams();

	const FStormProfileKeyHistory* FindKeyHistory(const FGuid& KeyId) const;
	const FGuid& GetActiveKeyId() const { return ActiveKeyRenderTargetId; }
	FGuid GetActiveHistoryStateId() const
	{
		return GetCurrentHistoryStateId(ActiveKeyRenderTargetId);
	}

	// Replays the active key's log from its origin into scratch surfaces and diffs
	// them against the live ones. This is the invariant the whole design rests on
	// -- replay(origin, ops) == live pixels -- so it is verifiable before anything
	// depends on it. Returns false when the replay could not be run; on success
	// OutMaxDelta carries the largest per-channel difference found.
	bool VerifyActiveKeyReplay(float& OutMaxDelta);
#endif

protected:
	void OnRegister() override;
	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	bool SampleVerticalProfileCurve(const FRuntimeFloatCurve& Curve, uint16 NumSamples, TArray<float>& Out, float MinT = 0.f, float MaxT = 1.f) const;
	// Allocates the profile RTs when missing or resized. Returns true if it (re)allocated.
	bool RegenerateProfileRenderTargets();
	void ResetProfileParamsToDefaults();
	void BuildBottomTypeProfile();
	void BuildBottomTypeProfile(
		UTextureRenderTarget2D* TargetRT,
		const FStormProfileParams& Params);
	// Builds the parametric top profile into the paint surface.
	void BuildTopTypeProfile(UTextureRenderTarget2D* TargetRT);
	void BuildAnvilProfile();
	// Params- and target-explicit forms. Replay has to rebuild a historical params
	// snapshot into a scratch surface, which the member-reading forms above cannot
	// express; those now forward here with StormProfileParams and the live RTs.
	void BuildTopTypeProfile(
		UTextureRenderTarget2D* TargetRT,
		const FStormProfileParams& Params);
	void BuildAnvilProfile(
		UTextureRenderTarget2D* TargetRT,
		const FStormProfileParams& Params);
	// Issues one brush dab against an explicit target. StampBrush forwards here for
	// the live surfaces; replay forwards here for scratch surfaces. Returns false
	// when the pass could not be issued, which is what keeps the edit log honest:
	// an op is only recorded for a dab that actually ran.
	bool StampBrushInto(
		UTextureRenderTarget2D* TargetRT,
		FVector2D BrushUV,
		float BrushRadiusUV,
		float Strength,
		float Value,
		bool bErase,
		bool bOverwrite);
	// Full-target opaque blit of a source texture (Texture2D or RT) into a render target.
	// Returns false when the draw could not be issued: the canvas needs World->Scene,
	// which is not guaranteed during OnRegister. Callers holding a one-shot source must
	// keep it until this returns true.
	bool BlitToRenderTarget(UTexture* Source, UTextureRenderTarget2D* Target);
	bool HasCompletePersistentProfile() const;
	UTextureRenderTarget2D* CreateProfileRenderTarget(
		const FName& BaseName,
		int32 Resolution);

	/** Mutable playhead and preview state for the active profile sequence. */
	struct FProfileSequenceRuntimeState
	{
		/** Current playhead position, clamped to the sequence duration when queried. */
		float TimeSeconds = 0.0f;
		/** Whether the composed Current render targets are being displayed. */
		bool bPreviewActive = false;
		/** Whether the playhead wraps when it reaches either end of the timeline. */
		bool bLooping = false;
		/** Whether bLooping overrides the asset's bLoopByDefault setting. */
		bool bHasLoopingOverride = false;
		/** Current playback state. */
		EStormProfileSequencePlaybackState State =
			EStormProfileSequencePlaybackState::Stopped;
		/** Direction used when advancing the playhead. */
		EStormProfileSequencePlaybackDirection Direction =
			EStormProfileSequencePlaybackDirection::Forward;
	};

	/** Three corresponding profile surfaces used as one blend operation's endpoints. */
	struct FProfileSurfaceSet
	{
		/** Bottom-density surface for this key. */
		UTexture* Bottom = nullptr;
		/** Top-density surface for this key. */
		UTexture* Top = nullptr;
		/** Anvil-density surface for this key. */
		UTexture* Anvil = nullptr;

		/** Returns true only when all three surfaces are available. */
		bool IsComplete() const
		{
			return Bottom && Top && Anvil;
		}
	};

	/** Returns whether an asset is structurally valid for use by this component. */
	bool IsProfileAssetComplete(const UStormVerticalProfileAsset* ProfileAsset) const;
	/** Notifies the owning actor that the profile textures it supplies have changed. */
	void NotifyProfileRenderDataChanged();
	/** Advances the playhead, handles bounds/looping, and recomposes the current surfaces. */
	void TickProfileSequence(float DeltaTime);
	/** Initializes playback state for the requested direction and composes its first frame. */
	bool StartProfileSequencePlayback(
		EStormProfileSequencePlaybackDirection Direction,
		bool bRestart);
	/** Enables component ticking only while the sequence playhead is advancing. */
	void UpdatePlaybackTickEnabled();
	/** Clears playback/preview state; optionally returns the playhead to time zero. */
	void ResetProfileSequenceRuntime(bool bResetTime);
	/** Resolves a key to editor working surfaces when available, or baked surfaces at runtime. */
	bool ResolveProfileSequenceKeySurfaces(
		int32 KeyIndex,
		FProfileSurfaceSet& OutSurfaces) const;
	/** Blends both endpoint surface sets into the component's Current render targets. */
	bool ComposeProfileSurfaces(
		const FProfileSurfaceSet& From,
		const FProfileSurfaceSet& To,
		float Alpha);
	/** Finds the current timeline segment, applies its easing, and composes that frame. */
	bool ComposeProfileSequenceAtCurrentTime();

	/** Returns true when the cloud should sample the composed Current render targets. */
	bool IsSamplingCompositedProfile() const;

private:
	UPROPERTY(Transient, DuplicateTransient)
	bool bProfilesInitialized = false;

	FProfileSequenceRuntimeState ProfileSequenceRuntime;

#if WITH_EDITOR
	void RestoreKeyRenderTargetState(
		const FGuid& KeyId,
		FStormProfileKeyRenderTargetSet& State);
	bool RestoreTombstonedKeyRenderTargets(const FGuid& KeyId);
	void UpdateActiveKeyParams();
	bool AllocateKeyRenderTargets(
		const FGuid& KeyId,
		const FStormProfileParams& Params,
		FStormProfileKeyRenderTargetSet& OutState);

	// Gives the pre-asset surfaces a real working set with a generated id, so edits
	// made before any asset exists are still recorded. Without it the component
	// paints on unowned scratch RTs, RecordOp finds no active key, and every op is
	// dropped -- no undo at all until the first save. The id is later adopted by
	// the first baked key (see CommitToolToKey), which keeps this log attached to
	// the key it becomes.
	void EnsureScratchWorkingSet();

	// Creates the key's immutable history graph and transactional bookmark if
	// absent, anchoring its origin at the supplied params. The origin pixels are
	// the key's already-populated Stash surfaces.
	FStormProfileKeyHistory& EnsureKeyHistory(
		const FGuid& KeyId,
		const FStormProfileParams& OriginParams);
	void EnsureEditUndoState();
	UStormProfileKeyUndoState* EnsureKeyUndoState(const FGuid& KeyId);
	UStormProfileKeyUndoState* FindKeyUndoState(const FGuid& KeyId);
	const UStormProfileKeyUndoState* FindKeyUndoState(const FGuid& KeyId) const;
	UStormProfileKeyUndoState* FindActiveKeyUndoState();
	const UStormProfileKeyUndoState* FindActiveKeyUndoState() const;
	bool IsHistoryStrokeOpen() const;
	void ModifyEditHistory();
	FStormProfileKeyHistory* FindActiveKeyHistory();
	FGuid GetCurrentHistoryStateId(const FGuid& KeyId) const;
	bool IsHistorySaved(
		const FGuid& KeyId,
		const FStormProfileKeyHistory& History) const;
	FGuid AppendHistoryEdit(
		FStormProfileKeyHistory& History,
		UStormProfileKeyUndoState& KeyState,
		const FStormProfileEdit& Edit,
		const FGuid& RequestedContentId = FGuid());
	bool FlushPendingStrokeEdit();
	bool AppendSavedAssetRestore(
		const FStormProfileSavedState& SavedState);
	// Adds a stamp to the active stroke edit, or appends a one-dab stroke edit when
	// no explicit stroke is open. No-op when no key is active.
	void RecordOp(const FStormProfileOp& Op);
	// Chooses a surviving position for a bookmark that names a state the history no
	// longer holds. Pruning is what made that possible, so this is the recovery for
	// a branch that was reclaimed while undo still referenced it. Returns an invalid
	// id only when the history has nothing left to fall back to.
	FGuid RepairMissingHistoryState(
		const FStormProfileKeyHistory& History,
		const FGuid& MissingStateId) const;
	// Replays the active key to TargetStateId in its live surfaces, then syncs params,
	// the parametric bottom, and the reflected-position shadow. The single path
	// through which the active key's pixels ever move along its log.
	bool ReplayActiveKeyTo(const FGuid& TargetStateId);

	// Replays the root-to-target graph path over the supplied origin surfaces into
	// the outputs. The origin is a parameter rather than the StashTop/StashAnvil
	// members because those follow the active key, and replaying a non-active key
	// against them would start from the wrong pixels.
	bool ReplayKeyInto(
		const FStormProfileKeyHistory& History,
		const FGuid& TargetStateId,
		UTextureRenderTarget2D* OriginTop,
		UTextureRenderTarget2D* OriginAnvil,
		UTextureRenderTarget2D* OutTop,
		UTextureRenderTarget2D* OutAnvil);
	void ValidateKeyRenderTargetOwnership() const;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, NonTransactional)
	TMap<FGuid, FStormProfileKeyRenderTargetSet> KeyRenderTargets;

	// Workspaces for keys currently absent from the transactional asset. Moving a
	// value here retains its five RTs without copying them; a matching key restored
	// by undo moves the same value back into KeyRenderTargets.
	UPROPERTY(Transient, NonTransactional)
	TMap<FGuid, FStormProfileKeyRenderTargetSet> TombstonedKeyRenderTargets;

	// Stable session registry. Immutable graphs stay here and are never transacted;
	// each key's small bookmark object participates independently in undo/redo.
	UPROPERTY(Transient, NonTransactional)
	TObjectPtr<UStormProfileUndoState> EditUndoState = nullptr;

	// Transactional sentinel used only to commit an irreversible profile-history
	// or document-replacement boundary. Its value has no authoring/runtime meaning.
	UPROPERTY(Transient)
	int32 ProfileEditSessionSerial = 0;

	// What each key's target asset currently contains. This must not be a
	// UPROPERTY: an undo changes a key's transactional bookmark, but a save
	// performed after that transaction must remain authoritative. Content identity
	// lets a newly appended Revert state compare equal to the saved asset.
	TMap<FGuid, FStormProfileSavedState> SavedStatesByKey;

	// The mutable builder for the current stroke chunk. Long gestures may seal
	// several chunks before EndStroke while the surrounding transaction stays open.
	FStormProfileEdit PendingStrokeEdit;

	// What each key's surfaces actually reflect, as a stable state identity.
	//
	// Deliberately a plain member rather than a UPROPERTY: it must NOT be
	// transacted. An undo rolls edit histories back but leaves this alone, and the
	// disagreement between the two is precisely how we identify which keys' pixels
	// the undo invalidated -- without needing the transaction system to tell us.
	TMap<FGuid, FGuid> ReplayedStateByKey;

	FGuid ActiveKeyRenderTargetId;

	// The working set created before any asset existed, if one is still in that
	// state. It is not in any asset's Keys by definition, so pruning must exempt
	// it or the first undo on an unsaved profile destroys everything painted so
	// far. Cleared once an asset key adopts the id, after which it prunes normally.
	FGuid ScratchWorkingSetId;
#endif
};
