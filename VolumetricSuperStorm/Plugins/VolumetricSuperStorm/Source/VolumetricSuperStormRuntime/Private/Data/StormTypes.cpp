// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormTypes.cpp
 * @brief Implements lossless radial-shape curve migration and defaults.
 */

#include "Data/StormTypes.h"

#include "Serialization/CustomVersion.h"

const FGuid FStormShapeCurveCustomVersion::GUID(
	0xAE8B7D52,
	0xC6714CB4,
	0xA3D0A9FB,
	0x246E12F7);

namespace
{
FCustomVersionRegistration ShapeCurveVersionRegistration(
	FStormShapeCurveCustomVersion::GUID,
	FStormShapeCurveCustomVersion::LatestVersion,
	TEXT("StormShapeCurveCustomVersion"));

constexpr int32 LegacyCurveSnapshotSampleCount = 256;

float SelectColorChannel(const FLinearColor& Color, int32 ChannelIndex)
{
	switch (ChannelIndex)
	{
	case 0: return Color.R;
	case 1: return Color.G;
	case 2: return Color.B;
	case 3: return Color.A;
	default: return 0.0f;
	}
}

void CopyNamedCurveToLegacyChannel(
	const FRuntimeFloatCurve& Source,
	FRuntimeCurveLinearColor& Destination,
	int32 ChannelIndex)
{
	if (ChannelIndex < 0 ||
		ChannelIndex >= UE_ARRAY_COUNT(Destination.ColorCurves))
	{
		return;
	}

	const FRichCurve* SourceCurve = Source.GetRichCurveConst();
	if (SourceCurve)
	{
		Destination.ColorCurves[ChannelIndex] = *SourceCurve;
	}
}
}

FStormShapeCurves::FStormShapeCurves()
{
	EnsureNamedCurves();
}

bool FStormShapeCurves::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FStormShapeCurveCustomVersion::GUID);

	if (Ar.IsSaving())
	{
		SyncLegacyCurvesFromNamed();
	}

	// Returning false asks UScriptStruct to continue normal tagged-property
	// serialization after registering the custom version.
	return false;
}

void FStormShapeCurves::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading() &&
		Ar.CustomVer(FStormShapeCurveCustomVersion::GUID) <
			FStormShapeCurveCustomVersion::NamedFloatCurves)
	{
		MigrateLegacyCurves();
	}

	EnsureNamedCurves();
}

void FStormShapeCurves::MigrateLegacyCurves()
{
	CopyLegacyChannel(CoverageStrength, 0, BodyCoverage);
	CopyLegacyChannel(CoverageStrength, 3, AnvilFill);
	CopyLegacyChannel(TypeStrength, 1, BottomSoftness);
	CopyLegacyChannel(TypeStrength, 2, TopAnvilProfileCoordinate);
	CopyLegacyChannel(LayerHeight, 0, LayerBottom);
	CopyLegacyChannel(LayerHeight, 2, LayerTop);
}

void FStormShapeCurves::SyncLegacyCurvesFromNamed()
{
	// Inline packed channels keep rollback compatibility without exposing the
	// ambiguous color-curve UI. Unused packed channels are intentionally retained.
	CoverageStrength.ExternalCurve = nullptr;
	TypeStrength.ExternalCurve = nullptr;
	LayerHeight.ExternalCurve = nullptr;

	CopyNamedCurveToLegacyChannel(BodyCoverage, CoverageStrength, 0);
	CopyNamedCurveToLegacyChannel(AnvilFill, CoverageStrength, 3);
	CopyNamedCurveToLegacyChannel(BottomSoftness, TypeStrength, 1);
	CopyNamedCurveToLegacyChannel(
		TopAnvilProfileCoordinate,
		TypeStrength,
		2);
	CopyNamedCurveToLegacyChannel(LayerBottom, LayerHeight, 0);
	CopyNamedCurveToLegacyChannel(LayerTop, LayerHeight, 2);
}

void FStormShapeCurves::EnsureNamedCurves()
{
	EnsureCoverageCurves();
	EnsureTypeCurves();
	EnsureLayerHeightCurves();
}

void FStormShapeCurves::EnsureCoverageCurves()
{
	EnsureCoverageCurve(BodyCoverage, 0.98f, 0.98f, 0.58f);
	EnsureCoverageCurve(AnvilFill, 0.98f, 0.98f, 0.58f);
}

void FStormShapeCurves::EnsureTypeCurves()
{
	EnsureNormalizedCurve(BottomSoftness, 1.0f, 0.0f);
	EnsureNormalizedCurve(TopAnvilProfileCoordinate, 1.0f, 0.0f);
}

void FStormShapeCurves::EnsureLayerHeightCurves()
{
	EnsureNormalizedCurve(LayerBottom, 0.08f, 0.12f);
	EnsureNormalizedCurve(LayerTop, 1.0f, 0.96f);
}

const FRichCurve* FStormShapeCurves::GetLegacyChannel(
	const FRuntimeCurveLinearColor& Curve,
	int32 ChannelIndex)
{
	if (ChannelIndex < 0 ||
		ChannelIndex >= UE_ARRAY_COUNT(Curve.ColorCurves))
	{
		return nullptr;
	}

	return Curve.ExternalCurve
		? &Curve.ExternalCurve->FloatCurves[ChannelIndex]
		: &Curve.ColorCurves[ChannelIndex];
}

void FStormShapeCurves::CopyLegacyChannel(
	const FRuntimeCurveLinearColor& Source,
	int32 ChannelIndex,
	FRuntimeFloatCurve& Destination)
{
	if (Source.ExternalCurve)
	{
		SampleAdjustedLegacyChannel(Source, ChannelIndex, Destination);
		return;
	}

	const FRichCurve* SourceCurve = GetLegacyChannel(Source, ChannelIndex);
	if (!SourceCurve)
	{
		return;
	}

	Destination.ExternalCurve = nullptr;
	Destination.EditorCurveData = *SourceCurve;
}

void FStormShapeCurves::SampleAdjustedLegacyChannel(
	const FRuntimeCurveLinearColor& Source,
	int32 ChannelIndex,
	FRuntimeFloatCurve& Destination)
{
	Destination.ExternalCurve = nullptr;
	FRichCurve& DestinationCurve = Destination.EditorCurveData;
	DestinationCurve.Reset();

	for (int32 Index = 0; Index < LegacyCurveSnapshotSampleCount; ++Index)
	{
		const float Radius01 = static_cast<float>(Index) /
			static_cast<float>(LegacyCurveSnapshotSampleCount - 1);
		const float Value = SelectColorChannel(
			Source.GetLinearColorValue(Radius01),
			ChannelIndex);
		const FKeyHandle Handle = DestinationCurve.AddKey(Radius01, Value);
		DestinationCurve.SetKeyInterpMode(Handle, RCIM_Linear, false);
	}
}

void FStormShapeCurves::EnsureCoverageCurve(
	FRuntimeFloatCurve& Curve,
	float CenterValue,
	float MiddleValue,
	float EdgeValue)
{
	if (Curve.ExternalCurve || Curve.EditorCurveData.GetNumKeys() > 0)
	{
		return;
	}

	Curve.EditorCurveData.AddKey(0.0f, CenterValue);
	Curve.EditorCurveData.AddKey(0.55f, MiddleValue);
	Curve.EditorCurveData.AddKey(1.0f, EdgeValue);
}

void FStormShapeCurves::EnsureNormalizedCurve(
	FRuntimeFloatCurve& Curve,
	float CenterValue,
	float RadiusValue)
{
	if (Curve.ExternalCurve || Curve.EditorCurveData.GetNumKeys() > 0)
	{
		return;
	}

	Curve.EditorCurveData.AddKey(0.0f, CenterValue);
	Curve.EditorCurveData.AddKey(1.0f, RadiusValue);
}
