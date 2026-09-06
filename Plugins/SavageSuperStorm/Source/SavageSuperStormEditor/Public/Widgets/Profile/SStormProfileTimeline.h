#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class UStormVerticalProfileAsset;

/**
 * Compact timeline for vertical-profile keys.
 *
 * The widget owns no authored state. It visualizes bound asset and playhead
 * attributes, then reports selection, scrubbing, add-key requests, and
 * completed key drags to the profile painter.
 */
class SStormProfileTimeline final : public SLeafWidget
{
public:
	/** Invoked when the user selects a timeline key. */
    DECLARE_DELEGATE_OneParam(FOnKeySelected, FGuid);
	/** Invoked when the user scrubs the playhead to a time in seconds. */
    DECLARE_DELEGATE_OneParam(FOnScrubbed, float /* TimeSeconds */);
	/** Invoked when the user requests a new key at a timeline time in seconds. */
    DECLARE_DELEGATE_OneParam(
        FOnAddKeyRequested,
        float /* TimeSeconds */);
	/** Invoked when a dragged key is released at a committed timeline time. */
    DECLARE_DELEGATE_TwoParams(
        FOnKeyTimeCommitted,
        FGuid,
        float /* TimeSeconds */);

    SLATE_BEGIN_ARGS(SStormProfileTimeline) {}
        SLATE_ATTRIBUTE(UStormVerticalProfileAsset*, ProfileAsset)
        SLATE_ATTRIBUTE(FGuid, SelectedKeyId)
        SLATE_ATTRIBUTE(float, PlayheadTime)
        SLATE_ATTRIBUTE(bool, PlayheadActive)
        SLATE_ATTRIBUTE(bool, ScrubbingEnabled)
        SLATE_ATTRIBUTE(bool, AddingEnabled)
        SLATE_EVENT(FOnKeySelected, OnKeySelected)
        SLATE_EVENT(FOnScrubbed, OnScrubbed)
        SLATE_EVENT(FOnAddKeyRequested, OnAddKeyRequested)
        SLATE_EVENT(FOnKeyTimeCommitted, OnKeyTimeCommitted)
    SLATE_END_ARGS()

    /** Binds Slate attributes and delegates used by the timeline. */
    void Construct(const FArguments& InArgs);

    /** Expands the visible time range while the final key is dragged near the right edge. */
    virtual void Tick(
        const FGeometry& AllottedGeometry,
        const double InCurrentTime,
        const float InDeltaTime) override;
    /** Draws the timeline ruler, transition lanes, keys, and playhead. */
    virtual int32 OnPaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;
    /** Starts key selection, scrubbing, dragging, or the add-key context menu. */
    virtual FReply OnMouseButtonDown(
        const FGeometry& Geometry,
        const FPointerEvent& Event) override;
    /** Updates the active scrub or key drag while the pointer moves. */
    virtual FReply OnMouseMove(
        const FGeometry& Geometry,
        const FPointerEvent& Event) override;
    /** Commits the active scrub or key drag and releases mouse capture. */
    virtual FReply OnMouseButtonUp(
        const FGeometry& Geometry,
        const FPointerEvent& Event) override;
    /** Reframes the sequence when empty ruler space is double-clicked. */
    virtual FReply OnMouseButtonDoubleClick(
        const FGeometry& Geometry,
        const FPointerEvent& Event) override;
    /** Clears hover state when the pointer leaves the widget. */
    virtual void OnMouseLeave(const FPointerEvent& Event) override;
    /** Cancels any in-progress pointer interaction after capture is lost. */
    virtual void OnMouseCaptureLost(
        const FCaptureLostEvent& CaptureLostEvent) override;
    /** Returns the cursor appropriate for the current hover or drag state. */
    virtual FCursorReply OnCursorQuery(
        const FGeometry& Geometry,
        const FPointerEvent& Event) const override;
    /** Returns the timeline's preferred compact editor size. */
    virtual FVector2D ComputeDesiredSize(float) const override
    {
        return FVector2D(520.0f, 112.0f);
    }
    /** Keeps the widget eligible for repaint while bound playhead attributes change. */
    virtual bool ComputeVolatility() const override { return true; }

private:
    static constexpr float LeftGutter = 42.0f;
    static constexpr float RightGutter = 12.0f;
    static constexpr float RulerY = 20.0f;
    static constexpr float LaneY = 59.0f;
    static constexpr float KeyHitRadius = 9.0f;
    static constexpr float KeyDragStep = 0.05f;
    static constexpr float MinKeyGap = 0.01f;
    static constexpr float DefaultViewDuration = 5.0f;
    static constexpr float RightEdgeExpansionZone = 36.0f;
    // Continuous growth rate for the held-at-the-edge expansion. ln(2) doubles the
    // visible range for every second of full-pressure hold, so the doubling period
    // is the tunable rather than an emergent property of a clamped per-frame rate.
    static constexpr float EdgeExpansionGrowthPerSecond = 0.69314718f;
    // Bounds the accumulated hold so the exponential cannot reach infinity; the
    // range is clamped to the authoring limit long before this is reached.
    static constexpr float MaxEdgeHoldPressureSeconds = 32.0f;

    /** Returns the asset currently bound to the timeline. */
    UStormVerticalProfileAsset* GetProfileAsset() const;
    /** Returns the bound asset's final key time in seconds. */
    float GetSequenceDuration() const;
    /** Returns whether scrubbing is enabled and the asset has a non-zero duration. */
    bool CanScrub() const;
    /** Returns the minimum duration used to keep the ruler readable. */
    float GetBaseViewDuration() const;
    /** Returns the duration currently represented by the visible ruler. */
    float GetViewDuration() const;
    /** Returns the drawable ruler width after accounting for the side gutters. */
    float GetPlotWidth(const FGeometry& Geometry) const;
    /** Converts a timeline time to a local X coordinate. */
    float TimeToLocalX(float TimeSeconds, const FGeometry& Geometry) const;
    /** Converts a local X coordinate to a clamped timeline time. */
    float LocalXToTime(float LocalX, const FGeometry& Geometry) const;
    /** Snaps a dragged key between neighboring keys while preserving the minimum gap. */
    float ConstrainDraggedKeyTime(float DesiredTime) const;
    /** Returns whether the currently dragged key is the final key in the asset. */
    bool IsDraggingLastKey() const;
    /** Returns the temporary dragged time for a key, or its authored time otherwise. */
    float GetDisplayedKeyTime(const FGuid& KeyId, float AuthoredTime) const;
    /** Returns whether a new key may be requested at the supplied time. */
    bool CanAddKeyAt(float TimeSeconds) const;
    /** Returns the key index under the pointer, or INDEX_NONE when no key is hit. */
    int32 FindKeyUnderCursor(
        const FGeometry& Geometry,
        const FPointerEvent& Event) const;
    /** Chooses a readable ruler interval for the current view duration and width. */
    float ChooseTickInterval(float ViewDuration, float PlotWidth) const;
    /** Emits a scrub event for the pointer's current timeline position. */
    void ScrubAt(const FGeometry& Geometry, const FPointerEvent& Event);
    /** Opens the context menu used to request a new copied profile key. */
    void ShowContextMenu(
        const FGeometry& Geometry,
        const FPointerEvent& Event);
    /** Emits an add-key request when the requested time is valid. */
    void ExecuteAddKeyRequest(float TimeSeconds);
    /** Clears pointer and drag state, leaving any accumulated view expansion intact. */
    void ResetInteraction();
    /** Collapses drag-time range expansion so the view reframes to the sequence. */
    void ResetViewExpansion();

    TAttribute<UStormVerticalProfileAsset*> ProfileAssetAttr;
    TAttribute<FGuid> SelectedKeyIdAttr;
    TAttribute<float> PlayheadTimeAttr;
    TAttribute<bool> PlayheadActiveAttr;
    TAttribute<bool> ScrubbingEnabledAttr;
    TAttribute<bool> AddingEnabledAttr;
    FOnKeySelected OnKeySelected;
    FOnScrubbed OnScrubbed;
    FOnAddKeyRequested OnAddKeyRequested;
    FOnKeyTimeCommitted OnKeyTimeCommitted;

    FGuid DraggedKeyId;
    FGuid HoveredKeyId;
    float DraggedKeyTime = 0.0f;
    float DragStartLocalX = 0.0f;
    float DragPointerLocalX = 0.0f;
    float ExpandedViewDuration = 0.0f;
    /** View duration captured when the active key drag began; anchors the growth curve. */
    float DragAnchorViewDuration = 0.0f;
    /** Edge pressure integrated over the active drag, in pressure-seconds. */
    float EdgeHoldPressureSeconds = 0.0f;
    /** Asset the expansion belongs to, so swapping assets reframes the view. */
    TWeakObjectPtr<UStormVerticalProfileAsset> ExpansionOwnerAsset;
    bool bScrubbing = false;
};
