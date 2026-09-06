#include "Widgets/Profile/SStormProfileCompositePreview.h"

#include "Engine/Texture.h"
#include "Brushes/SlateColorBrush.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DrawElements.h"

namespace
{
    static const FName TopProfileParameterName(TEXT("TopProfile"));
    static const FName BottomProfileParameterName(TEXT("BottomProfile"));
    static const TCHAR* CompositePreviewMaterialPath =
        TEXT("/SavageSuperStorm/VerticalProfile/Internal/M_StormProfileCompositePreview.M_StormProfileCompositePreview");
}

void SCompositeProfilePreview::Construct(const FArguments& InArgs)
{
    TopProfileAttr = InArgs._TopProfile;
    BottomProfileAttr = InArgs._BottomProfile;
    AnvilProfileAttr = InArgs._AnvilProfile;

    Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.TintColor = FSlateColor(FLinearColor::White);

    if (UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, CompositePreviewMaterialPath))
    {
        CompositeMID.Reset(UMaterialInstanceDynamic::Create(BaseMaterial, GetTransientPackage()));
    }
}

void SCompositeProfilePreview::SetPreviewMode(
    EStormProfilePreviewMode InMode)
{
    if (InMode == EStormProfilePreviewMode::Count || PreviewMode == InMode)
    {
        return;
    }

    // Drop the resource used by the previous page before binding the next one.
    ReleaseProfileTextures();
    PreviewMode = InMode;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SCompositeProfilePreview::CyclePreview(int32 Direction)
{
    if (!CanCyclePreview(Direction))
    {
        return;
    }

    const int32 Step = Direction < 0 ? -1 : 1;
    SetPreviewMode(static_cast<EStormProfilePreviewMode>(
        static_cast<int32>(PreviewMode) + Step));
}

bool SCompositeProfilePreview::CanCyclePreview(int32 Direction) const
{
    if (Direction == 0)
    {
        return false;
    }

    const int32 Step = Direction < 0 ? -1 : 1;
    const int32 Next = static_cast<int32>(PreviewMode) + Step;
    return Next >= 0 &&
        Next < static_cast<int32>(EStormProfilePreviewMode::Count);
}

FText SCompositeProfilePreview::GetPreviewModeText() const
{
    return PreviewMode == EStormProfilePreviewMode::Anvil
        ? INVTEXT("Anvil")
        : INVTEXT("Body Composite");
}

void SCompositeProfilePreview::ReleaseProfileTextures() const
{
    if (UMaterialInstanceDynamic* MID = CompositeMID.Get())
    {
        MID->ClearParameterValues();
    }
    Brush.SetResourceObject(nullptr);
}

int32 SCompositeProfilePreview::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
    const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    (void)Args;
    (void)CullingRect;
    (void)InWidgetStyle;
    (void)bParentEnabled;

    static const FSlateColorBrush BlackFallbackBrush(FLinearColor::Black);

    if (PreviewMode == EStormProfilePreviewMode::Anvil)
    {
        if (UTexture* AnvilProfile = AnvilProfileAttr.Get())
        {
            // The anvil surface uses its own vertical coordinate, so display it
            // directly instead of folding it into the body composite.
            Brush.SetResourceObject(AnvilProfile);
            Brush.TintColor = FSlateColor(FLinearColor::White);

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(),
                &Brush,
                ESlateDrawEffect::None);

            return LayerId + 1;
        }
    }
    else
    {
        UTexture* TopProfile = TopProfileAttr.Get();
        UTexture* BottomProfile = BottomProfileAttr.Get();
        UMaterialInstanceDynamic* MID = CompositeMID.Get();
        if (TopProfile && BottomProfile && MID)
        {
            MID->SetTextureParameterValue(TopProfileParameterName, TopProfile);
            MID->SetTextureParameterValue(BottomProfileParameterName, BottomProfile);
            Brush.SetResourceObject(MID);
            Brush.TintColor = FSlateColor(FLinearColor::White);

            FSlateDrawElement::MakeBox(
                OutDrawElements,
                LayerId,
                AllottedGeometry.ToPaintGeometry(),
                &Brush,
                ESlateDrawEffect::None);

            return LayerId + 1;
        }
    }

    // No surfaces to show: let go of whatever we sampled last. Without this the
    // MID keeps hard references to the previous textures forever, which is how a
    // closed world stays reachable even after the painter has been detached.
    ReleaseProfileTextures();

    FSlateDrawElement::MakeBox(
        OutDrawElements,
        LayerId,
        AllottedGeometry.ToPaintGeometry(),
        &BlackFallbackBrush,
        ESlateDrawEffect::None);

    return LayerId + 1;
}
