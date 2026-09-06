/**
 * @file SSSActorDetailCustomization.cpp
 * @brief Builds the storm actor Details panel actions and grouping.
 */

#include "Customization/SSSActorDetailCustomization.h"
#include "SavageSuperStormEditor.h"
#include "Actors/VolumetricSuperStormActor.h"
#include "Components/StormFlowMapComponent.h"
#include "Components/StormVerticalProfileToolComponent.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "PropertyHandle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SSSActorDetailCustomization"

void FSSSActorDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	if (SelectedObjects.Num() == 1)
	{
		WeakActor = Cast<AVolumetricSuperStormActor>(SelectedObjects[0].Get());
	}

	IDetailCategoryBuilder&           AnimationCategory           = DetailBuilder.EditCategory("Storm|Animation", FText::GetEmpty(), ECategoryPriority::Important);
	const TSharedRef<IPropertyHandle> FormationDurationProperty   = DetailBuilder.GetProperty(TEXT("FormationDurationSeconds"), AVolumetricSuperStormActor::StaticClass());
	const TSharedRef<IPropertyHandle> DissolutionDurationProperty = DetailBuilder.GetProperty(TEXT("DissolutionDurationSeconds"), AVolumetricSuperStormActor::StaticClass());
	AnimationCategory.AddProperty(FormationDurationProperty);
	AnimationCategory.AddProperty(DissolutionDurationProperty);

	AnimationCategory.AddCustomRow(LOCTEXT("FormationFilter", "Create Storm")).WholeRowContent()[SNew(SButton).HAlign(HAlign_Center).OnClicked(this, &FSSSActorDetailCustomization::OnFormationClicked)
	[
		SNew(STextBlock).Text(LOCTEXT("FormationButton", "Create Storm"))
	]];

	AnimationCategory.AddCustomRow(LOCTEXT("DissolutionFilter", "Dissolve Storm")).WholeRowContent()[SNew(SButton).HAlign(HAlign_Center).OnClicked(this, &FSSSActorDetailCustomization::OnDissolutionClicked)
	[
		SNew(STextBlock).Text(LOCTEXT("DissolutionButton", "Dissolve Storm"))
	]];

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory("Storm|Actions", FText::GetEmpty(), ECategoryPriority::Important);
	Category.AddCustomRow(LOCTEXT("EditProfileFilter", "Edit Storm Profile")).NameContent()[SNew(STextBlock).Text(LOCTEXT("PaintProfileLabel", "Edit Storm Profile")).Font(IDetailLayoutBuilder::GetDetailFont())].ValueContent().MinDesiredWidth(180.0f)[SNew(SButton)
	.HAlign(HAlign_Center)
	.IsEnabled_Lambda(
		[this]
		{
			AVolumetricSuperStormActor* Actor = WeakActor.Get();
			return Actor && Actor->FindComponentByClass<UStormVerticalProfileToolComponent>();
		}
	)
	.OnClicked(this, &FSSSActorDetailCustomization::OnEditClicked)
	[
		SNew(STextBlock).Text(LOCTEXT("EditProfileButton", "Edit Storm Profile"))
	]];

	Category.AddCustomRow(LOCTEXT("EditFlowMapFilter", "Edit Storm Flow Map")).NameContent()[SNew(STextBlock).Text(LOCTEXT("PaintFlowMapLabel", "Edit Storm Flow Map")).Font(IDetailLayoutBuilder::GetDetailFont())].ValueContent().MinDesiredWidth(180.0f)[SNew(SButton)
	.HAlign(HAlign_Center)
	.IsEnabled_Lambda(
		[this]
		{
			AVolumetricSuperStormActor* Actor = WeakActor.Get();
			return Actor && Actor->FindComponentByClass<UStormFlowMapComponent>();
		}
	)
	.OnClicked(this, &FSSSActorDetailCustomization::OnEditFlowMapClicked)
	[
		SNew(STextBlock).Text(LOCTEXT("EditFlowMapButton", "Edit Flow Map"))
	]];

	Category.AddCustomRow(LOCTEXT("LoadPresetFilter", "Load Preset")).NameContent()[SNew(STextBlock).Text(LOCTEXT("LoadPresetLabel", "Load Preset")).Font(IDetailLayoutBuilder::GetDetailFont())].ValueContent().MinDesiredWidth(180.0f)[SNew(SButton)
	.HAlign(HAlign_Center)
	.ToolTipText(LOCTEXT("LoadPresetTooltip", "Pick a Storm preset asset and apply its settings to this actor. One-shot: no reference is kept, and the replacement cannot be undone."))
	.IsEnabled_Lambda(
		[this]
		{
			return WeakActor.IsValid();
		}
	)
	.OnClicked(this, &FSSSActorDetailCustomization::OnLoadPresetClicked)
	[
		SNew(STextBlock).Text(LOCTEXT("LoadPresetButton", "Load Preset..."))
	]];

	Category.AddCustomRow(LOCTEXT("SavePresetAsFilter", "Save Preset As")).NameContent()[SNew(STextBlock).Text(LOCTEXT("SavePresetAsLabel", "Save Preset As")).Font(IDetailLayoutBuilder::GetDetailFont())].ValueContent().MinDesiredWidth(180.0f)[SNew(SButton)
	.HAlign(HAlign_Center)
	.ToolTipText(LOCTEXT("SavePresetAsTooltip", "Capture this actor's current settings into a new (or overwritten) Storm preset asset."))
	.IsEnabled_Lambda(
		[this]
		{
			return WeakActor.IsValid();
		}
	)
	.OnClicked(this, &FSSSActorDetailCustomization::OnSavePresetAsClicked)
	[
		SNew(STextBlock).Text(LOCTEXT("SavePresetAsButton", "Save Preset As..."))
	]];
}

FReply FSSSActorDetailCustomization::OnEditClicked()
{
	if (AVolumetricSuperStormActor* Actor = WeakActor.Get())
	{
		if (UStormVerticalProfileToolComponent* Component = Actor->FindComponentByClass<UStormVerticalProfileToolComponent>())
		{
			if (FSavageSuperStormEditorModule* Module = FModuleManager::GetModulePtr<FSavageSuperStormEditorModule>("SavageSuperStormEditor"))
			{
				Module->OpenProfilePainter(Component);
			}
		}
	}
	return FReply::Handled();
}

FReply FSSSActorDetailCustomization::OnFormationClicked()
{
	if (AVolumetricSuperStormActor* Actor = WeakActor.Get())
	{
		Actor->Modify();
		Actor->StartFormationAnimation();
	}
	return FReply::Handled();
}

FReply FSSSActorDetailCustomization::OnDissolutionClicked()
{
	if (AVolumetricSuperStormActor* Actor = WeakActor.Get())
	{
		Actor->Modify();
		Actor->StartDissolutionAnimation();
	}
	return FReply::Handled();
}

FReply FSSSActorDetailCustomization::OnEditFlowMapClicked()
{
	if (AVolumetricSuperStormActor* Actor = WeakActor.Get())
	{
		if (UStormFlowMapComponent* Component = Actor->FindComponentByClass<UStormFlowMapComponent>())
		{
			if (FSavageSuperStormEditorModule* Module = FModuleManager::GetModulePtr<FSavageSuperStormEditorModule>("SavageSuperStormEditor"))
			{
				Module->OpenFlowMapPainter(Component);
			}
		}
	}
	return FReply::Handled();
}

FReply FSSSActorDetailCustomization::OnSavePresetAsClicked()
{
	if (AVolumetricSuperStormActor* Actor = WeakActor.Get())
	{
		if (FSavageSuperStormEditorModule* Module = FModuleManager::GetModulePtr<FSavageSuperStormEditorModule>("SavageSuperStormEditor"))
		{
			Module->SavePresetAs(Actor);
		}
	}
	return FReply::Handled();
}

FReply FSSSActorDetailCustomization::OnLoadPresetClicked()
{
	// LoadPresetInto owns the picker, edit guard, and irreversible undo boundary, so
	// cancelling before the apply leaves the actor untouched -- no Modify() here.
	if (AVolumetricSuperStormActor* Actor = WeakActor.Get())
	{
		if (FSavageSuperStormEditorModule* Module = FModuleManager::GetModulePtr<FSavageSuperStormEditorModule>("SavageSuperStormEditor"))
		{
			Module->LoadPresetInto(Actor);
		}
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
