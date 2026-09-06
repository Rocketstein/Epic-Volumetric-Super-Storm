/**
 * @file StormLightningComponent.h
 * @brief Declares the storm lightning sequence component.
 */

#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Components/ActorComponent.h"
#include "Data/StormLightningTypes.h"
#include "StormLightningComponent.generated.h"

class AVolumetricSuperStormActor;
class UStormMaterialBinderComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStormLightningStartedSignature, const FStormLightningContext&, Context);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStormLightningCustomCueSignature, FName, CueId, const FStormLightningContext&, Context);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FStormGroundStrikeSignature, const FVector&, StrikeStartWorld, const FVector&, StrikeEndWorld, const FStormLightningContext&, Context);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStormLightningPulseUpdatedSignature, const FStormLightningContext&, Context);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStormLightningFinishedSignature, const FStormLightningContext&, Context);

/**
 * @brief Evaluates lightning sequences and updates material flash state.
 */
UCLASS(ClassGroup = (Storm), BlueprintType, meta = (BlueprintSpawnableComponent))

class SAVAGESUPERSTORMRUNTIME_API UStormLightningComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UStormLightningComponent();

	void InitializeRuntime(UStormMaterialBinderComponent* InMaterialBinder);

	void SetLightningSequenceSettings(const FStormLightningSequenceSettings& InSettings);

	void SetGroundStrikeProbability(float InProbability);

	void SetLightningRepeatDelay(float InDelaySeconds);

	/** @brief Starts the configured lightning sequence. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Lightning")
	void PlayLightningSequence();

	/** @brief Stops lightning playback and clears material lightning state. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Lightning")
	void StopLightning();

	/** @brief Returns whether the lightning sequence is active. */
	UFUNCTION(BlueprintPure, Category = "Storm|Lightning")
	bool IsLightningSequencePlaying() const
	{
		return bSequenceActive;
	}

	UPROPERTY(BlueprintAssignable, Category = "Storm|Lightning|Events")
	FStormLightningStartedSignature OnLightningStarted;

	UPROPERTY(BlueprintAssignable, Category = "Storm|Lightning|Events")
	FStormLightningCustomCueSignature OnCustomLightningCue;

	UPROPERTY(BlueprintAssignable, Category = "Storm|Lightning|Events")
	FStormGroundStrikeSignature OnGroundStrike;

	UPROPERTY(BlueprintAssignable, Category = "Storm|Lightning|Events", meta = (AdvancedDisplay))
	FStormLightningPulseUpdatedSignature OnLightningPulseUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Storm|Lightning|Events")
	FStormLightningFinishedSignature OnLightningFinished;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool ResolveRuntimeBindings();
	void InitializeSequenceRandom();
	void BeginLightningSequence();
	void FinishLightningSequence();
	void ScheduleNextSequence();
	void CancelRepeatTimer();

	void PrepareRuntimeCues();
	void DispatchCuesUpTo(float NormalizedTime);
	void TriggerCustomCue(const FStormLightningCue& Cue);
	void TriggerGroundStrike();

	FVector GetGroundStrikeCenterN() const;
	FVector FlashCenterNToWorld(const FVector& CenterN) const;
	bool    ResolveGroundStrikeTarget(const FVector& StrikeStartWorld, FVector& OutGroundWorld, bool& bOutUsedFallbackPlane) const;
	bool    FindGroundPoint(const FVector& StartWorld, FVector& OutGroundWorld) const;
	FVector MakeFallbackGroundPoint(const FVector& StartWorld) const;

	void                   ApplyLightningMaterialState();
	void                   UploadPulse(float Pulse);
	float                  EvaluatePulse(float SequenceTimeSeconds) const;
	FStormLightningContext MakeEventContext() const;

	FStormLightningSequenceSettings SequenceSettings;
	TArray<float>                   RuntimeGroundStrikeTimes01;
	TArray<FStormLightningCue>      RuntimeCustomCues;

	TWeakObjectPtr<AVolumetricSuperStormActor>    StormOwner;
	TWeakObjectPtr<UStormMaterialBinderComponent> MaterialBinder;

	FRandomStream SequenceRandom;

	bool bRuntimeInitialized     = false;
	bool bSequenceActive         = false;
	bool bGroundStrikeQueued     = false;
	bool bGroundFallbackReported = false;

	FTimerHandle NextSequenceTimer;

	float CurveStartTime              = 0.0f;
	float SequenceDuration            = 0.0f;
	float SequenceTime                = 0.0f;
	float CurrentPulse                = 0.0f;
	float GroundStrikeProbability     = 1.0f;
	float LightningRepeatDelaySeconds = 2.0f;
	int32 NextGroundStrikeCueIndex    = 0;
	int32 NextCustomCueIndex          = 0;
	int32 SequenceIndex               = 0;
	int32 CurrentSequenceSeed         = 0;
};