#pragma once
#include "CoreMinimal.h"
#include "EditorUndoClient.h"
#include "Types/SlateEnums.h"
#include "Widgets/SCompoundWidget.h"

class UStormVerticalProfileToolComponent;
class UStormVerticalProfileAsset;
class IDetailsView;
class SCompositeProfilePreview;
class SStormProfileTimeline;
class UTexture;
class UTextureRenderTarget2D;
class UWorld;

/**
 * Slate editor for authoring vertical-profile keys and previewing their sequence.
 *
 * The painter coordinates the profile tool component, the timeline widget, and
 * the paint surfaces. It owns the editor interaction state but leaves profile
 * data and playback state in the runtime component and profile asset.
 */
class SStormProfilePainter
    : public SCompoundWidget
    , public FEditorUndoClient
{
public:
    SLATE_BEGIN_ARGS(SStormProfilePainter) {}
    SLATE_END_ARGS()

    /** Builds the painter layout and binds its controls to the profile tool. */
    void Construct(const FArguments& InArgs);
    /** Releases the painter's editor-only widget and details-view resources. */
    virtual ~SStormProfilePainter() override;

    /** Advances editor-world sequence preview while the viewport is open. */
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    /** Refreshes the painter after an undo operation. */
    virtual void PostUndo(bool bSuccess) override;
    /** Refreshes the painter after a redo operation. */
    virtual void PostRedo(bool bSuccess) override;

    /** Points the painter at the component selected by the details-panel launch button. */
    void SetTarget(UStormVerticalProfileToolComponent* InTool);

    /** Returns whether this painter currently owns the supplied tool's editor session. */
    bool IsEditingTool(
        const UStormVerticalProfileToolComponent* Tool) const
    {
        return WeakTool.Get() == Tool;
    }

    /** Saves, discards, or cancels before replacing the profile document. */
    bool PrepareToReplaceProfileDocument(
        const FText& DialogTitle,
        const FText& Prompt);

    /**
     * Confirms pending edits before the host window closes.
     * @return true when the window may close, or false when the user cancels.
     */
    bool ConfirmClose();

private:
    UStormVerticalProfileToolComponent* GetTool() const { return WeakTool.Get(); }
    bool HasTarget() const { return WeakTool.IsValid(); }
    /**
     * Returns whether Revert has a saved asset state to restore the active key from.
     *
     * Requires an asset, not just pending edits: Revert restores the key from what
     * the asset holds, and a profile that has never been saved has no such state.
     */
    bool CanRevert() const;
    FText GetStatusText() const;
    UTextureRenderTarget2D* GetActiveRenderTarget() const;
    bool IsTopViewActive() const;
    bool IsAnvilViewActive() const;
    bool IsParameterizeMode() const;
    bool IsPaintMode() const;
    bool IsPaintingEnabled() const;
    bool IsBlendPaintModeActive() const;
    UStormVerticalProfileAsset* GetEditedProfileAsset() const;
    /** Returns the selected key's current index, or INDEX_NONE when no key is selected. */
    int32 GetSelectedKeyIndex() const;
    /** Returns whether the selected key ID resolves in the active asset. */
    bool HasSelectedKey() const;
    /** Returns whether another profile key can be added to the active asset. */
    bool CanAddProfileKey() const;
    /** Returns whether the selected key may be deleted without invalidating the asset. */
    bool CanDeleteSelectedKey() const;
    /** Returns whether the selected key's timeline time is editable. */
    bool CanEditSelectedKeyTime() const;
    /** Returns the authored time of the selected key. */
    float GetSelectedKeyTime() const;
    /** Returns the checkbox state for the selected key's linear easing mode. */
    ECheckBoxState GetSelectedKeyLinearEasingState() const;
    /** Returns the checkbox state for the selected key's smooth-step easing mode. */
    ECheckBoxState GetSelectedKeySmoothStepEasingState() const;
    /** Returns the checkbox state for the active sequence looping setting. */
    ECheckBoxState GetSequenceLoopState() const;
    /** Returns whether the active profile has enough valid keys to preview. */
    bool CanPreviewProfileSequence() const;
    /** Returns whether the profile tool's sequence playhead is advancing. */
    bool IsProfileSequencePlaying() const;
    /** Returns whether composed sequence surfaces are currently displayed. */
    bool IsProfileSequencePreviewActive() const;
    /** Returns the timeline status text shown beside the playback controls. */
    FText GetProfileSequencePreviewText() const;
    float GetBrushRadiusUV() const;
    UTexture* GetDisplayTopProfile() const;
    UTexture* GetDisplayBottomProfile() const;
    UTexture* GetDisplayAnvilProfile() const;
    EVisibility GetPaintToolsVisibility() const;
    EVisibility GetClearPaintButtonVisibility() const;
    bool CanClearPaintLayer() const;
    ECheckBoxState GetTopViewCheckState() const;
    ECheckBoxState GetBottomViewCheckState() const;
    ECheckBoxState GetAnvilViewCheckState() const;
    ECheckBoxState GetParameterizeModeCheckState() const;
    ECheckBoxState GetPaintModeCheckState() const;
    ECheckBoxState GetBlendPaintModeCheckState() const;
    ECheckBoxState GetOverwritePaintModeCheckState() const;
    void OnTopViewCheckStateChanged(ECheckBoxState NewState);
    void OnBottomViewCheckStateChanged(ECheckBoxState NewState);
    void OnAnvilViewCheckStateChanged(ECheckBoxState NewState);
    void OnParameterizeModeCheckStateChanged(ECheckBoxState NewState);
    void OnPaintModeCheckStateChanged(ECheckBoxState NewState);
    void OnBlendPaintModeCheckStateChanged(ECheckBoxState NewState);
    void OnOverwritePaintModeCheckStateChanged(ECheckBoxState NewState);
    /** Commits an edited selected-key time and keeps the asset keys sorted. */
    void OnSelectedKeyTimeCommitted(
        float NewTime,
        ETextCommit::Type CommitType);
    /** Applies linear easing to the selected key's outgoing segment. */
    void OnSelectedKeyLinearEasingChanged(ECheckBoxState NewState);
    /** Applies smooth-step easing to the selected key's outgoing segment. */
    void OnSelectedKeySmoothStepEasingChanged(ECheckBoxState NewState);
    /** Applies the user's looping preference to the runtime sequence controller. */
    void OnSequenceLoopChanged(ECheckBoxState NewState);
    /** Selects the key reported by the timeline. */
    void OnTimelineKeySelected(FGuid KeyId);
    /** Commits a timeline drag to the selected key's authored time. */
    void OnTimelineKeyTimeCommitted(FGuid KeyId, float NewTime);
    /** Scrubs the runtime sequence preview to the timeline's playhead time. */
    void OnTimelineScrubbed(float TimeSeconds);
    /** Duplicates the selected key at the timeline's requested time. */
    void OnTimelineAddKeyRequested(float TimeSeconds);
    // Shows the "switch discards paint" warning (unless suppressed this session), and on
    // confirm switches the component into Parameterize mode.
    void RequestEnterParameterizeMode();

    /** Stops preview and returns the paint surface to the live authoring textures. */
    void ExitProfileSequencePreview();

    void HandlePaintUV(FVector2D UV);
    /** Opens the tool's stroke group so one drag becomes one history entry. */
    void HandleStrokeBegin();
    /** Closes the stroke group opened by HandleStrokeBegin. */
    void HandleStrokeEnd();
    FReply OnClearPaintLayer();
    FReply OnRevert();
    FReply OnNewProfile();
    FReply OnSaveAs();
    FReply OnSave();
    FReply OnLoad();
    /** Adds a duplicate of the selected profile key after the current selection. */
    FReply OnAddProfileKey();
    /** Removes the selected profile key when the asset still has a valid timeline. */
    FReply OnDeleteSelectedKey();
    /** Starts or resumes forward sequence preview. */
    FReply OnPlayProfileSequence();
    /** Starts or resumes reverse sequence preview. */
    FReply OnPlayProfileSequenceReverse();
    /** Pauses sequence preview at the current playhead. */
    FReply OnPauseProfileSequence();
    /** Stops sequence preview and restores the authoring surfaces. */
    FReply OnStopProfileSequence();
    /** Rebinds the timeline after profile keys are added, removed, or reordered. */
    void RefreshTimelineKeys();
    /** Duplicates the selected key at an optional explicit timeline time. */
    bool DuplicateSelectedKey(
        TOptional<float> RequestedTime = TOptional<float>());
    /** Bakes all resident editor key working sets into the profile asset. */
    bool CommitAllKeys();
    /** Selects a key and activates its editor working surfaces. */
    bool SelectProfileKey(FGuid KeyId);
    /** Refreshes the details panel after timeline or authoring state changes. */
    void RefreshProfileDetails();
    /** Updates all painter state after undo or redo. */
    void HandleUndoRedo(bool bSuccess);
    /**
     * Detaches from the tool component when its world is torn down.
     *
     * The painter window outlives a map change, so without this it keeps
     * pointing at the closing world's component. WeakTool alone does not help:
     * the preview MID's hard texture references are what hold the world's outer
     * chain reachable, so the weak pointer never goes stale and the editor's
     * post-teardown leak check kills the process.
     */
    void HandleWorldCleanup(
        UWorld* World,
        bool bSessionEnded,
        bool bCleanupResources);
    /** Establishes and commits a bounded-history checkpoint when the tool asks. */
    bool TryCheckpointProfileHistory();

private:
    // Set explicitly via SetTarget when the painter window is opened for an actor.
    TWeakObjectPtr<UStormVerticalProfileToolComponent> WeakTool;
    TSharedPtr<IDetailsView> DetailsView;
    TSharedPtr<SStormProfileTimeline> TimelineWidget;
    // Held so world teardown can make it drop its texture references; it is the
    // only widget here that pins UObjects strongly.
    TSharedPtr<SCompositeProfilePreview> CompositePreview;
    FDelegateHandle WorldCleanupHandle;
    FGuid SelectedKeyId;

    /** Surface currently shown in the paint area. */
    enum class EProfileViewMode : uint8
    {
        Top,
        Bottom,
        Anvil
    };

    EProfileViewMode ActiveViewMode = EProfileViewMode::Top;

    /** Painter-session mode; this is not serialized component state. */
    enum class EAuthoringMode : uint8
    {
        Parameterize,
        Paint
    };

    EAuthoringMode ActiveAuthoringMode = EAuthoringMode::Parameterize;

    /** How brush values are applied to painted profile surfaces. */
    enum class EBrushPaintMode : uint8
    {
        Blend,
        Overwrite
    };

    EBrushPaintMode PaintMode = EBrushPaintMode::Overwrite;

    // True between HandleStrokeBegin and HandleStrokeEnd. A stroke's transaction is
    // opened and closed by hand rather than by FScopedTransaction, because the drag
    // outlives the call that starts it, so this is what proves the two halves stay
    // paired -- including when the gesture ends by losing mouse capture instead of
    // by a button release.
    bool bStrokeTransactionOpen = false;

    // Brush state, read at stamp time.
    int32 BrushTexels   = 2;
    float BrushStrength = 0.8f;
    float EraserStrength = 1.0f;
    float BrushValue    = 1.0f;
    bool  bErase        = false;
};
