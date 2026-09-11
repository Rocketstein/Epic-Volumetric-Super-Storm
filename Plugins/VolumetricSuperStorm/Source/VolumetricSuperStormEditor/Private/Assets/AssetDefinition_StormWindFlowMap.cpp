// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file AssetDefinition_StormWindFlowMap.cpp
 * @brief Registers the read-only flow-map asset definition.
 */

#include "Assets/AssetDefinition_StormWindFlowMap.h"

#include "Assets/StormWindFlowMapDataAsset.h"
#include "Misc/EngineVersionComparison.h"

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
	// UE 5.8 introduced both the Data category and section-style category entries.
#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 8, 0)
	static const TArray<FAssetCategoryPath> Categories = { FAssetCategoryPath(EAssetCategoryPaths::Data, LOCTEXT("StormAssetCategory", "Savage SuperStorm"), ECategoryMenuType::Section) };
#else
	static const TArray<FAssetCategoryPath> Categories = { FAssetCategoryPath(EAssetCategoryPaths::Misc, LOCTEXT("StormAssetCategory", "Savage SuperStorm")) };
#endif
	return Categories;
}

#undef LOCTEXT_NAMESPACE
