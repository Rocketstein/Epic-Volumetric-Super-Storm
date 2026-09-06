#pragma once

#include "CoreMinimal.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SLeafWidget.h"

class UTexture;

/** Profile surface shown by the compact preview carousel. */
enum class EStormProfilePreviewMode : uint8
{
    BodyComposite,
    Anvil,
    Count
};

/** Displays either the top/bottom composite or the raw anvil profile surface. */
class SCompositeProfilePreview : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SCompositeProfilePreview) {}
        SLATE_ATTRIBUTE(UTexture*, TopProfile)
        SLATE_ATTRIBUTE(UTexture*, BottomProfile)
        SLATE_ATTRIBUTE(UTexture*, AnvilProfile)
    SLATE_END_ARGS()

    /** Binds the profile texture attributes used by the preview material. */
    void Construct(const FArguments& InArgs);

    /** Draws the composited profile preview into the Slate widget. */
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

    /** Returns the preview's preferred square display size. */
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(256.f, 256.f); }

    /** Selects the profile surface displayed by the preview. */
    void SetPreviewMode(EStormProfilePreviewMode InMode);

    /** Moves to the previous or next preview mode when that mode exists. */
    void CyclePreview(int32 Direction);

    /** Returns whether the preview can move in the requested direction. */
    bool CanCyclePreview(int32 Direction) const;

    /** Returns the label displayed over the active preview. */
    FText GetPreviewModeText() const;

    EStormProfilePreviewMode GetPreviewMode() const { return PreviewMode; }

    /**
     * Drops the preview's references to the profile surfaces it last drew.
     *
     * The MID outlives any one world -- it is a permanent GC root held by this
     * widget -- but the textures it samples are owned by the profile tool
     * component, so they carry the world in their outer chain. Anything that
     * tears a world down must call this first, or the MID's texture parameters
     * keep that world alive past the verification GC.
     *
     * Const because the paint path calls it too, and that path is const by
     * Slate convention; the brush it clears is already mutable for the same
     * reason.
     */
    void ReleaseProfileTextures() const;

private:
    TAttribute<UTexture*> TopProfileAttr;
    TAttribute<UTexture*> BottomProfileAttr;
    TAttribute<UTexture*> AnvilProfileAttr;

    EStormProfilePreviewMode PreviewMode =
        EStormProfilePreviewMode::BodyComposite;

    TStrongObjectPtr<UMaterialInstanceDynamic> CompositeMID;
    mutable FSlateBrush Brush;
};
