/**
 * @file SSSActorDetailCustomization.h
 * @brief Declares the storm actor Details customization.
 */

#pragma once
#include "IDetailCustomization.h"
#include "Input/Reply.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AVolumetricSuperStormActor;

class FSSSActorDetailCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance()
	{
		return MakeShared<FSSSActorDetailCustomization>();
	}

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	FReply                                     OnEditClicked();
	FReply                                     OnFormationClicked();
	FReply                                     OnDissolutionClicked();
	FReply                                     OnEditFlowMapClicked();
	FReply                                     OnSavePresetAsClicked();
	FReply                                     OnLoadPresetClicked();
	TWeakObjectPtr<AVolumetricSuperStormActor> WeakActor;
};