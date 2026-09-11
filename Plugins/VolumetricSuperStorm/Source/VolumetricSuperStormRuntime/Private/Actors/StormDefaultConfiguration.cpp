// Copyright 2026 GoroGoro. All Rights Reserved.

#include "Actors/StormDefaultConfiguration.h"

#include <initializer_list>

namespace
{
	struct FStormDefaultCurveKey
	{
		float Time;
		float Value;
		ERichCurveInterpMode InterpMode = RCIM_Linear;
		ERichCurveTangentMode TangentMode = RCTM_Auto;
		float ArriveTangent = 0.0f;
		float LeaveTangent = 0.0f;
	};

	void SetDefaultCurve(
		FRuntimeFloatCurve& Curve,
		std::initializer_list<FStormDefaultCurveKey> Keys)
	{
		Curve.ExternalCurve = nullptr;
		FRichCurve& RichCurve = Curve.EditorCurveData;
		RichCurve.Reset();
		RichCurve.PreInfinityExtrap = RCCE_Constant;
		RichCurve.PostInfinityExtrap = RCCE_Constant;

		for (const FStormDefaultCurveKey& Key : Keys)
		{
			const FKeyHandle Handle = RichCurve.AddKey(Key.Time, Key.Value);
			FRichCurveKey& RichKey = RichCurve.GetKey(Handle);
			RichKey.InterpMode = Key.InterpMode;
			RichKey.TangentMode = Key.TangentMode;
			RichKey.ArriveTangent = Key.ArriveTangent;
			RichKey.LeaveTangent = Key.LeaveTangent;
		}
	}

	FStormShapeSettings MakeDefaultShapeSettings()
	{
		FStormShapeSettings Settings;
		Settings.Extent = FVector(800000.0, 800000.0, 2200.0);
		Settings.Radius = 600000.0f;
		Settings.CoordinateScale = 1.0f;
		Settings.EnvelopeRadius = 672.925903f;
		Settings.EnvelopeFalloff = 32.520756f;
		Settings.bClockwise = false;
		Settings.AnvilStrength = 1.0f;
		Settings.AnvilDepth01 = 0.434f;
		Settings.AnvilHeightTwistDegrees = 5.0f;
		Settings.AnvilHeightTwistStart01 = 0.57096f;
		Settings.AnvilCoverage = 0.2364f;
		Settings.StormBaseColor = FLinearColor::Black;
		Settings.UndersideVisibility = 0.192f;
		Settings.BrimEmissiveColor = FLinearColor::Black;
		Settings.StormStaticGlowIntensity = 0.05f;
		Settings.StormStaticGlowColor = FLinearColor(1.0f, 0.191990f, 0.045736f, 1.0f);
		Settings.GlowExtent = FVector(0.15, 0.15, 0.14);
		Settings.Density = 0.05f;
		Settings.DensityGamma = 1.0f;
		Settings.HFStrength = 0.75f;
		Settings.WindDirection = FVector(1.0, 0.15, 0.0);

		SetDefaultCurve(Settings.ShapeCurves.BodyCoverage, {
			{ 0.002301f, 0.771621f },
			{ 0.199879f, 0.770276f, RCIM_Cubic, RCTM_Auto, -0.083445f, -0.083445f },
			{ 0.395151f, 0.738840f, RCIM_Cubic, RCTM_User, -0.282655f, -0.282654f },
			{ 0.570279f, 0.636115f, RCIM_Cubic, RCTM_User, 0.022331f, 0.022331f },
			{ 0.679070f, 0.663713f, RCIM_Cubic, RCTM_Auto, 0.502166f, 0.502166f },
			{ 0.881539f, 0.792419f },
			{ 1.011637f, 0.683030f, RCIM_Cubic, RCTM_User, -0.325353f, -0.325354f },
		});
		SetDefaultCurve(Settings.ShapeCurves.AnvilFill, {
			{ 0.014158f, 0.847785f },
			{ 0.223994f, 0.847721f, RCIM_Cubic, RCTM_Auto, -0.074862f, -0.074862f },
			{ 0.618813f, 0.802519f },
			{ 0.870467f, 0.876342f },
			{ 0.941437f, 0.897161f },
		});
		SetDefaultCurve(Settings.ShapeCurves.BottomSoftness, {
			{ -0.003284f, 0.998727f },
			{ 0.121750f, 0.993028f, RCIM_Cubic, RCTM_Auto, -0.143488f, -0.143488f },
			{ 0.204419f, 0.968924f, RCIM_Cubic, RCTM_User, -1.163047f, -1.163048f },
			{ 0.327670f, 0.686255f, RCIM_Cubic, RCTM_Auto, -1.435303f, -1.435303f },
			{ 0.496013f, 0.550398f, RCIM_Cubic, RCTM_Auto, -1.332866f, -1.332866f },
			{ 0.645008f, 0.263285f },
			{ 0.825060f, 0.263285f },
			{ 1.073132f, 0.263285f },
		});
		SetDefaultCurve(Settings.ShapeCurves.TopAnvilProfileCoordinate, {
			{ 0.0f, 1.0f },
			{ 0.218497f, 0.965187f },
			{ 0.279493f, 0.926689f },
			{ 0.360822f, 0.782320f },
			{ 0.474681f, 0.673242f, RCIM_Cubic, RCTM_User, -1.814935f, -1.814934f },
			{ 0.974852f, 0.047646f },
		});
		SetDefaultCurve(Settings.ShapeCurves.LayerBottom, {
			{ 0.005541f, 0.053581f },
			{ 0.113245f, 0.061345f, RCIM_Cubic, RCTM_Auto, 0.220142f, 0.220142f },
			{ 0.307077f, 0.119962f },
			{ 0.457128f, 0.119962f, RCIM_Cubic, RCTM_User, 0.474762f, 0.474762f },
			{ 0.612539f, 0.226714f, RCIM_Cubic, RCTM_Auto, 0.364952f, 0.364952f },
			{ 0.808141f, 0.248064f, RCIM_Cubic },
		});
		SetDefaultCurve(Settings.ShapeCurves.LayerTop, {
			{ 0.0f, 1.0f },
			{ 0.261282f, 0.989549f },
			{ 0.263718f, 0.989451f },
			{ 0.558462f, 0.977661f },
			{ 0.975000f, 0.961000f },
			{ 1.000000f, 0.960000f },
		});

		return Settings;
	}

	FStormMotionSettings MakeDefaultMotionSettings()
	{
		FStormMotionSettings Settings;
		Settings.bEnabled = true;
		Settings.bPreviewInEditor = true;
		Settings.MotionStrength = 1.0f;
		Settings.TimeScale = 1.0f;
		Settings.RingCount = 6;
		Settings.RingEndRadii01 = { 0.10f, 0.22f, 0.36f, 0.54f, 0.74f };
		Settings.RingAngularSpeedDegrees = { 50.0f, 40.0f, 30.0f, 20.0f, 10.0f, 5.0f };
		Settings.RingSkewDegrees = { 140.0f, 96.0f, 60.0f, 34.0f, 16.0f, 0.0f };
		Settings.BoundaryOverlap01 = 0.04f;
		Settings.TransitionMode = EStormRingTransitionMode::ResultBlend;
		Settings.MotionRadiusScale = 1.0f;
		Settings.RadialFeather01 = 0.08f;
		Settings.HeightMin01 = 0.0f;
		Settings.HeightMax01 = 0.62f;
		Settings.HeightFeather01 = 0.08f;
		Settings.LFRotationMultiplier = 1.0f;
		Settings.HFRotationMultiplier = 1.08f;
		Settings.CurlRotationMultiplier = 1.12f;
		Settings.RadialShearGain = 0.6f;
		return Settings;
	}

	FStormLightningSequenceSettings MakeDefaultLightningSettings()
	{
		FStormLightningSequenceSettings Settings;
		Settings.bEnabled = true;
		Settings.bAutoPlay = true;
		Settings.bLoop = true;
		SetDefaultCurve(Settings.FlashPulseCurve, {
			{ 0.0f, 1.0f },
			{ 0.12f, 1.0f },
			{ 0.25f, 0.18f },
			{ 0.42f, 0.72f },
			{ 0.65f, 0.08f },
			{ 1.0f, 0.0f },
		});
		Settings.GroundStrikeTimes01 = { 0.2f };
		Settings.CustomCues.Reset();
		Settings.MaterialState.CenterN = FVector(0.0, 0.0, 0.204621);
		Settings.MaterialState.FlashExtentN = FVector(0.15, 0.15, 0.15);
		Settings.MaterialState.HaloExtentN = FVector(0.5, 0.5, 0.16);
		Settings.MaterialState.HotWhiteColor = FLinearColor(0.931261f, 0.927993f, 1.0f, 1.0f);
		Settings.MaterialState.FillColor = FLinearColor(1.0f, 0.050082f, 0.137594f, 0.2f);
		Settings.MaterialState.LeakColor = FLinearColor(1.0f, 0.02f, 0.05f, 1.0f);
		Settings.MaterialState.CorePeakHDR = 4.0f;
		Settings.MaterialState.FillIntensity = 0.006f;
		Settings.MaterialState.LeakIntensity = 0.1f;
		return Settings;
	}
}

namespace VolumetricSuperStorm::Defaults
{
	FStormDefaultConfiguration MakeStormDefaultConfiguration()
	{
		FStormDefaultConfiguration Configuration;
		Configuration.ShapeSettings = MakeDefaultShapeSettings();
		Configuration.MotionSettings = MakeDefaultMotionSettings();
		Configuration.LightningSequence = MakeDefaultLightningSettings();
		return Configuration;
	}
}
