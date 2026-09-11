// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormMaterialPayload.h
 * @brief Defines backend-neutral storm material parameter payloads.
 */

#pragma once

#include "CoreMinimal.h"
#include "Data/StormRenderUpdateTypes.h"

class UTexture;
struct FStormLightningMaterialState;
struct FStormRenderData;

namespace VolumetricSuperStorm
{
	enum class EStormMaterialTextureSlot : uint8
	{
		Shape,
		Shape2,
		BottomProfile,
		TopProfile,
		AnvilProfile,
		FlowLower,
		FlowMiddle,
		FlowUpper,
		Count
	};

	struct FStormScalarMaterialParameter
	{
		FName Name;
		float Value = 0.0f;
	};

	struct FStormVectorMaterialParameter
	{
		FName Name;
		FLinearColor Value = FLinearColor::Black;
	};

	struct FStormTextureMaterialParameter
	{
		EStormMaterialTextureSlot Slot = EStormMaterialTextureSlot::Shape;
		FName ParameterName;
		UTexture* Texture = nullptr;
	};

	/** Shape textures are world-subsystem resources and are not stored in FStormRenderData. */
	struct FStormMaterialTextureSources
	{
		UTexture* Shape = nullptr;
		UTexture* Shape2 = nullptr;
	};

	/** A transient parameter block that can be consumed by any material binding backend. */
	struct FStormMaterialPayload
	{
		TArray<FStormScalarMaterialParameter, TInlineAllocator<32>> Scalars;
		TArray<FStormVectorMaterialParameter, TInlineAllocator<32>> Vectors;
		TArray<FStormTextureMaterialParameter, TInlineAllocator<8>> Textures;

		void Reset();
	};

	/** Builds the complete payload used after static storm state changes. */
	void BuildFullStormMaterialPayload(
		const FStormRenderData& RenderData,
		const FStormMaterialTextureSources& TextureSources,
		FStormMaterialPayload& OutPayload);

	/** Builds only spatial, motion, lifecycle, and time-dependent parameters. */
	void BuildFrameStormMaterialPayload(
		const FStormRenderData& RenderData,
		FStormMaterialPayload& OutPayload);

	/** Builds only the three vertical-profile texture bindings. */
	void BuildProfileStormMaterialPayload(
		const FStormRenderData& RenderData,
		FStormMaterialPayload& OutPayload);

	/** Adds one texture binding using the canonical material parameter for its slot. */
	void AppendStormMaterialTexture(
		EStormMaterialTextureSlot Slot,
		UTexture* Texture,
		FStormMaterialPayload& OutPayload);

	/** Builds the texture bindings selected by a backend-neutral dirty mask. */
	void BuildTextureStormMaterialPayload(
		EStormTextureDirtyFlags DirtyTextures,
		const FStormRenderData& RenderData,
		const FStormMaterialTextureSources& TextureSources,
		FStormMaterialPayload& OutPayload);

	/** Builds the complete non-pulse lightning parameter payload. */
	void BuildLightningStormMaterialPayload(
		const FStormLightningMaterialState& State,
		FStormMaterialPayload& OutPayload);

	/** Builds the independently updated lightning pulse payload. */
	void BuildLightningPulseStormMaterialPayload(
		float Pulse,
		FStormMaterialPayload& OutPayload);
}
