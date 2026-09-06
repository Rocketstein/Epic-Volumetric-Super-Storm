#include "Widgets/Profile/SStormProfileTimeline.h"

#include "Assets/StormVerticalProfileAsset.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "InputCoreTypes.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

namespace
{
const FLinearColor TimelineBackground(0.018f, 0.022f, 0.028f, 1.0f);
const FLinearColor RulerColor(0.42f, 0.46f, 0.52f, 0.8f);
const FLinearColor LaneColor(0.055f, 0.065f, 0.08f, 1.0f);
const FLinearColor LinearSegmentColor(0.08f, 0.36f, 0.48f, 0.9f);
const FLinearColor SmoothSegmentColor(0.31f, 0.18f, 0.52f, 0.9f);
const FLinearColor KeyColor(0.58f, 0.64f, 0.72f, 1.0f);
const FLinearColor SelectedKeyColor(0.08f, 0.62f, 0.95f, 1.0f);
const FLinearColor DraggedKeyColor(1.0f, 0.66f, 0.16f, 1.0f);
const FLinearColor PlayheadColor(1.0f, 0.28f, 0.12f, 1.0f);
}

void SStormProfileTimeline::Construct(const FArguments& InArgs)
{
    ProfileAssetAttr = InArgs._ProfileAsset;
    SelectedKeyIdAttr = InArgs._SelectedKeyId;
    PlayheadTimeAttr = InArgs._PlayheadTime;
    PlayheadActiveAttr = InArgs._PlayheadActive;
    ScrubbingEnabledAttr = InArgs._ScrubbingEnabled;
    AddingEnabledAttr = InArgs._AddingEnabled;
    OnKeySelected = InArgs._OnKeySelected;
    OnScrubbed = InArgs._OnScrubbed;
    OnAddKeyRequested = InArgs._OnAddKeyRequested;
    OnKeyTimeCommitted = InArgs._OnKeyTimeCommitted;
    SetToolTipText(INVTEXT(
        "Click the timeline to scrub. Click a key to select it; drag any "
        "key after K1 to change its time. Right-click empty space to add "
        "a copy of the selected key. Hold the last key at the right edge "
        "to extend the visible range; double-click empty space to reframe "
        "the sequence. K1 is fixed at 0 seconds."));
}

void SStormProfileTimeline::Tick(
    const FGeometry& AllottedGeometry,
    const double InCurrentTime,
    const float InDeltaTime)
{
    SLeafWidget::Tick(
        AllottedGeometry,
        InCurrentTime,
        InDeltaTime);

    // The expansion belongs to one asset's authoring session. Since it now
    // survives a drag release, a swapped asset has to reframe explicitly or it
    // inherits the previous asset's range.
    if (ExpansionOwnerAsset.Get() != GetProfileAsset())
    {
        ExpansionOwnerAsset = GetProfileAsset();
        ResetViewExpansion();
    }

    if (!IsDraggingLastKey() ||
        InDeltaTime <= 0.0f ||
        DragPointerLocalX <=
            DragStartLocalX + 2.0f)
    {
        return;
    }

    const float RightEdge =
        static_cast<float>(
            AllottedGeometry.GetLocalSize().X) -
        RightGutter;
    const float ExpansionStart = FMath::Max(
        LeftGutter,
        RightEdge - RightEdgeExpansionZone);
    const float EdgePressure = FMath::Clamp(
        (DragPointerLocalX - ExpansionStart) /
            FMath::Max(
                RightEdge - ExpansionStart,
                1.0f),
        0.0f,
        1.0f);
    if (EdgePressure <= 0.0f)
    {
        return;
    }

    // Squaring the pressure keeps expansion gentle on entering the edge zone,
    // while a pointer held against the boundary still extends the range
    // decisively. Growth is time-based, so it continues without mouse motion.
    const float WeightedPressure =
        EdgePressure * EdgePressure;

    // Integrating pressure and evaluating the curve in closed form, rather than
    // stepping the duration each frame, makes the range an exact function of how
    // long the edge has been held:
    //
    //     View = Anchor * e^(rate * held)
    //
    // so growth is geometric in hold time with a directly tunable doubling
    // period. The previous incremental form floored its rate at an absolute
    // constant that, at the default 5s view, exactly cancelled the relative term
    // (5 * 0.15 == 0.75) and made the expansion linear. What looked geometric was
    // really the *committed* drag raising the sequence-derived base between
    // gestures, which is why the acceleration only appeared after a release.
    //
    // Pressure gates the integral rather than the range, so easing off the edge
    // pauses growth instead of reversing it, and half pressure accumulates four
    // times slower -- that squared term is the user's throttle.
    EdgeHoldPressureSeconds = FMath::Min(
        EdgeHoldPressureSeconds +
            WeightedPressure * InDeltaTime,
        MaxEdgeHoldPressureSeconds);
    // Cap at the authoring limit, or a held pointer would widen the view -- and
    // with it the last key's drag ceiling -- without bound.
    ExpandedViewDuration = FMath::Min(
        DragAnchorViewDuration *
            FMath::Exp(
                EdgeExpansionGrowthPerSecond *
                    EdgeHoldPressureSeconds),
        UStormVerticalProfileAsset::MaxKeyTimeSeconds);
    DraggedKeyTime = ConstrainDraggedKeyTime(
        LocalXToTime(
            DragPointerLocalX,
            AllottedGeometry));
    Invalidate(EInvalidateWidgetReason::Paint);
}

UStormVerticalProfileAsset* SStormProfileTimeline::GetProfileAsset() const
{
    return ProfileAssetAttr.Get(nullptr);
}

float SStormProfileTimeline::GetSequenceDuration() const
{
    const UStormVerticalProfileAsset* Asset = GetProfileAsset();
    return Asset ? Asset->GetDurationSeconds() : 0.0f;
}

bool SStormProfileTimeline::CanScrub() const
{
    return ScrubbingEnabledAttr.Get(false) &&
        GetSequenceDuration() > KINDA_SMALL_NUMBER;
}

float SStormProfileTimeline::GetBaseViewDuration() const
{
    // Deliberately derived from the authored duration, never from DraggedKeyTime.
    // LocalXToTime scales by GetViewDuration(), so a base that followed the drag
    // would close the loop T = f * 1.05 * T, which has no stable solution once the
    // pointer fraction f passes ~0.95 -- exactly where the edge expansion zone is.
    // Drag-time growth belongs in ExpandedViewDuration for that reason.
    const float Duration = GetSequenceDuration();
    return FMath::Max(
        DefaultViewDuration,
        Duration + FMath::Max(0.5f, Duration * 0.05f));
}

float SStormProfileTimeline::GetViewDuration() const
{
    return FMath::Max(
        GetBaseViewDuration(),
        ExpandedViewDuration);
}

float SStormProfileTimeline::GetPlotWidth(const FGeometry& Geometry) const
{
    return FMath::Max(
        static_cast<float>(Geometry.GetLocalSize().X) -
            LeftGutter -
            RightGutter,
        1.0f);
}

float SStormProfileTimeline::TimeToLocalX(
    float TimeSeconds,
    const FGeometry& Geometry) const
{
    return LeftGutter +
        FMath::Clamp(TimeSeconds / GetViewDuration(), 0.0f, 1.0f) *
            GetPlotWidth(Geometry);
}

float SStormProfileTimeline::LocalXToTime(
    float LocalX,
    const FGeometry& Geometry) const
{
    const float Fraction = FMath::Clamp(
        (LocalX - LeftGutter) / GetPlotWidth(Geometry),
        0.0f,
        1.0f);
    return Fraction * GetViewDuration();
}

float SStormProfileTimeline::ConstrainDraggedKeyTime(float DesiredTime) const
{
    const UStormVerticalProfileAsset* Asset = GetProfileAsset();
    if (!Asset || !DraggedKeyId.IsValid())
    {
        return 0.0f;
    }

    const int32 KeyIndex = Asset->Keys.IndexOfByPredicate(
        [this](const FStormProfileKey& Key)
        {
            return Key.KeyId == DraggedKeyId;
        });
    if (KeyIndex <= 0)
    {
        return 0.0f;
    }

    float Minimum = Asset->Keys[KeyIndex - 1].TimeSeconds + MinKeyGap;
    // The last key is bounded only by the visible range, so the authoring limit
    // has to apply here as well -- this is what the drag actually commits.
    float Maximum = FMath::Min(
        GetViewDuration(),
        UStormVerticalProfileAsset::MaxKeyTimeSeconds);
    if (Asset->Keys.IsValidIndex(KeyIndex + 1))
    {
        Maximum = Asset->Keys[KeyIndex + 1].TimeSeconds - MinKeyGap;
    }
    if (Maximum < Minimum)
    {
        return Asset->Keys[KeyIndex].TimeSeconds;
    }

    const float Snapped = FMath::RoundToFloat(
        DesiredTime / KeyDragStep) * KeyDragStep;
    return FMath::Clamp(Snapped, Minimum, Maximum);
}

bool SStormProfileTimeline::IsDraggingLastKey() const
{
    const UStormVerticalProfileAsset* Asset =
        GetProfileAsset();
    return Asset &&
        !Asset->Keys.IsEmpty() &&
        DraggedKeyId.IsValid() &&
        Asset->Keys.Last().KeyId == DraggedKeyId;
}

float SStormProfileTimeline::GetDisplayedKeyTime(
    const FGuid& KeyId,
    float AuthoredTime) const
{
    return DraggedKeyId == KeyId
        ? DraggedKeyTime
        : AuthoredTime;
}

bool SStormProfileTimeline::CanAddKeyAt(
    float TimeSeconds) const
{
    const UStormVerticalProfileAsset* Asset =
        GetProfileAsset();
    if (!OnAddKeyRequested.IsBound() ||
        !AddingEnabledAttr.Get(false) ||
        !Asset ||
        Asset->GetKeyCount() >=
            UStormVerticalProfileAsset::MaxProfileKeys ||
        !FMath::IsFinite(TimeSeconds) ||
        TimeSeconds < 0.0f ||
        TimeSeconds >
            UStormVerticalProfileAsset::MaxKeyTimeSeconds)
    {
        return false;
    }

    return !Asset->Keys.ContainsByPredicate(
        [TimeSeconds](const FStormProfileKey& Key)
        {
            return FMath::IsNearlyEqual(
                Key.TimeSeconds,
                TimeSeconds);
        });
}

int32 SStormProfileTimeline::FindKeyUnderCursor(
    const FGeometry& Geometry,
    const FPointerEvent& Event) const
{
    const UStormVerticalProfileAsset* Asset = GetProfileAsset();
    if (!Asset)
    {
        return INDEX_NONE;
    }

    const FVector2D Local = Geometry.AbsoluteToLocal(
        Event.GetScreenSpacePosition());
    if (FMath::Abs(static_cast<float>(Local.Y) - LaneY) > 22.0f)
    {
        return INDEX_NONE;
    }

    int32 BestIndex = INDEX_NONE;
    float BestDistance = KeyHitRadius;
    for (int32 KeyIndex = 0;
        KeyIndex < Asset->Keys.Num();
        ++KeyIndex)
    {
        const FStormProfileKey& Key = Asset->Keys[KeyIndex];
        const float KeyX = TimeToLocalX(
            GetDisplayedKeyTime(Key.KeyId, Key.TimeSeconds),
            Geometry);
        const float Distance = FMath::Abs(
            static_cast<float>(Local.X) - KeyX);
        if (Distance <= BestDistance)
        {
            BestDistance = Distance;
            BestIndex = KeyIndex;
        }
    }
    return BestIndex;
}

float SStormProfileTimeline::ChooseTickInterval(
    float ViewDuration,
    float PlotWidth) const
{
    const float TargetTickCount = FMath::Max(
        FMath::FloorToFloat(PlotWidth / 82.0f),
        1.0f);
    const float RawInterval = ViewDuration / TargetTickCount;
    const float Magnitude = FMath::Pow(
        10.0f,
        FMath::FloorToFloat(FMath::LogX(10.0f, RawInterval)));
    const float Normalized = RawInterval / Magnitude;
    const float NiceNormalized = Normalized <= 1.0f
        ? 1.0f
        : Normalized <= 2.0f
            ? 2.0f
            : Normalized <= 5.0f
                ? 5.0f
                : 10.0f;
    return FMath::Max(NiceNormalized * Magnitude, KINDA_SMALL_NUMBER);
}

int32 SStormProfileTimeline::OnPaint(
    const FPaintArgs&,
    const FGeometry& Geometry,
    const FSlateRect&,
    FSlateWindowElementList& Out,
    int32 LayerId,
    const FWidgetStyle&,
    bool bParentEnabled) const
{
    const FVector2D LocalSize = Geometry.GetLocalSize();
    const float Width = static_cast<float>(LocalSize.X);
    const float Height = static_cast<float>(LocalSize.Y);
    const float PlotWidth = GetPlotWidth(Geometry);
    const FSlateBrush* WhiteBrush =
        FCoreStyle::Get().GetBrush("WhiteBrush");
    const float EnabledOpacity = bParentEnabled ? 1.0f : 0.45f;

    FSlateDrawElement::MakeBox(
        Out,
        LayerId,
        Geometry.ToPaintGeometry(),
        WhiteBrush,
        ESlateDrawEffect::None,
        TimelineBackground.CopyWithNewOpacity(
            TimelineBackground.A * EnabledOpacity));

    const TArray<FVector2f> RulerLine =
    {
        FVector2f(LeftGutter, RulerY),
        FVector2f(Width - RightGutter, RulerY),
    };
    FSlateDrawElement::MakeLines(
        Out,
        LayerId + 1,
        Geometry.ToPaintGeometry(),
        RulerLine,
        ESlateDrawEffect::None,
        RulerColor.CopyWithNewOpacity(
            RulerColor.A * EnabledOpacity),
        true,
        1.0f);

    const float ViewDuration = GetViewDuration();
    const float TickInterval = ChooseTickInterval(
        ViewDuration,
        PlotWidth);
    const FSlateFontInfo RulerFont =
        FCoreStyle::GetDefaultFontStyle("Regular", 8);
    const int32 TickCount = FMath::Min(
        FMath::FloorToInt(ViewDuration / TickInterval) + 1,
        100);
    for (int32 TickIndex = 0;
        TickIndex < TickCount;
        ++TickIndex)
    {
        const float TickTime = TickIndex * TickInterval;
        const float TickX = TimeToLocalX(TickTime, Geometry);
        const TArray<FVector2f> TickLine =
        {
            FVector2f(TickX, RulerY - 4.0f),
            FVector2f(TickX, RulerY + 5.0f),
        };
        FSlateDrawElement::MakeLines(
            Out,
            LayerId + 1,
            Geometry.ToPaintGeometry(),
            TickLine,
            ESlateDrawEffect::None,
            RulerColor.CopyWithNewOpacity(
                RulerColor.A * EnabledOpacity),
            true,
            1.0f);

        const FString TickLabel = TickInterval < 1.0f
            ? FString::Printf(TEXT("%.2fs"), TickTime)
            : FString::Printf(TEXT("%.0fs"), TickTime);
        FSlateDrawElement::MakeText(
            Out,
            LayerId + 2,
            Geometry.ToPaintGeometry(
                FVector2f(40.0f, 12.0f),
                FSlateLayoutTransform(
                    FVector2f(TickX + 3.0f, 2.0f))),
            TickLabel,
            RulerFont,
            ESlateDrawEffect::None,
            FLinearColor(0.62f, 0.66f, 0.72f, EnabledOpacity));
    }

    FSlateDrawElement::MakeBox(
        Out,
        LayerId + 1,
        Geometry.ToPaintGeometry(
            FVector2f(PlotWidth, 24.0f),
            FSlateLayoutTransform(
                FVector2f(LeftGutter, LaneY - 12.0f))),
        WhiteBrush,
        ESlateDrawEffect::None,
        LaneColor.CopyWithNewOpacity(
            LaneColor.A * EnabledOpacity));

    const UStormVerticalProfileAsset* Asset = GetProfileAsset();
    if (!Asset || Asset->Keys.IsEmpty())
    {
        FSlateDrawElement::MakeText(
            Out,
            LayerId + 2,
            Geometry.ToPaintGeometry(
                FVector2f(260.0f, 18.0f),
                FSlateLayoutTransform(
                    FVector2f(LeftGutter + 8.0f, LaneY - 8.0f))),
            TEXT("Save or load a profile asset to author keys"),
            FCoreStyle::GetDefaultFontStyle("Regular", 9),
            ESlateDrawEffect::None,
            FLinearColor(0.55f, 0.58f, 0.62f, EnabledOpacity));
        return LayerId + 3;
    }

    const FSlateFontInfo KeyFont =
        FCoreStyle::GetDefaultFontStyle("Bold", 8);
    const FSlateFontInfo SegmentFont =
        FCoreStyle::GetDefaultFontStyle("Regular", 7);

    for (int32 KeyIndex = 0;
        KeyIndex + 1 < Asset->Keys.Num();
        ++KeyIndex)
    {
        const FStormProfileKey& FromKey = Asset->Keys[KeyIndex];
        const FStormProfileKey& ToKey = Asset->Keys[KeyIndex + 1];
        const float FromX = TimeToLocalX(
            GetDisplayedKeyTime(
                FromKey.KeyId,
                FromKey.TimeSeconds),
            Geometry);
        const float ToX = TimeToLocalX(
            GetDisplayedKeyTime(
                ToKey.KeyId,
                ToKey.TimeSeconds),
            Geometry);
        const float SegmentX = FMath::Min(FromX, ToX);
        const float SegmentWidth = FMath::Max(
            FMath::Abs(ToX - FromX),
            1.0f);
        const bool bSmooth =
            FromKey.OutgoingEasing ==
                EStormProfileTransitionEasing::SmoothStep;
        const FLinearColor SegmentColor = bSmooth
            ? SmoothSegmentColor
            : LinearSegmentColor;

        FSlateDrawElement::MakeBox(
            Out,
            LayerId + 2,
            Geometry.ToPaintGeometry(
                FVector2f(SegmentWidth, 10.0f),
                FSlateLayoutTransform(
                    FVector2f(SegmentX, LaneY - 5.0f))),
            WhiteBrush,
            ESlateDrawEffect::None,
            SegmentColor.CopyWithNewOpacity(
                SegmentColor.A * EnabledOpacity));

        if (SegmentWidth >= 54.0f)
        {
            FSlateDrawElement::MakeText(
                Out,
                LayerId + 3,
                Geometry.ToPaintGeometry(
                    FVector2f(SegmentWidth, 10.0f),
                    FSlateLayoutTransform(
                        FVector2f(SegmentX + 5.0f, LaneY - 5.0f))),
                bSmooth ? TEXT("Smooth") : TEXT("Linear"),
                SegmentFont,
                ESlateDrawEffect::None,
                FLinearColor(0.9f, 0.94f, 1.0f, EnabledOpacity));
        }
    }

    const FGuid SelectedKeyId = SelectedKeyIdAttr.Get(FGuid());
    for (int32 KeyIndex = 0;
        KeyIndex < Asset->Keys.Num();
        ++KeyIndex)
    {
        const FStormProfileKey& Key = Asset->Keys[KeyIndex];
        const float DisplayedTime = GetDisplayedKeyTime(
            Key.KeyId,
            Key.TimeSeconds);
        const float KeyX = TimeToLocalX(
            DisplayedTime,
            Geometry);
        const bool bSelected = Key.KeyId == SelectedKeyId;
        const bool bDragged = Key.KeyId == DraggedKeyId;
        const bool bHovered = Key.KeyId == HoveredKeyId;
        FLinearColor HandleColor = bDragged
            ? DraggedKeyColor
            : bSelected
                ? SelectedKeyColor
                : KeyColor;
        if (bHovered && !bDragged)
        {
            HandleColor = FLinearColor::LerpUsingHSV(
                HandleColor,
                FLinearColor::White,
                0.32f);
        }
        HandleColor.A *= EnabledOpacity;

        FSlateDrawElement::MakeBox(
            Out,
            LayerId + 4,
            Geometry.ToPaintGeometry(
                FVector2f(3.0f, 30.0f),
                FSlateLayoutTransform(
                    FVector2f(KeyX - 1.5f, LaneY - 15.0f))),
            WhiteBrush,
            ESlateDrawEffect::None,
            HandleColor);
        FSlateDrawElement::MakeBox(
            Out,
            LayerId + 5,
            Geometry.ToPaintGeometry(
                FVector2f(11.0f, 9.0f),
                FSlateLayoutTransform(
                    FVector2f(KeyX - 5.5f, LaneY - 4.5f))),
            WhiteBrush,
            ESlateDrawEffect::None,
            HandleColor);

        const FString KeyLabel = FString::Printf(
            TEXT("K%d"),
            KeyIndex + 1);
        FSlateDrawElement::MakeText(
            Out,
            LayerId + 6,
            Geometry.ToPaintGeometry(
                FVector2f(28.0f, 12.0f),
                FSlateLayoutTransform(
                    FVector2f(KeyX - 8.0f, LaneY + 18.0f))),
            KeyLabel,
            KeyFont,
            ESlateDrawEffect::None,
            HandleColor);

        if (bSelected || bDragged || bHovered)
        {
            const FString TimeLabel = FString::Printf(
                TEXT("%.2fs%s"),
                DisplayedTime,
                KeyIndex == 0 ? TEXT("  fixed") : TEXT(""));
            FSlateDrawElement::MakeText(
                Out,
                LayerId + 6,
                Geometry.ToPaintGeometry(
                    FVector2f(74.0f, 12.0f),
                    FSlateLayoutTransform(
                        FVector2f(
                            FMath::Clamp(
                                KeyX - 22.0f,
                                LeftGutter,
                                FMath::Max(
                                    Width - RightGutter - 74.0f,
                                    LeftGutter)),
                            LaneY + 32.0f))),
                TimeLabel,
                RulerFont,
                ESlateDrawEffect::None,
                FLinearColor(0.82f, 0.86f, 0.92f, EnabledOpacity));
        }
    }

    const float PlayheadTime = FMath::Clamp(
        PlayheadTimeAttr.Get(0.0f),
        0.0f,
        GetSequenceDuration());
    const float PlayheadX = TimeToLocalX(
        PlayheadTime,
        Geometry);
    const bool bPlayheadActive =
        PlayheadActiveAttr.Get(false);
    const FLinearColor EffectivePlayheadColor =
        PlayheadColor.CopyWithNewOpacity(
            (bPlayheadActive ? 1.0f : 0.38f) *
            EnabledOpacity);
    const TArray<FVector2f> PlayheadLine =
    {
        FVector2f(PlayheadX, RulerY),
        FVector2f(PlayheadX, FMath::Max(Height - 7.0f, LaneY + 18.0f)),
    };
    FSlateDrawElement::MakeLines(
        Out,
        LayerId + 7,
        Geometry.ToPaintGeometry(),
        PlayheadLine,
        ESlateDrawEffect::None,
        EffectivePlayheadColor,
        true,
        1.5f);
    FSlateDrawElement::MakeBox(
        Out,
        LayerId + 8,
        Geometry.ToPaintGeometry(
            FVector2f(7.0f, 5.0f),
            FSlateLayoutTransform(
                FVector2f(PlayheadX - 3.5f, RulerY - 2.0f))),
        WhiteBrush,
        ESlateDrawEffect::None,
        EffectivePlayheadColor);

    return LayerId + 9;
}

void SStormProfileTimeline::ScrubAt(
    const FGeometry& Geometry,
    const FPointerEvent& Event)
{
    const float Duration = GetSequenceDuration();
    if (!CanScrub())
    {
        return;
    }

    const FVector2D Local = Geometry.AbsoluteToLocal(
        Event.GetScreenSpacePosition());
    OnScrubbed.ExecuteIfBound(
        FMath::Clamp(
            LocalXToTime(static_cast<float>(Local.X), Geometry),
            0.0f,
            Duration));
}

void SStormProfileTimeline::ShowContextMenu(
    const FGeometry& Geometry,
    const FPointerEvent& Event)
{
    const FVector2D Local = Geometry.AbsoluteToLocal(
        Event.GetScreenSpacePosition());
    const float RequestedTime = FMath::Clamp(
        FMath::RoundToFloat(
            LocalXToTime(
                static_cast<float>(Local.X),
                Geometry) /
            KeyDragStep) *
            KeyDragStep,
        0.0f,
        GetViewDuration());
    const bool bCanAdd = CanAddKeyAt(RequestedTime);

    FNumberFormattingOptions TimeFormat;
    TimeFormat.MinimumFractionalDigits = 2;
    TimeFormat.MaximumFractionalDigits = 2;
    FMenuBuilder MenuBuilder(
        /*bInShouldCloseWindowAfterMenuSelection=*/true,
        nullptr);
    MenuBuilder.AddMenuEntry(
        FText::Format(
            INVTEXT("Add Key at {0}s"),
            FText::AsNumber(RequestedTime, &TimeFormat)),
        INVTEXT(
            "Duplicate the selected key, including its independent live "
            "render-target working set, at this timeline time."),
        FSlateIcon(),
        FUIAction(
            FExecuteAction::CreateSP(
                this,
                &SStormProfileTimeline::ExecuteAddKeyRequest,
                RequestedTime),
            FCanExecuteAction::CreateLambda(
                [bCanAdd]()
                {
                    return bCanAdd;
                })));

    const FWidgetPath WidgetPath =
        Event.GetEventPath()
            ? *Event.GetEventPath()
            : FWidgetPath();
    FSlateApplication::Get().PushMenu(
        AsShared(),
        WidgetPath,
        MenuBuilder.MakeWidget(),
        Event.GetScreenSpacePosition(),
        FPopupTransitionEffect(
            FPopupTransitionEffect::ContextMenu));
}

void SStormProfileTimeline::ExecuteAddKeyRequest(
    float TimeSeconds)
{
    if (CanAddKeyAt(TimeSeconds))
    {
        OnAddKeyRequested.ExecuteIfBound(TimeSeconds);
    }
}

FReply SStormProfileTimeline::OnMouseButtonDown(
    const FGeometry& Geometry,
    const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    UStormVerticalProfileAsset* Asset = GetProfileAsset();
    if (!Asset)
    {
        return FReply::Unhandled();
    }

    const int32 KeyIndex = FindKeyUnderCursor(
        Geometry,
        Event);
    if (Asset->Keys.IsValidIndex(KeyIndex))
    {
        const FStormProfileKey& Key = Asset->Keys[KeyIndex];
        OnKeySelected.ExecuteIfBound(Key.KeyId);
        HoveredKeyId = Key.KeyId;
        if (KeyIndex > 0)
        {
            DraggedKeyId = Key.KeyId;
            DraggedKeyTime = Key.TimeSeconds;
            DragPointerLocalX = static_cast<float>(
                Geometry.AbsoluteToLocal(
                    Event.GetScreenSpacePosition()).X);
            DragStartLocalX =
                DragPointerLocalX;
            // Anchor the growth curve on what the user can currently see, so a
            // drag starting inside an already-expanded view keeps compounding from
            // there instead of restarting. GetViewDuration already floors at
            // DefaultViewDuration through the base; restating it here keeps the
            // anchor well-defined on its own terms, since a zero or negative
            // anchor would flatten the exponential to nothing.
            DragAnchorViewDuration = FMath::Max(
                GetViewDuration(),
                DefaultViewDuration);
            EdgeHoldPressureSeconds = 0.0f;
            ExpandedViewDuration =
                DragAnchorViewDuration;
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled().CaptureMouse(
                SharedThis(this));
        }
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    if (!CanScrub())
    {
        return FReply::Handled();
    }
    bScrubbing = true;
    ScrubAt(Geometry, Event);
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SStormProfileTimeline::OnMouseMove(
    const FGeometry& Geometry,
    const FPointerEvent& Event)
{
    const UStormVerticalProfileAsset* Asset = GetProfileAsset();
    if (!Asset)
    {
        ResetInteraction();
        return FReply::Unhandled();
    }

    if (DraggedKeyId.IsValid())
    {
        const FVector2D Local = Geometry.AbsoluteToLocal(
            Event.GetScreenSpacePosition());
        DragPointerLocalX =
            static_cast<float>(Local.X);
        DraggedKeyTime = ConstrainDraggedKeyTime(
            LocalXToTime(
                DragPointerLocalX,
                Geometry));
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }
    if (bScrubbing)
    {
        ScrubAt(Geometry, Event);
        return FReply::Handled();
    }

    const int32 HoveredIndex = FindKeyUnderCursor(
        Geometry,
        Event);
    const FGuid NewHoveredKeyId =
        Asset->Keys.IsValidIndex(HoveredIndex)
            ? Asset->Keys[HoveredIndex].KeyId
            : FGuid();
    if (NewHoveredKeyId != HoveredKeyId)
    {
        HoveredKeyId = NewHoveredKeyId;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return FReply::Unhandled();
}

FReply SStormProfileTimeline::OnMouseButtonUp(
    const FGeometry& Geometry,
    const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() ==
        EKeys::RightMouseButton)
    {
        // A key drag or a scrub owns mouse capture until its own button release.
        if (!DraggedKeyId.IsValid() && !bScrubbing)
        {
            ShowContextMenu(Geometry, Event);
        }
        return FReply::Handled();
    }
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    if (DraggedKeyId.IsValid())
    {
        const FGuid CommittedKeyId = DraggedKeyId;
        const float CommittedTime = DraggedKeyTime;
        ResetInteraction();
        OnKeyTimeCommitted.ExecuteIfBound(
            CommittedKeyId,
            CommittedTime);
        return FReply::Handled().ReleaseMouseCapture();
    }
    if (bScrubbing)
    {
        ResetInteraction();
        return FReply::Handled().ReleaseMouseCapture();
    }
    return FReply::Unhandled();
}

FReply SStormProfileTimeline::OnMouseButtonDoubleClick(
    const FGeometry& Geometry,
    const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return FReply::Unhandled();
    }

    // Range expansion now persists past the drag that produced it, so there has
    // to be a way back. Empty ruler space only: double-clicking a key is left to
    // the normal select/drag path.
    const UStormVerticalProfileAsset* Asset = GetProfileAsset();
    const int32 KeyIndex = FindKeyUnderCursor(Geometry, Event);
    if (Asset && Asset->Keys.IsValidIndex(KeyIndex))
    {
        return FReply::Unhandled();
    }

    ResetViewExpansion();
    return FReply::Handled();
}

void SStormProfileTimeline::OnMouseLeave(
    const FPointerEvent& Event)
{
    SLeafWidget::OnMouseLeave(Event);
    HoveredKeyId.Invalidate();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SStormProfileTimeline::OnMouseCaptureLost(
    const FCaptureLostEvent& CaptureLostEvent)
{
    SLeafWidget::OnMouseCaptureLost(CaptureLostEvent);
    ResetInteraction();
}

FCursorReply SStormProfileTimeline::OnCursorQuery(
    const FGeometry& Geometry,
    const FPointerEvent& Event) const
{
    const UStormVerticalProfileAsset* Asset = GetProfileAsset();
    const int32 KeyIndex = FindKeyUnderCursor(
        Geometry,
        Event);
    const bool bMovableKey =
        DraggedKeyId.IsValid() ||
        (Asset && KeyIndex > 0);
    if (bMovableKey)
    {
        return FCursorReply::Cursor(
            EMouseCursor::ResizeLeftRight);
    }
    return CanScrub()
        ? FCursorReply::Cursor(EMouseCursor::Crosshairs)
        : FCursorReply::Unhandled();
}

void SStormProfileTimeline::ResetInteraction()
{
    DraggedKeyId.Invalidate();
    DraggedKeyTime = 0.0f;
    DragStartLocalX = 0.0f;
    DragPointerLocalX = 0.0f;
    DragAnchorViewDuration = 0.0f;
    EdgeHoldPressureSeconds = 0.0f;
    // ExpandedViewDuration deliberately survives. Collapsing it here snapped the
    // view back to the committed key's range the moment the pointer released --
    // dragging out to 12s but dropping the key at 6s rewound the view to 6.5s --
    // which is most of why the growth read as something that only happened
    // between drags. ResetViewExpansion is the explicit way back.
    bScrubbing = false;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SStormProfileTimeline::ResetViewExpansion()
{
    if (ExpandedViewDuration == 0.0f)
    {
        return;
    }
    ExpandedViewDuration = 0.0f;
    Invalidate(EInvalidateWidgetReason::Paint);
}
