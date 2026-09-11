// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormLightningComponent.cpp
 * @brief Evaluates lightning cues, material pulses, and ground strikes.
 */

#include "Components/StormLightningComponent.h"

#include "EngineUtils.h"
#include "LandscapeHeightfieldCollisionComponent.h"
#include "LandscapeProxy.h"
#include "Actors/VolumetricSuperStormActor.h"
#include "Components/StormMaterialBinderComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogStormLightning, Log, All);

namespace StormLightning
{
	constexpr float FallbackSequenceDuration   = 1.0f;
	constexpr float GroundTraceDistance        = 2000000.0f;
	constexpr float MinimumFallbackBoltLength  = 1000.0f;
	constexpr float KilometersToCentimeters    = 100000.0f;

	float SampleFallbackPulse(float NormalizedTime)
	{
		static constexpr float Times[]  = { 0.00f, 0.12f, 0.25f, 0.42f, 0.65f, 1.00f };
		static constexpr float Values[] = { 1.00f, 1.00f, 0.18f, 0.72f, 0.08f, 0.00f };

		const float SafeTime = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);
		for (int32 Index = 1; Index < UE_ARRAY_COUNT(Times); ++Index)
		{
			if (SafeTime <= Times[Index])
			{
				const float Alpha = (SafeTime - Times[Index - 1]) / FMath::Max(Times[Index] - Times[Index - 1], SMALL_NUMBER);
				return FMath::Lerp(Values[Index - 1], Values[Index], Alpha);
			}
		}

		return Values[UE_ARRAY_COUNT(Values) - 1];
	}
}

UStormLightningComponent::UStormLightningComponent()
{
	PrimaryComponentTick.bCanEverTick          = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickInterval          = 0.0f;
}

void UStormLightningComponent::InitializeRuntime(UStormMaterialBinderComponent* InMaterialBinder)
{
	MaterialBinder      = InMaterialBinder;
	bRuntimeInitialized = ResolveRuntimeBindings();

	if (!bRuntimeInitialized)
	{
		UE_LOG(LogStormLightning, Warning, TEXT("Cannot initialize %s: its owner is not a VolumetricSuperStormActor."), *GetPathName());
		return;
	}

	InitializeSequenceRandom();
	if (SequenceSettings.bEnabled && SequenceSettings.bAutoPlay)
	{
		PlayLightningSequence();
	}
	else
	{
		UploadPulse(0.0f);
	}

	UE_LOG(LogStormLightning, Log, TEXT( "Lightning initialized for %s. Binder=%s Enabled=%s AutoPlay=%s " "GroundStrikeProbability=%.2f RepeatDelay=%.2fs"), *GetPathNameSafe(StormOwner.Get()), *GetPathNameSafe(MaterialBinder.Get()), SequenceSettings.bEnabled ? TEXT("true") : TEXT("false"), SequenceSettings.bAutoPlay ? TEXT("true") : TEXT("false"), GroundStrikeProbability, LightningRepeatDelaySeconds);
}

void UStormLightningComponent::SetLightningSequenceSettings(const FStormLightningSequenceSettings& InSettings)
{
	SequenceSettings = InSettings;

	for (float& GroundStrikeTime : SequenceSettings.GroundStrikeTimes01)
	{
		GroundStrikeTime = FMath::Clamp(GroundStrikeTime, 0.0f, 1.0f);
	}

	for (FStormLightningCue& Cue : SequenceSettings.CustomCues)
	{
		Cue.Time01 = FMath::Clamp(Cue.Time01, 0.0f, 1.0f);
	}

	SequenceSettings.StrikeRadiusMinN = FMath::Clamp(SequenceSettings.StrikeRadiusMinN, 0.0f, 1.0f);
	SequenceSettings.StrikeRadiusMaxN = FMath::Clamp(SequenceSettings.StrikeRadiusMaxN, SequenceSettings.StrikeRadiusMinN, 1.0f);

	FStormLightningMaterialState& Material = SequenceSettings.MaterialState;
	Material.FlashExtentN.X                = FMath::Max(0.0, Material.FlashExtentN.X);
	Material.FlashExtentN.Y                = FMath::Max(0.0, Material.FlashExtentN.Y);
	Material.FlashExtentN.Z                = FMath::Max(0.0, Material.FlashExtentN.Z);
	Material.HaloExtentN.X                 = FMath::Max(0.0, Material.HaloExtentN.X);
	Material.HaloExtentN.Y                 = FMath::Max(0.0, Material.HaloExtentN.Y);
	Material.HaloExtentN.Z                 = FMath::Max(0.0, Material.HaloExtentN.Z);
	Material.CorePeakHDR                   = FMath::Max(0.0f, Material.CorePeakHDR);
	Material.FillIntensity                 = FMath::Max(0.0f, Material.FillIntensity);
	Material.LeakIntensity                 = FMath::Max(0.0f, Material.LeakIntensity);

	if (!SequenceSettings.bEnabled)
	{
		StopLightning();
	}
}

void UStormLightningComponent::SetGroundStrikeProbability(float InProbability)
{
	GroundStrikeProbability = FMath::Clamp(InProbability, 0.0f, 1.0f);
}

void UStormLightningComponent::SetLightningRepeatDelay(float InDelaySeconds)
{
	LightningRepeatDelaySeconds = FMath::Max(0.1f, InDelaySeconds);
}

void UStormLightningComponent::PlayLightningSequence()
{
	if (!SequenceSettings.bEnabled)
	{
		return;
	}

	if (!bRuntimeInitialized)
	{
		bRuntimeInitialized = ResolveRuntimeBindings();
		if (!bRuntimeInitialized)
		{
			return;
		}
		InitializeSequenceRandom();
	}

	CancelRepeatTimer();

	if (bSequenceActive)
	{
		UploadPulse(0.0f);
		bSequenceActive = false;
	}

	BeginLightningSequence();
}

void UStormLightningComponent::StopLightning()
{
	CancelRepeatTimer();

	bRuntimeInitialized      = false;
	bSequenceActive          = false;
	bGroundStrikeQueued      = false;
	NextGroundStrikeCueIndex = 0;
	NextCustomCueIndex       = 0;
	SequenceTime             = 0.0f;
	CurrentPulse             = 0.0f;

	SetComponentTickEnabled(false);

	if (UStormMaterialBinderComponent* Binder = MaterialBinder.Get())
	{
		Binder->ClearLightningMaterialState();
	}
}

void UStormLightningComponent::BeginPlay()
{
	Super::BeginPlay();
	StormOwner = Cast<AVolumetricSuperStormActor>(GetOwner());
}

void UStormLightningComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopLightning();
	Super::EndPlay(EndPlayReason);
}

void UStormLightningComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bSequenceActive || SequenceDuration <= SMALL_NUMBER)
	{
		SetComponentTickEnabled(false);
		return;
	}

	SequenceTime = FMath::Min(SequenceTime + FMath::Max(0.0f, DeltaTime), SequenceDuration);

	UploadPulse(EvaluatePulse(SequenceTime));
	const float NormalizedTime = SequenceDuration > SMALL_NUMBER ? SequenceTime / SequenceDuration : 1.0f;
	DispatchCuesUpTo(NormalizedTime);

	if (SequenceTime >= SequenceDuration)
	{
		FinishLightningSequence();
	}
}

bool UStormLightningComponent::ResolveRuntimeBindings()
{
	AVolumetricSuperStormActor* Owner = Cast<AVolumetricSuperStormActor>(GetOwner());
	StormOwner                        = Owner;

	if (!Owner)
	{
		MaterialBinder.Reset();
		return false;
	}

	if (!MaterialBinder.IsValid())
	{
		MaterialBinder = Owner->GetMaterialBinder();
	}

	return MaterialBinder.IsValid();
}

void UStormLightningComponent::InitializeSequenceRandom()
{
	uint32 BaseHash = 0;
	if (const AVolumetricSuperStormActor* Owner = StormOwner.Get())
	{
		const int32 StableId = Owner->GetStableStormId();
		BaseHash             = StableId > 0 ? GetTypeHash(StableId) : GetTypeHash(Owner->GetPathName());
	}
	else
	{
		BaseHash = GetTypeHash(GetPathName());
	}

	SequenceRandom.Initialize(static_cast<int32>((BaseHash ^ 0x6A09E667u) & 0x7fffffffu));
	SequenceIndex = 0;
}

void UStormLightningComponent::BeginLightningSequence()
{
	if (!bRuntimeInitialized || !SequenceSettings.bEnabled)
	{
		return;
	}

	++SequenceIndex;
	const float GroundStrikeRoll = SequenceRandom.FRand();
	CurrentSequenceSeed          = static_cast<int32>(SequenceRandom.GetUnsignedInt() & 0x7fffffffu);

	bGroundStrikeQueued = GroundStrikeRoll < GroundStrikeProbability;
	PickSequenceStrikeCenter();

	CurveStartTime               = 0.0f;
	SequenceDuration             = StormLightning::FallbackSequenceDuration;
	const FRichCurve* FlashCurve = SequenceSettings.FlashPulseCurve.GetRichCurveConst();
	if (FlashCurve && FlashCurve->GetNumKeys() > 0)
	{
		float CurveEndTime = 0.0f;
		FlashCurve->GetTimeRange(CurveStartTime, CurveEndTime);
		SequenceDuration = FMath::Max(CurveEndTime - CurveStartTime, SMALL_NUMBER);
	}

	PrepareRuntimeCues();
	SequenceTime    = 0.0f;
	bSequenceActive = true;

	ApplyLightningMaterialState();
	CurrentPulse = EvaluatePulse(0.0f);
	OnLightningStarted.Broadcast(MakeEventContext());
	UploadPulse(CurrentPulse);
	DispatchCuesUpTo(0.0f);
	SetComponentTickEnabled(true);

	UE_LOG(LogStormLightning, Verbose, TEXT( "Sequence %d started for %s. GroundRoll=%.3f " "GroundQueued=%s Duration=%.2fs RepeatDelay=%.2fs."), SequenceIndex, *GetPathName(), GroundStrikeRoll, bGroundStrikeQueued ? TEXT("true") : TEXT("false"), SequenceDuration, LightningRepeatDelaySeconds);
}

void UStormLightningComponent::FinishLightningSequence()
{
	UploadPulse(0.0f);
	OnLightningFinished.Broadcast(MakeEventContext());

	bSequenceActive     = false;
	bGroundStrikeQueued = false;
	SetComponentTickEnabled(false);

	if (bRuntimeInitialized && SequenceSettings.bEnabled && SequenceSettings.bLoop)
	{
		ScheduleNextSequence();
	}
}

void UStormLightningComponent::ScheduleNextSequence()
{
	CancelRepeatTimer();

	UWorld* World = GetWorld();
	if (!bRuntimeInitialized || !SequenceSettings.bEnabled || !SequenceSettings.bLoop || !World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(NextSequenceTimer, this, &UStormLightningComponent::BeginLightningSequence, LightningRepeatDelaySeconds, false);
}

void UStormLightningComponent::CancelRepeatTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(NextSequenceTimer);
	}
	else
	{
		NextSequenceTimer.Invalidate();
	}
}

void UStormLightningComponent::PrepareRuntimeCues()
{
	RuntimeGroundStrikeTimes01 = SequenceSettings.GroundStrikeTimes01;
	for (float& GroundStrikeTime : RuntimeGroundStrikeTimes01)
	{
		GroundStrikeTime = FMath::Clamp(GroundStrikeTime, 0.0f, 1.0f);
	}
	RuntimeGroundStrikeTimes01.Sort();

	RuntimeCustomCues = SequenceSettings.CustomCues;
	for (FStormLightningCue& Cue : RuntimeCustomCues)
	{
		Cue.Time01 = FMath::Clamp(Cue.Time01, 0.0f, 1.0f);
	}
	RuntimeCustomCues.StableSort(
		[](const FStormLightningCue& A, const FStormLightningCue& B)
		{
			return A.Time01 < B.Time01;
		}
	);

	NextGroundStrikeCueIndex = 0;
	NextCustomCueIndex       = 0;
}

void UStormLightningComponent::PickSequenceStrikeCenter()
{
	// Interpolating the squared radii keeps strikes uniform across the ring's area; lerping the
	// radius itself would crowd them against the inner edge.
	const float   MinRadius      = FMath::Clamp(SequenceSettings.StrikeRadiusMinN, 0.0f, 1.0f);
	const float   MaxRadius      = FMath::Clamp(SequenceSettings.StrikeRadiusMaxN, MinRadius, 1.0f);
	const float   Angle          = SequenceRandom.FRand() * UE_TWO_PI;
	const float   RadiusFraction = FMath::Sqrt(FMath::Lerp(MinRadius * MinRadius, MaxRadius * MaxRadius, SequenceRandom.FRand()));
	const FVector CenterN        = SequenceSettings.MaterialState.CenterN;

	SequenceStrikeCenterN = FVector(
		CenterN.X + RadiusFraction * FMath::Cos(Angle),
		CenterN.Y + RadiusFraction * FMath::Sin(Angle),
		CenterN.Z);
}

void UStormLightningComponent::DispatchCuesUpTo(const float NormalizedTime)
{
	const float SafeTime = FMath::Clamp(NormalizedTime, 0.0f, 1.0f);

	while (NextGroundStrikeCueIndex < RuntimeGroundStrikeTimes01.Num() && RuntimeGroundStrikeTimes01[NextGroundStrikeCueIndex] <= SafeTime + KINDA_SMALL_NUMBER)
	{
		++NextGroundStrikeCueIndex;
		if (bGroundStrikeQueued)
		{
			TriggerGroundStrike();
		}
	}

	while (NextCustomCueIndex < RuntimeCustomCues.Num() && RuntimeCustomCues[NextCustomCueIndex].Time01 <= SafeTime + KINDA_SMALL_NUMBER)
	{
		const FStormLightningCue Cue = RuntimeCustomCues[NextCustomCueIndex];
		++NextCustomCueIndex;
		TriggerCustomCue(Cue);
	}
}

void UStormLightningComponent::TriggerCustomCue(const FStormLightningCue& Cue)
{
	if (!Cue.CueId.IsNone())
	{
		OnCustomLightningCue.Broadcast(Cue.CueId, MakeEventContext());
	}
}

void UStormLightningComponent::TriggerGroundStrike()
{
	const FVector StartWorld         = FlashCenterNToWorld(GetGroundStrikeCenterN());
	FVector       GroundWorld        = FVector::ZeroVector;
	bool          bUsedFallbackPlane = false;
	if (!ResolveGroundStrikeTarget(StartWorld, GroundWorld, bUsedFallbackPlane))
	{
		return;
	}

	if (bUsedFallbackPlane && !bGroundFallbackReported)
	{
		bGroundFallbackReported = true;
		UE_LOG(LogStormLightning, Log, TEXT( "No queryable ground was loaded below %s. Using the world ground " "plane at Z=%.1f; load collision or a Landscape under the storm " "for terrain-accurate strikes."), *GetPathName(), GroundWorld.Z);
	}

	OnGroundStrike.Broadcast(StartWorld, GroundWorld, MakeEventContext());
}

FVector UStormLightningComponent::GetGroundStrikeCenterN() const
{
	const double StrikeX  = SequenceStrikeCenterN.X;
	const double StrikeY  = SequenceStrikeCenterN.Y;
	const float  Radius01 = static_cast<float>(FMath::Clamp(FMath::Sqrt(StrikeX * StrikeX + StrikeY * StrikeY), 0.0, 1.0));

	// The root sits at the authored flash height so the glow wraps it, held inside the storm's own
	// vertical band at that radius so it can never start below the cloud base or above the top.
	double RootHeight01 = SequenceStrikeCenterN.Z + SequenceSettings.RootHeightOffsetN;
	if (const AVolumetricSuperStormActor* Owner = StormOwner.Get())
	{
		const FStormShapeCurves& Curves      = Owner->GetStormRenderDataRef().Shape.ShapeCurves;
		const FRichCurve*        BottomCurve = Curves.LayerBottom.GetRichCurveConst();
		const FRichCurve*        TopCurve    = Curves.LayerTop.GetRichCurveConst();
		if (BottomCurve && TopCurve && BottomCurve->GetNumKeys() > 0 && TopCurve->GetNumKeys() > 0)
		{
			const double Bottom01 = FMath::Clamp(BottomCurve->Eval(Radius01), 0.0f, 1.0f);
			const double Top01    = FMath::Max(static_cast<double>(TopCurve->Eval(Radius01)), Bottom01);
			RootHeight01          = FMath::Clamp(RootHeight01, Bottom01, Top01);
		}
	}

	return FVector(StrikeX, StrikeY, RootHeight01);
}

FVector UStormLightningComponent::FlashCenterNToWorld(const FVector& CenterN) const
{
	const AVolumetricSuperStormActor* Owner = StormOwner.Get();
	if (!Owner)
	{
		return GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	}

	const FStormRenderData& RenderData    = Owner->GetStormRenderDataRef();
	const FVector           ActorLocation = Owner->GetActorLocation();
	const float             Radius        = FMath::Max(1.0f, RenderData.Shape.Radius);
	FVector                 Result(ActorLocation.X + CenterN.X * Radius, ActorLocation.Y + CenterN.Y * Radius, ActorLocation.Z);

	const UStormMaterialBinderComponent* Binder = MaterialBinder.Get();
	const UVolumetricCloudComponent*     Cloud  = Binder ? Binder->ResolveTargetCloud() : nullptr;
	if (Cloud)
	{
		const float LayerBottomCm = Cloud->LayerBottomAltitude * StormLightning::KilometersToCentimeters;
		const float LayerHeightCm = FMath::Max(1.0f, Cloud->LayerHeight * StormLightning::KilometersToCentimeters);
		Result.Z                  = LayerBottomCm + FMath::Clamp(CenterN.Z, 0.0, 1.0) * LayerHeightCm;
	}
	else
	{
		const float LayerHeightCm = FMath::Max(1.0f, FMath::Abs(static_cast<float>(RenderData.WorldExtent.Z)));
		const float LayerBottomCm = static_cast<float>(RenderData.WorldCenter.Z) - LayerHeightCm * 0.5f;
		Result.Z                  = LayerBottomCm + FMath::Clamp(CenterN.Z, 0.0, 1.0) * LayerHeightCm;
	}

	return Result;
}

bool UStormLightningComponent::ResolveGroundStrikeTarget(const FVector& StrikeStartWorld, FVector& OutGroundWorld, bool& bOutUsedFallbackPlane) const
{
	bOutUsedFallbackPlane = false;

	if (FindGroundPoint(StrikeStartWorld, OutGroundWorld))
	{
		return true;
	}

	OutGroundWorld        = MakeFallbackGroundPoint(StrikeStartWorld);
	bOutUsedFallbackPlane = true;
	return true;
}

bool UStormLightningComponent::FindGroundPoint(const FVector& StartWorld, FVector& OutGroundWorld) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector TraceEnd = StartWorld - FVector::UpVector * StormLightning::GroundTraceDistance;

	FCollisionObjectQueryParams ObjectParams(FCollisionObjectQueryParams::InitType::AllStaticObjects);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FHitResult BestHit;
	bool       bFoundCollisionSurface = false;
	const auto TryObjectTrace         = [&](const bool bTraceComplex)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(StormLightningGroundTrace), bTraceComplex, GetOwner());

		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, StartWorld, TraceEnd, ObjectParams, Params) && (!bFoundCollisionSurface || Hit.Distance < BestHit.Distance))
		{
			BestHit                = Hit;
			bFoundCollisionSurface = true;
		}
	};

	TryObjectTrace(false);
	TryObjectTrace(true);
	if (bFoundCollisionSurface)
	{
		OutGroundWorld = BestHit.ImpactPoint;
		return true;
	}

	TOptional<float> BestLandscapeHeight;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		const ALandscapeProxy* Landscape = *It;
		TOptional<float>       Height    = Landscape->GetHeightAtLocation(StartWorld, EHeightfieldSource::Complex);
		if (!Height.IsSet())
		{
			Height = Landscape->GetHeightAtLocation(StartWorld, EHeightfieldSource::Simple);
		}

		if (!Height.IsSet())
		{
			continue;
		}

		const float CandidateHeight = Height.GetValue();
		if (CandidateHeight <= StartWorld.Z && CandidateHeight >= TraceEnd.Z && (!BestLandscapeHeight.IsSet() || CandidateHeight > BestLandscapeHeight.GetValue()))
		{
			BestLandscapeHeight = CandidateHeight;
		}
	}

	if (!BestLandscapeHeight.IsSet())
	{
		return false;
	}

	OutGroundWorld   = StartWorld;
	OutGroundWorld.Z = BestLandscapeHeight.GetValue();
	return true;
}

FVector UStormLightningComponent::MakeFallbackGroundPoint(const FVector& StartWorld) const
{
	// Cloud layer altitudes are already read as heights above world Z 0, so that plane is the only
	// ground the strike start agrees with. The Actor Z only places the storm and collapses the bolt
	// to MinimumFallbackBoltLength whenever an artist lifts the Actor into the cloud layer.
	const double LowestAllowedZ  = StartWorld.Z - static_cast<double>(StormLightning::GroundTraceDistance);
	const double HighestAllowedZ = StartWorld.Z - static_cast<double>(StormLightning::MinimumFallbackBoltLength);

	FVector Result = StartWorld;
	Result.Z       = FMath::Clamp(0.0, LowestAllowedZ, HighestAllowedZ);
	return Result;
}

void UStormLightningComponent::ApplyLightningMaterialState()
{
	UStormMaterialBinderComponent* Binder = MaterialBinder.Get();
	if (!Binder)
	{
		return;
	}

	// The glow has to sit over the bolt, so both read the same sequence strike center.
	FStormLightningMaterialState State = SequenceSettings.MaterialState;
	State.CenterN                      = SequenceStrikeCenterN;
	Binder->ApplyLightningMaterialState(State);
}

void UStormLightningComponent::UploadPulse(float Pulse)
{
	const float SafePulse = FMath::Max(0.0f, Pulse);
	CurrentPulse          = SafePulse;

	if (UStormMaterialBinderComponent* Binder = MaterialBinder.Get())
	{
		Binder->SetLightningPulse(SafePulse);
	}

	if (bSequenceActive && OnLightningPulseUpdated.IsBound())
	{
		OnLightningPulseUpdated.Broadcast(MakeEventContext());
	}
}

float UStormLightningComponent::EvaluatePulse(float SequenceTimeSeconds) const
{
	const float       SafeTime   = FMath::Clamp(SequenceTimeSeconds, 0.0f, SequenceDuration);
	const FRichCurve* FlashCurve = SequenceSettings.FlashPulseCurve.GetRichCurveConst();
	if (FlashCurve && FlashCurve->GetNumKeys() > 0)
	{
		return FMath::Max(0.0f, FlashCurve->Eval(CurveStartTime + SafeTime));
	}

	const float NormalizedTime = SequenceDuration > SMALL_NUMBER ? SafeTime / SequenceDuration : 1.0f;
	return StormLightning::SampleFallbackPulse(NormalizedTime);
}

FStormLightningContext UStormLightningComponent::MakeEventContext() const
{
	FStormLightningContext Context;
	Context.SequenceIndex  = SequenceIndex;
	Context.SequenceSeed   = CurrentSequenceSeed;
	Context.SequenceTime   = SequenceTime;
	Context.NormalizedTime = SequenceDuration > SMALL_NUMBER ? FMath::Clamp(SequenceTime / SequenceDuration, 0.0f, 1.0f) : 0.0f;
	Context.Pulse          = CurrentPulse;
	return Context;
}
