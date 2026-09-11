// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file VolumetricSuperStormActor.h
 * @brief Defines the single supported storm actor and its public controls.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/StormLightningTypes.h"
#include "Data/StormRenderData.h"
#include "Data/StormRenderUpdateTypes.h"
#include "Data/StormTypes.h"
#include "GameFramework/Actor.h"
#include "VolumetricSuperStormActor.generated.h"

struct FStormPresetData;

class UStormMaterialBinderComponent;
class UStormLightningComponent;
class UStormFlowMapComponent;
class UStormVerticalProfileToolComponent;
class UStormPresetDataAsset;
class UStormVerticalProfileAsset;
class USceneComponent;
class UTexture;
class UTexture2D;
class UBillboardComponent;

class AVolumetricSuperStormActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStormFormationCompletedSignature);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStormDissolutionCompletedSignature);

/**
 * @brief Rotation state of a single motion ring.
 * @details The wrapped angle alone loses how many turns have elapsed, which the shader
 * needs to apply a speed multiplier before wrapping. Keeping the turn count preserves
 * the unwrapped phase as WrappedRadians + TurnCount * 2pi.
 */
struct FStormRingPhase
{
	double WrappedRadians = 0.0;
	int64  TurnCount      = 0;
};

/**
 * @brief Coordinates the only storm supported by a world.
 * @details Owns shape, motion, profile, flow-map, lightning, and material-binding
 * state while the world subsystem owns the shared shape render targets.
 */
UCLASS(Blueprintable)
class VOLUMETRICSUPERSTORMRUNTIME_API AVolumetricSuperStormActor : public AActor
{
	GENERATED_BODY()

public:
	AVolumetricSuperStormActor(const FObjectInitializer& ObjectInitializer);

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Destroyed() override;

	/** @brief Returns the live bottom profile texture. */
	UTexture* GetBottomTypeProfileTexture() const;
	/** @brief Returns the live top profile texture. */
	UTexture* GetTopTypeProfileTexture() const;

	/** @brief Rebuilds and publishes the render-data subset selected by the caller. */
	void RequestRenderDataUpdate(EStormRenderUpdateScope Scope);

	/** @brief Queues changed texture contents without rebuilding render data. */
	void RequestTextureContentUpdate(EStormTextureDirtyFlags DirtyTextures);

	/** @brief Applies a complete preset and rebuilds runtime state. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Preset")
	void ApplyPreset(UStormPresetDataAsset* InPreset);

	/** @brief Registers this actor as the world storm and initializes render resources. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Render")
	void PrepareStormResources();

	/** @brief Starts the authored formation sequence. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Formation", meta = (DisplayName = "Create Storm"))
	void StartFormationAnimation();

	/** @brief Starts the authored dissolution sequence. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Formation", meta = (DisplayName = "Dissolve Storm"))
	void StartDissolutionAnimation();

	/** @brief Sets the sanitized formation duration. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Formation")
	void SetFormationAnimationDuration(UPARAM(meta = (ClampMin = "0.1")) float DurationSeconds);

	/** @brief Sets the sanitized dissolution duration. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Formation")
	void SetDissolutionAnimationDuration(UPARAM(meta = (ClampMin = "0.1")) float DurationSeconds);

	/** @brief Returns the formation duration in seconds. */
	UFUNCTION(BlueprintPure, Category = "Storm|Formation")
	float GetFormationAnimationDuration() const
	{
		return FormationDurationSeconds;
	}

	/** @brief Returns the dissolution duration in seconds. */
	UFUNCTION(BlueprintPure, Category = "Storm|Formation")
	float GetDissolutionAnimationDuration() const
	{
		return DissolutionDurationSeconds;
	}

	/** @brief Immediately switches the storm to its mature state. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Formation")
	void ShowMatureStorm();

	/** @brief Immediately switches the storm to its hidden state. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Formation")
	void HideStormImmediately();

	/** @brief Copies the current configuration into preset data. */
	void BuildPresetData(FStormPresetData& OutPreset) const;

	/** @brief Rebuilds static and frame render data and refreshes the cloud material. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Render")
	void RebuildRenderData();

	/** @brief Returns the explicit or deterministic resolved storm identifier. */
	UFUNCTION(BlueprintPure, Category = "Storm|Identity")
	int32 GetStableStormId() const;

	/** @brief Returns a Blueprint-safe copy of the current render data. */
	UFUNCTION(BlueprintPure, Category = "Storm|Render")
	FStormRenderData GetStormRenderData() const;
	/** @brief Returns the current render data without copying. */
	const FStormRenderData& GetStormRenderDataRef() const;

	/** @brief Returns the cloud material binder component. */
	UFUNCTION(BlueprintPure, Category = "Storm|Components")
	UStormMaterialBinderComponent* GetMaterialBinder() const;

	/** @brief Returns the vertical profile component. */
	UFUNCTION(BlueprintPure, Category = "Storm|Components")
	UStormVerticalProfileToolComponent* GetVerticalProfileTool() const;

	/** @brief Returns the flow-map component. */
	UFUNCTION(BlueprintPure, Category = "Storm|Components")
	UStormFlowMapComponent* GetFlowMapComponent() const;

	/** @brief Returns the lightning component. */
	UFUNCTION(BlueprintPure, Category = "Storm|Components")
	UStormLightningComponent* GetLightningComponent() const;

	/** @brief Starts the configured lightning sequence. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Lightning")
	void PlayLightningSequence();

	/** @brief Stops lightning playback and clears material lightning state. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Lightning")
	void StopLightning();

	/** @brief Returns whether the lightning sequence is active. */
	UFUNCTION(BlueprintPure, Category = "Storm|Lightning")
	bool IsLightningSequencePlaying() const;

	/** @brief Replaces the lightning sequence. Playback picks it up on the next sequence. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Lightning")
	void SetLightningSequence(const FStormLightningSequenceSettings& InSequence);

	/** @brief Sets the delay between automatically repeated lightning sequences. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Lightning")
	void SetLightningRepeatDelay(float InDelaySeconds);

	/** @brief Sets the probability that a sequence also produces a ground strike. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Lightning")
	void SetGroundStrikeProbability(float InProbability);

	/** Starts or resumes forward playback of the active vertical-profile sequence. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	bool PlayProfileSequence(bool bRestart = false);

	/** Starts or resumes reverse playback from the sequence end when needed. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	bool PlayProfileSequenceReverse(bool bRestart = false);

	/** Pauses playback while keeping the composed sequence preview visible. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	void PauseProfileSequence();

	/** Stops playback and returns rendering to the active authoring profile. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	void StopProfileSequence();

	/** Scrubs to a timeline position and leaves the sequence preview paused. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	bool SetProfileSequenceTime(float InTimeSeconds);

	/** Overrides the asset's default looping behavior. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Profile|Sequence")
	void SetProfileSequenceLooping(bool bLooping);

	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	float GetProfileSequenceTime() const;

	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	float GetProfileSequenceDuration() const;

	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	bool IsProfileSequencePlaying() const;

	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	bool IsProfileSequencePlayingInReverse() const;

	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	bool IsProfileSequencePreviewActive() const;

	UFUNCTION(BlueprintPure, Category = "Storm|Profile|Sequence")
	bool CanPlayProfileSequence() const;

	/** @brief Enables or disables shader motion. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Motion")
	void SetStormMotionEnabled(bool bEnabled);

	/** @brief Pauses motion time integration. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Motion")
	void PauseStormMotion();

	/** @brief Resumes motion time integration. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Motion")
	void ResumeStormMotion();

	/** @brief Resets motion time and ring phases. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Motion")
	void ResetStormMotion();

	/** @brief Sets deterministic motion time in seconds. */
	UFUNCTION(BlueprintCallable, Category = "Storm|Motion")
	void SetStormMotionTime(float TimeSeconds);

#if WITH_EDITOR

	bool SetMotionRingEndRadius01ForEditor(int32 BoundaryIndex, float NewRadius01);

	void CommitMotionRingEndRadiiEditForEditor();
#endif

	UPROPERTY(BlueprintAssignable, Category = "Storm|Formation|Events")
	FStormFormationCompletedSignature OnStormFormationCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Storm|Formation|Events")
	FStormDissolutionCompletedSignature OnStormDissolutionCompleted;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** @brief Receives the Blueprint lightning-start event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Storm|Lightning|Events", meta = (DisplayName = "On Lightning Started"))
	void ReceiveLightningStarted(const FStormLightningContext& Context);

	/** @brief Receives the Blueprint ground-strike event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Storm|Lightning|Events", meta = (DisplayName = "On Ground Strike"))
	void ReceiveGroundStrike(const FVector& StrikeStartWorld, const FVector& StrikeEndWorld, const FStormLightningContext& Context);

	/** @brief Receives a Blueprint custom lightning cue. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Storm|Lightning|Events", meta = (DisplayName = "On Custom Lightning Cue"))
	void ReceiveCustomLightningCue(FName CueId, const FStormLightningContext& Context);

	/** @brief Receives the Blueprint lightning-finished event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Storm|Lightning|Events", meta = (DisplayName = "On Lightning Finished"))
	void ReceiveLightningFinished(const FStormLightningContext& Context);

	/** @brief Receives the Blueprint formation-completed event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Storm|Formation|Events", meta = (DisplayName = "On Storm Formation Completed"))
	void ReceiveStormFormationCompleted();

	/** @brief Receives the Blueprint dissolution-completed event. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Storm|Formation|Events", meta = (DisplayName = "On Storm Dissolution Completed"))
	void ReceiveStormDissolutionCompleted();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool ShouldTickIfViewportsOnly() const override;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Storm|Advanced")
	TObjectPtr<UBillboardComponent> EditorIcon;
#endif

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Storm|Advanced")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Storm|Advanced")
	TObjectPtr<UStormMaterialBinderComponent> MaterialBinder;

	UPROPERTY(VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Storm|Advanced")
	TObjectPtr<UStormLightningComponent> LightningComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Storm|Advanced")
	TObjectPtr<UStormVerticalProfileToolComponent> VerticalProfileTool;

	UPROPERTY(BlueprintReadOnly, Category = "Storm|Advanced")
	TObjectPtr<UStormFlowMapComponent> FlowMapComponent;

	/** @brief Stable seed shared by motion and lightning; zero derives one from the actor name. */
	UPROPERTY(EditInstanceOnly, AdvancedDisplay, BlueprintReadOnly, Category = "Storm|Advanced", meta = (ClampMin = "0"))
	int32 StableStormId = 0;

	/** @brief Editable shape and appearance configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Shape", meta = (ShowOnlyInnerProperties))
	FStormShapeSettings ShapeSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Lightning", meta = ( DisplayName = "Lightning Sequence", ShowOnlyInnerProperties))
	FStormLightningSequenceSettings LightningSequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Lightning", meta = ( DisplayName = "Lightning Repeat Delay", ClampMin = "0.1", UIMin = "0.1", UIMax = "10.0", Delta = "0.1", Units = "s"))
	float LightningRepeatDelaySeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Storm|Lightning", meta = ( DisplayName = "Ground Strike Probability", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", Delta = "0.05"))
	float GroundStrikeProbability = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Animation", meta = (DisplayName = "Create Animation Duration", ClampMin = "0.1", UIMin = "0.1", UIMax = "60.0", Units = "s"))
	float FormationDurationSeconds = 13.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Animation", meta = (DisplayName = "Dissolve Animation Duration", ClampMin = "0.1", UIMin = "0.1", UIMax = "60.0", Units = "s"))
	float DissolutionDurationSeconds = 10.0f;

	/** @brief Editable ring-motion configuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Storm|Motion", meta = (ShowOnlyInnerProperties))
	FStormMotionSettings MotionSettings;

	UPROPERTY(Transient)
	FStormRenderData CachedRenderData;

private:
	void ApplyPresetConfiguration(UStormPresetDataAsset* InPreset, bool bRebuildRenderData);

	UStormLightningComponent* ConfigureLightningComponent();
	void                      BindLightningEvents();
	void                      UnbindLightningEvents();

	/** @brief Forwards lightning-start state to the actor event surface. */
	UFUNCTION()
	void HandleLightningStarted(const FStormLightningContext& Context);

	/** @brief Forwards a resolved ground strike to the actor event surface. */
	UFUNCTION()
	void HandleGroundStrike(const FVector& StrikeStartWorld, const FVector& StrikeEndWorld, const FStormLightningContext& Context);

	/** @brief Forwards a custom lightning cue to the actor event surface. */
	UFUNCTION()
	void HandleCustomLightningCue(FName CueId, const FStormLightningContext& Context);

	/** @brief Forwards lightning completion to the actor event surface. */
	UFUNCTION()
	void HandleLightningFinished(const FStormLightningContext& Context);

#if WITH_EDITOR

	void CreateEditorIconComponent();
#endif

	bool  RegisterWithRenderSubsystem();
	void  UnregisterFromRenderSubsystem();
	int32 ResolveStableStormId() const;
	void  PublishRenderDataUpdate(EStormRenderUpdateScope Scope);
	void  UpdateStaticRenderData();
	void  UpdateFrameRenderData();
	void  UpdateProfileRenderData();
	void  UpdateFlowMapRenderData();

	void QueueLifecycleWarmup();
	bool TickLifecycleWarmup();
	bool TickFormationAnimation(float DeltaSeconds);
	void BroadcastFormationCompletionEvents(EStormFormationState PreviousState, EStormFormationState NewState);
	bool TickMotion(float DeltaSeconds);
	void SynchronizeMotionPhaseCount(const FStormMotionSettings& SanitizedMotion);
	void RebuildMotionPhasesFromTime(double InTimeSeconds);
	void NormalizeFormationStateEndpoints();

	EStormFormationState FormationState            = EStormFormationState::Mature;
	float                FormationProgress         = 1.0f;
	bool                 bFormationAnimationActive = false;

	int32 LifecycleWarmupFramesRemaining = 0;
	bool  bLifecycleWarmupComplete       = false;

	double                   MotionElapsedSeconds = 0.0;
	TArray<FStormRingPhase>  RingPhases;
	bool           bMotionPaused                  = false;
	bool           bRegisteredWithRenderSubsystem = false;
};
