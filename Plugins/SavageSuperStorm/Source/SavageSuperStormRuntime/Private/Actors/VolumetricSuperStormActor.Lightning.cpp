/**
 * @file VolumetricSuperStormActor.Lightning.cpp
 * @brief Implements lightning control and event forwarding for the storm actor.
 */

#include "Actors/VolumetricSuperStormActor.h"

#include "Components/StormLightningComponent.h"

void AVolumetricSuperStormActor::PlayLightningSequence()
{
	if (UStormLightningComponent* Lightning = ConfigureLightningComponent())
	{
		Lightning->PlayLightningSequence();
	}
}

void AVolumetricSuperStormActor::StopLightning()
{
	if (UStormLightningComponent* Lightning = LightningComponent.Get())
	{
		Lightning->StopLightning();
	}
}

bool AVolumetricSuperStormActor::IsLightningSequencePlaying() const
{
	const UStormLightningComponent* Lightning = GetLightningComponent();
	return Lightning && Lightning->IsLightningSequencePlaying();
}

UStormLightningComponent* AVolumetricSuperStormActor::ConfigureLightningComponent()
{
	UStormLightningComponent* Lightning = LightningComponent.Get();
	if (!Lightning)
	{
		return nullptr;
	}

	Lightning->SetLightningSequenceSettings(LightningSequence);
	Lightning->SetGroundStrikeProbability(GroundStrikeProbability);
	Lightning->SetLightningRepeatDelay(LightningRepeatDelaySeconds);
	return Lightning;
}

void AVolumetricSuperStormActor::BindLightningEvents()
{
	UStormLightningComponent* Lightning = LightningComponent.Get();
	if (!Lightning)
	{
		return;
	}

	Lightning->OnLightningStarted.AddUniqueDynamic(this, &AVolumetricSuperStormActor::HandleLightningStarted);
	Lightning->OnGroundStrike.AddUniqueDynamic(this, &AVolumetricSuperStormActor::HandleGroundStrike);
	Lightning->OnCustomLightningCue.AddUniqueDynamic(this, &AVolumetricSuperStormActor::HandleCustomLightningCue);
	Lightning->OnLightningFinished.AddUniqueDynamic(this, &AVolumetricSuperStormActor::HandleLightningFinished);
}

void AVolumetricSuperStormActor::UnbindLightningEvents()
{
	UStormLightningComponent* Lightning = LightningComponent.Get();
	if (!Lightning)
	{
		return;
	}

	Lightning->OnLightningStarted.RemoveDynamic(this, &AVolumetricSuperStormActor::HandleLightningStarted);
	Lightning->OnGroundStrike.RemoveDynamic(this, &AVolumetricSuperStormActor::HandleGroundStrike);
	Lightning->OnCustomLightningCue.RemoveDynamic(this, &AVolumetricSuperStormActor::HandleCustomLightningCue);
	Lightning->OnLightningFinished.RemoveDynamic(this, &AVolumetricSuperStormActor::HandleLightningFinished);
}

void AVolumetricSuperStormActor::HandleLightningStarted(const FStormLightningContext& Context)
{
	ReceiveLightningStarted(Context);
}

void AVolumetricSuperStormActor::HandleGroundStrike(const FVector& StrikeStartWorld, const FVector& StrikeEndWorld, const FStormLightningContext& Context)
{
	ReceiveGroundStrike(StrikeStartWorld, StrikeEndWorld, Context);
}

void AVolumetricSuperStormActor::HandleCustomLightningCue(const FName CueId, const FStormLightningContext& Context)
{
	ReceiveCustomLightningCue(CueId, Context);
}

void AVolumetricSuperStormActor::HandleLightningFinished(const FStormLightningContext& Context)
{
	ReceiveLightningFinished(Context);
}