// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file AssetDefinition_StormWindFlowMap.h
 * @brief Declares the read-only flow-map asset definition.
 */

#pragma once

#include "AssetDefinitionDefault.h"
#include "AssetDefinition_StormWindFlowMap.generated.h"

/** @brief Provides UAssetDefinition_StormWindFlowMap behavior. */
UCLASS()
class UAssetDefinition_StormWindFlowMap final : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText                               GetAssetDisplayName() const override;
	virtual FLinearColor                        GetAssetColor() const override;
	virtual TSoftClassPtr<UObject>              GetAssetClass() const override;
	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override;
};
