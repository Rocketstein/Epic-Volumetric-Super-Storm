#pragma once

#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"

class UStormVerticalProfileToolComponent;
class UStormFlowMapComponent;
class AVolumetricSuperStormActor;
class FComponentVisualizer;
class SWindow;
class SStormFlowMapEditor;
class SStormProfilePainter;

class FSavageSuperStormEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    // Opens the profile painter window for the given component. If a window is
    // already open, brings it to front and retargets it to this component.
    void OpenProfilePainter(UStormVerticalProfileToolComponent* Component);
	void OpenFlowMapPainter(UStormFlowMapComponent* Component);
	// One-shot preset I/O for the actor. Save captures the actor's current settings into a
	// chosen asset; Load applies a chosen asset onto the actor. Neither stores a reference.
	void SavePresetAs(AVolumetricSuperStormActor* Actor);
	void LoadPresetInto(AVolumetricSuperStormActor* Actor);

private:
	/** Guards live profile edits before a preset replaces the actor's profile document. */
	bool PrepareToReplaceProfileDocument(
		UStormVerticalProfileToolComponent* Tool);

	TSharedPtr<FComponentVisualizer> StormMotionComponentVisualizer;
    TWeakPtr<SWindow> ActiveWindow;
    TWeakPtr<SStormProfilePainter> ActivePainter;
	TWeakPtr<SWindow> ActiveFlowMapWindow;
	TWeakPtr<SStormFlowMapEditor> ActiveFlowMapPainter;
};
