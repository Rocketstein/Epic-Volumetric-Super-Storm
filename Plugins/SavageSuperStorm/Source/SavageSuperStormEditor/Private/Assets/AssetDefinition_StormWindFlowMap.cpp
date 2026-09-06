/**
 * @file AssetDefinition_StormWindFlowMap.cpp
 * @brief Registers the read-only flow-map asset definition.
 */

#include "Assets/AssetDefinition_StormWindFlowMap.h"

#include "Assets/StormWindFlowMapDataAsset.h"
#include "Misc/MessageDialog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AssetDefinition_StormWindFlowMap)

#define LOCTEXT_NAMESPACE "AssetDefinition_StormWindFlowMap"

FText UAssetDefinition_StormWindFlowMap::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Storm Wind Flow Map");
}

FLinearColor UAssetDefinition_StormWindFlowMap::GetAssetColor() const
{
	return FLinearColor(0.08f, 0.55f, 0.82f);
}

TSoftClassPtr<UObject> UAssetDefinition_StormWindFlowMap::GetAssetClass() const
{
	return UStormWindFlowMapDataAsset::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_StormWindFlowMap::GetAssetCategories() const
{
	static const TArray<FAssetCategoryPath> Categories = { FAssetCategoryPath(EAssetCategoryPaths::Data, LOCTEXT("StormAssetCategory", "Savage SuperStorm"), ECategoryMenuType::Section) };
	return Categories;
}

EAssetCommandResult UAssetDefinition_StormWindFlowMap::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	FMessageDialog::Open(
		EAppMsgType::Ok,
		LOCTEXT(
			"ReadOnlyAssetNotice",
			"Storm flow-map assets are read-only baked documents.\n\n"
			"Select a Volumetric SuperStorm actor and open its Flow Map Editor to "
			"paint, load, or save flow maps."));
	return Super::OpenAssets(OpenArgs);
}

#undef LOCTEXT_NAMESPACE
