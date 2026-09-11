// Copyright 2026 GoroGoro. All Rights Reserved.

/**
 * @file StormMaterialBinderComponent.cpp
 * @brief Binds storm render data through a global MPC or dynamic material instance.
 */

#include "Components/StormMaterialBinderComponent.h"

#include "EngineUtils.h"
#include "VolumetricSuperStormRuntime.h"
#include "Actors/VolumetricSuperStormActor.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Data/StormRenderTargetResolution.h"
#include "Material/StormCloudMaterialParameters.h"
#include "Material/StormMaterialPayload.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Rendering/StormFlowMapRenderTargetUtils.h"
#include "RenderCommandFence.h"
#include "Subsystems/StormRenderWorldSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UnrealType.h"

using namespace VolumetricSuperStorm;

namespace
{
	TWeakObjectPtr<UStormMaterialBinderComponent> GlobalTextureAnchorOwner;

	constexpr uint16 TextureSlotBit(EStormMaterialTextureSlot Slot)
	{
		return static_cast<uint16>(1u << static_cast<uint8>(Slot));
	}

	constexpr uint16 AllTextureSlotsMask =
		static_cast<uint16>((1u << static_cast<uint8>(EStormMaterialTextureSlot::Count)) - 1u);
	constexpr uint16 ProfileTextureSlotsMask =
		TextureSlotBit(EStormMaterialTextureSlot::BottomProfile) |
		TextureSlotBit(EStormMaterialTextureSlot::TopProfile) |
		TextureSlotBit(EStormMaterialTextureSlot::AnvilProfile);

	void WritePayloadToMID(UMaterialInstanceDynamic* Material, const FStormMaterialPayload& Payload)
	{
		if (!Material)
		{
			return;
		}

		for (const FStormScalarMaterialParameter& Parameter : Payload.Scalars)
		{
			Material->SetScalarParameterValue(Parameter.Name, Parameter.Value);
		}

		for (const FStormVectorMaterialParameter& Parameter : Payload.Vectors)
		{
			Material->SetVectorParameterValue(Parameter.Name, Parameter.Value);
		}

		for (const FStormTextureMaterialParameter& Parameter : Payload.Textures)
		{
			Material->SetTextureParameterValue(Parameter.ParameterName, Parameter.Texture);
		}
	}

	bool IsGlobalBackendWorldPreferred(const UWorld* Candidate, const UWorld* Current)
	{
		if (!Candidate || !Current)
		{
			return false;
		}

		const bool bCandidateIsGame = Candidate->IsGameWorld();
		const bool bCurrentIsEditor = Current->WorldType == EWorldType::Editor;
		return bCandidateIsGame && bCurrentIsEditor;
	}
}

UStormMaterialBinderComponent::UStormMaterialBinderComponent()
{
	static_assert(MaterialTextureSlotCount == static_cast<int32>(EStormMaterialTextureSlot::Count));
	PrimaryComponentTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection> ParameterCollectionFinder(
		TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Internal/MPC_StormGlobal.MPC_StormGlobal"));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> ShapeAnchorFinder(
		TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Internal/RT_Internal/Shape/RT_StormGlobal_Shape.RT_StormGlobal_Shape"));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> Shape2AnchorFinder(
		TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Internal/RT_Internal/Shape/RT_StormGlobal_Shape2.RT_StormGlobal_Shape2"));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> ProfileBottomAnchorFinder(
		TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Internal/RT_Internal/Profile/RT_StormGlobal_ProfileBottom.RT_StormGlobal_ProfileBottom"));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> ProfileTopAnchorFinder(
		TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Internal/RT_Internal/Profile/RT_StormGlobal_ProfileTop.RT_StormGlobal_ProfileTop"));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> ProfileAnvilAnchorFinder(
		TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Internal/RT_Internal/Profile/RT_StormGlobal_ProfileAnvil.RT_StormGlobal_ProfileAnvil"));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> FlowBottomAnchorFinder(
		TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Internal/RT_Internal/Flowmap/RT_StormGlobal_FlowBottom.RT_StormGlobal_FlowBottom"));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> FlowMiddleAnchorFinder(
		TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Internal/RT_Internal/Flowmap/RT_StormGlobal_FlowMiddle.RT_StormGlobal_FlowMiddle"));
	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> FlowTopAnchorFinder(
		TEXT("/VolumetricSuperStorm/VolumetricSuperStorm/Internal/RT_Internal/Flowmap/RT_StormGlobal_FlowTop.RT_StormGlobal_FlowTop"));

	GlobalParameterCollection = ParameterCollectionFinder.Object;
	GlobalShapeAnchor = ShapeAnchorFinder.Object;
	GlobalShape2Anchor = Shape2AnchorFinder.Object;
	GlobalProfileBottomAnchor = ProfileBottomAnchorFinder.Object;
	GlobalProfileTopAnchor = ProfileTopAnchorFinder.Object;
	GlobalProfileAnvilAnchor = ProfileAnvilAnchorFinder.Object;
	GlobalFlowBottomAnchor = FlowBottomAnchorFinder.Object;
	GlobalFlowMiddleAnchor = FlowMiddleAnchorFinder.Object;
	GlobalFlowTopAnchor = FlowTopAnchorFinder.Object;
}

void UStormMaterialBinderComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	if (!ObjectPropertyChangedHandle.IsValid())
	{
		ObjectPropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddUObject(this, &UStormMaterialBinderComponent::HandleObjectPropertyChanged);
	}
#endif
	TryAutoResolveTargetCloud();
	RefreshCloudMaterialBinding(false);
}

void UStormMaterialBinderComponent::OnUnregister()
{
	ReleaseVolumetricCloudMaterial();
#if WITH_EDITOR
	if (ObjectPropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(ObjectPropertyChangedHandle);
		ObjectPropertyChangedHandle.Reset();
	}
#endif
	Super::OnUnregister();
}

#if WITH_EDITOR
void UStormMaterialBinderComponent::HandleObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& PropertyChangedEvent)
{
	if (BindingBackend != EStormMaterialBindingBackend::DynamicMaterialInstance ||
		!bTrackTargetCloudMaterialChange ||
		!Object)
	{
		return;
	}

	if (Object != ResolvedTargetCloud.Get())
	{
		return;
	}

	const FName PropertyName               = PropertyChangedEvent.GetPropertyName();
	const bool  bMaterialChanged           = PropertyName == GET_MEMBER_NAME_CHECKED(UVolumetricCloudComponent, Material);
	const bool  bTransactionRestoredObject = PropertyName == NAME_None;
	if (!bMaterialChanged && !bTransactionRestoredObject)
	{
		return;
	}

	if (bMaterialChanged)
	{
		UVolumetricCloudComponent* Cloud = CastChecked<UVolumetricCloudComponent>(Object);

		UMaterialInterface* NewBase = Cloud->Material.LoadSynchronous();
		if (UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(NewBase))
		{
			NewBase = MID->Parent.Get();
		}

		if (NewBase && NewBase != BaseCloudMaterial)
		{
			Modify();
			BaseCloudMaterial = NewBase;
		}
	}

	QueueTargetCloudMaterialRebind();
}

void UStormMaterialBinderComponent::QueueTargetCloudMaterialRebind()
{
	if (bCloudMaterialRebindPending)
	{
		return;
	}

	bCloudMaterialRebindPending = true;
	TWeakObjectPtr<UStormMaterialBinderComponent> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis](float)
			{
				if (UStormMaterialBinderComponent* Binder = WeakThis.Get())
				{
					Binder->bCloudMaterialRebindPending = false;
					if (Binder->bTrackTargetCloudMaterialChange)
					{
						Binder->RefreshCloudMaterialBinding(false);
					}
				}
				return false;
			}
		),
		0.0f
	);
}
#endif

void UStormMaterialBinderComponent::PushStormRenderData(const FStormRenderData& RenderData)
{
	PushStormData(RenderData, EStormRenderUpdateScope::Full);
}

void UStormMaterialBinderComponent::PushStormData(
	const FStormRenderData& RenderData,
	EStormRenderUpdateScope Scope)
{
	if (Scope == EStormRenderUpdateScope::Full)
	{
		DiscardPendingTextureSlots(AllTextureSlotsMask);

		FStormMaterialPayload Payload;
		BuildFullStormMaterialPayload(
			RenderData,
			ResolveTextureSources(),
			Payload);

		bHasUploadedFullPayload = ApplyPayload(Payload);
		LastFullyUploadedMaterial = BindingBackend == EStormMaterialBindingBackend::DynamicMaterialInstance && bHasUploadedFullPayload
			? DynamicCloudMaterial.Get()
			: nullptr;
		return;
	}

	if (Scope == EStormRenderUpdateScope::Profile)
	{
		DiscardPendingTextureSlots(ProfileTextureSlotsMask);
	}

	if (!IsReadyForIncrementalUpload())
	{
		PushStormData(RenderData, EStormRenderUpdateScope::Full);
		return;
	}

	FStormMaterialPayload Payload;
	if (Scope == EStormRenderUpdateScope::Profile)
	{
		BuildProfileStormMaterialPayload(RenderData, Payload);
	}
	else
	{
		BuildFrameStormMaterialPayload(RenderData, Payload);
	}
	ApplyPayload(Payload);
}

void UStormMaterialBinderComponent::QueueStormTexturePayload(const FStormMaterialPayload& Payload)
{
	if (BindingBackend != EStormMaterialBindingBackend::GlobalMPC ||
		!IsReadyForIncrementalUpload())
	{
		return;
	}

	for (const FStormTextureMaterialParameter& Parameter : Payload.Textures)
	{
		const int32 SlotIndex = static_cast<int32>(Parameter.Slot);
		if (SlotIndex < 0 || SlotIndex >= MaterialTextureSlotCount)
		{
			continue;
		}

		PendingTextures[SlotIndex] = Parameter.Texture;
		PendingTextureMask |= TextureSlotBit(Parameter.Slot);
	}

	if (PendingTextureMask == 0 || bTextureFlushScheduled)
	{
		return;
	}

	bTextureFlushScheduled = true;
	TWeakObjectPtr<UStormMaterialBinderComponent> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis](float)
			{
				if (UStormMaterialBinderComponent* Binder = WeakThis.Get())
				{
					Binder->bTextureFlushScheduled = false;
					Binder->FlushPendingTexturePayload();
				}
				return false;
			}),
		0.0f);
}

void UStormMaterialBinderComponent::FlushPendingTexturePayload()
{
	const uint16 PendingMask = PendingTextureMask;
	TStaticArray<UTexture*, MaterialTextureSlotCount> Textures{};
	for (int32 SlotIndex = 0; SlotIndex < MaterialTextureSlotCount; ++SlotIndex)
	{
		Textures[SlotIndex] = PendingTextures[SlotIndex].Get();
	}
	DiscardPendingTextureSlots(AllTextureSlotsMask);

	if (PendingMask == 0 ||
		BindingBackend != EStormMaterialBindingBackend::GlobalMPC ||
		!IsReadyForIncrementalUpload())
	{
		return;
	}

	FStormMaterialPayload Payload;
	for (int32 SlotIndex = 0; SlotIndex < MaterialTextureSlotCount; ++SlotIndex)
	{
		const EStormMaterialTextureSlot Slot = static_cast<EStormMaterialTextureSlot>(SlotIndex);
		if ((PendingMask & TextureSlotBit(Slot)) != 0)
		{
			AppendStormMaterialTexture(Slot, Textures[SlotIndex], Payload);
		}
	}
	ApplyPayload(Payload);
}

void UStormMaterialBinderComponent::DiscardPendingTextureSlots(uint16 SlotMask)
{
	for (int32 SlotIndex = 0; SlotIndex < MaterialTextureSlotCount; ++SlotIndex)
	{
		const uint16 SlotBit = static_cast<uint16>(1u << SlotIndex);
		if ((SlotMask & SlotBit) != 0)
		{
			PendingTextures[SlotIndex].Reset();
		}
	}
	PendingTextureMask = static_cast<uint16>(PendingTextureMask & ~SlotMask);
}

UMaterialInstanceDynamic* UStormMaterialBinderComponent::ApplyVolumetricCloudMaterial()
{
	if (BindingBackend != EStormMaterialBindingBackend::DynamicMaterialInstance)
	{
		UpdateVolumetricCloudMaterial();
		return nullptr;
	}

	if (bHasActiveBindingBackend)
	{
		DeactivateCurrentBackend();
	}
	UMaterialInstanceDynamic* Material = EnsureDynamicCloudMaterial(true);
	UpdateVolumetricCloudMaterial();
	return Material;
}

void UStormMaterialBinderComponent::UpdateVolumetricCloudMaterial()
{
	const AVolumetricSuperStormActor* StormActor = Cast<AVolumetricSuperStormActor>(GetOwner());
	if (!StormActor)
	{
		return;
	}

	const FStormRenderData& RenderData = StormActor->GetStormRenderDataRef();
	PushStormRenderData(RenderData);
}

void UStormMaterialBinderComponent::OnComponentCreated()
{
	Super::OnComponentCreated();

	TryAutoResolveTargetCloud();
}

void UStormMaterialBinderComponent::BeginPlay()
{
	Super::BeginPlay();

	TryAutoResolveTargetCloud();
	RefreshCloudMaterialBinding(false);
}

void UStormMaterialBinderComponent::RefreshCloudMaterialBinding(bool bReapplyCloudMaterial)
{
	if (bReapplyCloudMaterial && bHasActiveBindingBackend)
	{
		DeactivateCurrentBackend();
	}

	if (const AVolumetricSuperStormActor* StormActor = Cast<AVolumetricSuperStormActor>(GetOwner()))
	{
		PushStormRenderData(StormActor->GetStormRenderDataRef());
	}
	else
	{
		UpdateVolumetricCloudMaterial();
	}
}

#if WITH_EDITOR
void UStormMaterialBinderComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

	const bool bTargetChanged   = MemberName == GET_MEMBER_NAME_CHECKED(UStormMaterialBinderComponent, TargetCloudReference);
	const bool bMaterialChanged = bTargetChanged || PropertyName == GET_MEMBER_NAME_CHECKED(UStormMaterialBinderComponent, BaseCloudMaterial);
	const bool bBackendChanged  = PropertyName == GET_MEMBER_NAME_CHECKED(UStormMaterialBinderComponent, BindingBackend);

	if (bTargetChanged || bBackendChanged)
	{
		DeactivateCurrentBackend();
		ResolvedTargetCloud.Reset();
	}

	RefreshCloudMaterialBinding(bMaterialChanged || bBackendChanged);
}

#endif

UVolumetricCloudComponent* UStormMaterialBinderComponent::ResolveTargetCloud() const
{
	if (ResolvedTargetCloud.IsValid() && ResolvedTargetCloud->GetWorld() == GetWorld())
	{
		return ResolvedTargetCloud.Get();
	}

	UVolumetricCloudComponent* Cloud = nullptr;
	if (IsTargetCloudReferenceSet())
	{
		Cloud = Cast<UVolumetricCloudComponent>(TargetCloudReference.GetComponent(GetOwner()));
	}
	else
	{
		Cloud = FindUniqueWorldCloud();
	}

	if (!IsValid(Cloud) || Cloud->GetWorld() != GetWorld())
	{
		ResolvedTargetCloud.Reset();
		return nullptr;
	}

	bReportedAmbiguousCloudTarget = false;
	ResolvedTargetCloud           = Cloud;
	return Cloud;
}

void UStormMaterialBinderComponent::ReleaseVolumetricCloudMaterial()
{
	DeactivateCurrentBackend();

	ResolvedTargetCloud.Reset();
	bReportedAmbiguousCloudTarget = false;
	bReportedInvalidMaterial      = false;
	bReportedMissingGlobalResources = false;
	bReportedGlobalOwnershipConflict = false;
}

UMaterialInstanceDynamic* UStormMaterialBinderComponent::GetDynamicCloudMaterial() const
{
	return DynamicCloudMaterial;
}

void UStormMaterialBinderComponent::ApplyLightningMaterialState(const FStormLightningMaterialState& State)
{
	FStormMaterialPayload Payload;
	BuildLightningStormMaterialPayload(State, Payload);
	ApplyPayload(Payload);
}

void UStormMaterialBinderComponent::SetLightningPulse(float Pulse)
{
	FStormMaterialPayload Payload;
	BuildLightningPulseStormMaterialPayload(Pulse, Payload);
	ApplyPayload(Payload);
}

void UStormMaterialBinderComponent::ClearLightningMaterialState()
{
	SetLightningPulse(0.0f);
}

bool UStormMaterialBinderComponent::IsTargetCloudReferenceSet() const
{
	return TargetCloudReference.OtherActor.IsValid() || TargetCloudReference.OverrideComponent.IsValid() || TargetCloudReference.ComponentProperty != NAME_None || !TargetCloudReference.PathToComponent.IsEmpty();
}

UVolumetricCloudComponent* UStormMaterialBinderComponent::FindUniqueWorldCloud() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UVolumetricCloudComponent* Cloud      = nullptr;
	int32                      CloudCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		TInlineComponentArray<UVolumetricCloudComponent*> CloudComponents(*It);
		for (UVolumetricCloudComponent* Candidate : CloudComponents)
		{
			if (IsValid(Candidate))
			{
				Cloud = Candidate;
				++CloudCount;
			}
		}
	}

	if (CloudCount == 1)
	{
		bReportedAmbiguousCloudTarget = false;
		return Cloud;
	}

	if (CloudCount > 1 && !bReportedAmbiguousCloudTarget)
	{
		UE_LOG(LogVolumetricSuperStormRuntime, Error, TEXT("Storm '%s' found %d volumetric cloud components. Set Target Cloud Reference explicitly."), *GetNameSafe(GetOwner()), CloudCount);
		bReportedAmbiguousCloudTarget = true;
	}

	return nullptr;
}

void UStormMaterialBinderComponent::TryAutoResolveTargetCloud()
{
	if (BindingBackend != EStormMaterialBindingBackend::DynamicMaterialInstance)
	{
		return;
	}

	if (IsTargetCloudReferenceSet())
	{
		return;
	}

	UVolumetricCloudComponent* Cloud = FindUniqueWorldCloud();
	if (!Cloud)
	{
		return;
	}

	ResolvedTargetCloud = Cloud;
	if (!BaseCloudMaterial)
	{
		BaseCloudMaterial = Cloud->GetMaterial();
	}
}

UMaterialInstanceDynamic* UStormMaterialBinderComponent::EnsureDynamicCloudMaterial(bool bForceRecreate)
{
	const AVolumetricSuperStormActor* StormOwner      = Cast<AVolumetricSuperStormActor>(GetOwner());
	UWorld*                           World           = GetWorld();
	const UStormRenderWorldSubsystem* RenderSubsystem = World ? World->GetSubsystem<UStormRenderWorldSubsystem>() : nullptr;
	if (!StormOwner || !RenderSubsystem || !RenderSubsystem->IsRegisteredStorm(StormOwner))
	{
		return nullptr;
	}

	UVolumetricCloudComponent* Cloud = ResolveTargetCloud();
	if (!Cloud)
	{
		return nullptr;
	}

	UMaterialInterface*       CurrentMaterial = Cloud->GetMaterial();
	UMaterialInstanceDynamic* CurrentMID      = Cast<UMaterialInstanceDynamic>(CurrentMaterial);
	const bool                bOwnedByThis    = CurrentMID && CurrentMID->GetOuter() == this;
	if (!bOwnedByThis)
	{
		PreviousCloudMaterial             = CurrentMaterial;
		bHasCapturedPreviousCloudMaterial = true;
	}

	if (!bForceRecreate && bOwnedByThis)
	{
		const bool bReparentNeeded = BaseCloudMaterial && CurrentMID->Parent != BaseCloudMaterial;
		if (!bReparentNeeded)
		{
			DynamicCloudMaterial = CurrentMID;
			return CurrentMID;
		}
	}

	UMaterialInterface* Parent = BaseCloudMaterial ? BaseCloudMaterial.Get() : (bOwnedByThis ? CurrentMID->Parent.Get() : CurrentMaterial);
	if (!Parent)
	{
		return nullptr;
	}

	if (!CloudMaterialParams::HasStormMaterialContract(Parent))
	{
		if (!bReportedInvalidMaterial)
		{
			UE_LOG(
				LogVolumetricSuperStormRuntime,
				Error,
				TEXT("Storm '%s' cannot bind cloud material '%s': it does not implement the storm cloud contract, "
					 "so the Dynamic Material Instance backend has nothing to write into. Either set Base Cloud "
					 "Material to M_VolumetricSuperStorm, or switch Binding Backend to Global MPC and give the "
					 "cloud a material containing MF_Storm_Global, such as M_StormMinimalSetup."),
				*GetNameSafe(GetOwner()),
				*GetNameSafe(Parent));
			bReportedInvalidMaterial = true;
		}
		return nullptr;
	}
	bReportedInvalidMaterial = false;

	UMaterialInstanceDynamic* CachedMID          = DynamicCloudMaterial.Get();
	const bool                bCanReuseCachedMID = !bForceRecreate && IsValid(CachedMID) && CachedMID->GetOuter() == this && CachedMID->Parent == Parent;

	if (bCanReuseCachedMID)
	{
		Cloud->SetMaterial(CachedMID);
		return CachedMID;
	}

	DynamicCloudMaterial = UMaterialInstanceDynamic::Create(Parent, this);
	if (DynamicCloudMaterial)
	{
		Cloud->SetMaterial(DynamicCloudMaterial);
	}

	return DynamicCloudMaterial;
}

bool UStormMaterialBinderComponent::ActivateSelectedBackend(bool bForceRecreateMID)
{
	const AVolumetricSuperStormActor* StormOwner = Cast<AVolumetricSuperStormActor>(GetOwner());
	UWorld* World = GetWorld();
	const UStormRenderWorldSubsystem* RenderSubsystem = World
		? World->GetSubsystem<UStormRenderWorldSubsystem>()
		: nullptr;
	if (!StormOwner || !RenderSubsystem || !RenderSubsystem->IsRegisteredStorm(StormOwner))
	{
		return false;
	}

	if (bHasActiveBindingBackend && ActiveBindingBackend != BindingBackend)
	{
		DeactivateCurrentBackend();
	}

	if (BindingBackend == EStormMaterialBindingBackend::GlobalMPC)
	{
		if (!GlobalParameterCollection ||
			!GlobalShapeAnchor ||
			!GlobalShape2Anchor ||
			!GlobalProfileBottomAnchor ||
			!GlobalProfileTopAnchor ||
			!GlobalProfileAnvilAnchor ||
			!GlobalFlowBottomAnchor ||
			!GlobalFlowMiddleAnchor ||
			!GlobalFlowTopAnchor)
		{
			if (!bReportedMissingGlobalResources)
			{
				UE_LOG(
					LogVolumetricSuperStormRuntime,
					Error,
					TEXT("Storm '%s' cannot use the Global MPC backend because one or more internal MPC/render-target assets are missing."),
					*GetNameSafe(GetOwner()));
				bReportedMissingGlobalResources = true;
			}
			return false;
		}

		if (!AcquireGlobalTextureAnchors() || !ResolveGlobalParameterCollectionInstance())
		{
			return false;
		}

		ActiveBindingBackend = BindingBackend;
		bHasActiveBindingBackend = true;
		return true;
	}

	TryAutoResolveTargetCloud();
	if (!EnsureDynamicCloudMaterial(bForceRecreateMID))
	{
		return false;
	}

	ActiveBindingBackend = BindingBackend;
	bHasActiveBindingBackend = true;
	return true;
}

void UStormMaterialBinderComponent::DeactivateCurrentBackend()
{
	DiscardPendingTextureSlots(AllTextureSlotsMask);
	if (bOwnsGlobalTextureAnchors ||
		(bHasActiveBindingBackend && ActiveBindingBackend == EStormMaterialBindingBackend::GlobalMPC))
	{
		DisableGlobalMaterialState();
		ReleaseGlobalTextureAnchors();
	}

	if (DynamicCloudMaterial)
	{
		UVolumetricCloudComponent* Cloud = ResolvedTargetCloud.Get();
		if (!Cloud)
		{
			Cloud = ResolveTargetCloud();
		}

		UMaterialInstanceDynamic* CurrentMID = Cloud
			? Cast<UMaterialInstanceDynamic>(Cloud->GetMaterial())
			: nullptr;
		if (Cloud &&
			CurrentMID == DynamicCloudMaterial &&
			CurrentMID->GetOuter() == this &&
			bHasCapturedPreviousCloudMaterial)
		{
			Cloud->SetMaterial(PreviousCloudMaterial.Get());
		}
	}

	DynamicCloudMaterial = nullptr;
	PreviousCloudMaterial = nullptr;
	bHasCapturedPreviousCloudMaterial = false;
	LastFullyUploadedMaterial.Reset();
	bHasUploadedFullPayload = false;
	bHasActiveBindingBackend = false;
}

bool UStormMaterialBinderComponent::IsReadyForIncrementalUpload() const
{
	if (!bHasActiveBindingBackend ||
		ActiveBindingBackend != BindingBackend ||
		!bHasUploadedFullPayload)
	{
		return false;
	}

	if (BindingBackend == EStormMaterialBindingBackend::GlobalMPC)
	{
		return HasGlobalTextureAnchorOwnership() &&
			ResolveGlobalParameterCollectionInstance() != nullptr;
	}

	return DynamicCloudMaterial &&
		LastFullyUploadedMaterial.Get() == DynamicCloudMaterial.Get();
}

FStormMaterialTextureSources UStormMaterialBinderComponent::ResolveTextureSources() const
{
	FStormMaterialTextureSources TextureSources;
	if (UWorld* World = GetWorld())
	{
		if (UStormRenderWorldSubsystem* RenderSubsystem = World->GetSubsystem<UStormRenderWorldSubsystem>())
		{
			TextureSources.Shape = RenderSubsystem->GetShapeRenderTarget();
			TextureSources.Shape2 = RenderSubsystem->GetShapeRenderTarget2();
		}
	}
	return TextureSources;
}

bool UStormMaterialBinderComponent::ApplyPayload(const FStormMaterialPayload& Payload)
{
	if (!ActivateSelectedBackend())
	{
		return false;
	}

	switch (ActiveBindingBackend)
	{
	case EStormMaterialBindingBackend::GlobalMPC:
		return ApplyPayloadToMPC(Payload);
	case EStormMaterialBindingBackend::DynamicMaterialInstance:
		return ApplyPayloadToMID(Payload);
	default:
		return false;
	}
}

bool UStormMaterialBinderComponent::ApplyPayloadToMID(const FStormMaterialPayload& Payload)
{
	if (!DynamicCloudMaterial)
	{
		return false;
	}

	WritePayloadToMID(DynamicCloudMaterial, Payload);
	return true;
}

bool UStormMaterialBinderComponent::ApplyPayloadToMPC(const FStormMaterialPayload& Payload)
{
	UMaterialParameterCollectionInstance* CollectionInstance = ResolveGlobalParameterCollectionInstance();
	if (!CollectionInstance ||
		!HasGlobalTextureAnchorOwnership() ||
		!bGlobalTextureAnchorsReady)
	{
		return false;
	}

	bool bTexturesSynchronized = true;
	for (const FStormTextureMaterialParameter& Parameter : Payload.Textures)
	{
		bTexturesSynchronized &= SynchronizeGlobalTextureAnchor(Parameter.Slot, Parameter.Texture);
	}

	for (const FStormScalarMaterialParameter& Parameter : Payload.Scalars)
	{
		CollectionInstance->SetScalarParameterValue(Parameter.Name, Parameter.Value);
	}

	for (const FStormVectorMaterialParameter& Parameter : Payload.Vectors)
	{
		CollectionInstance->SetVectorParameterValue(Parameter.Name, Parameter.Value);
	}

	return bTexturesSynchronized;
}

UMaterialParameterCollectionInstance* UStormMaterialBinderComponent::ResolveGlobalParameterCollectionInstance() const
{
	UWorld* World = GetWorld();
	return World && GlobalParameterCollection
		? World->GetParameterCollectionInstance(GlobalParameterCollection)
		: nullptr;
}

UTextureRenderTarget2D* UStormMaterialBinderComponent::ResolveGlobalTextureAnchor(EStormMaterialTextureSlot Slot) const
{
	switch (Slot)
	{
	case EStormMaterialTextureSlot::Shape:
		return GlobalShapeAnchor.Get();
	case EStormMaterialTextureSlot::Shape2:
		return GlobalShape2Anchor.Get();
	case EStormMaterialTextureSlot::BottomProfile:
		return GlobalProfileBottomAnchor.Get();
	case EStormMaterialTextureSlot::TopProfile:
		return GlobalProfileTopAnchor.Get();
	case EStormMaterialTextureSlot::AnvilProfile:
		return GlobalProfileAnvilAnchor.Get();
	case EStormMaterialTextureSlot::FlowLower:
		return GlobalFlowBottomAnchor.Get();
	case EStormMaterialTextureSlot::FlowMiddle:
		return GlobalFlowMiddleAnchor.Get();
	case EStormMaterialTextureSlot::FlowUpper:
		return GlobalFlowTopAnchor.Get();
	default:
		return nullptr;
	}
}

bool UStormMaterialBinderComponent::ValidateGlobalTextureAnchorDimensions(
	UTextureRenderTarget2D* Anchor,
	const UTexture* SourceTexture) const
{
	if (!Anchor || !SourceTexture)
	{
		return false;
	}

	const int32 SourceWidth = FMath::RoundToInt(SourceTexture->GetSurfaceWidth());
	const int32 SourceHeight = FMath::RoundToInt(SourceTexture->GetSurfaceHeight());
	if (SourceWidth <= 0 || SourceHeight <= 0)
	{
		return false;
	}

	const bool bDimensionsMatch =
		Anchor->SizeX == SourceWidth &&
		Anchor->SizeY == SourceHeight;
	if (!bDimensionsMatch && !bReportedUnexpectedAnchorSize)
	{
		UE_LOG(
			LogVolumetricSuperStormRuntime,
			Warning,
			TEXT("Storm '%s' produced a %dx%d texture for fixed %dx%d global anchor '%s'. The source will be resampled into the anchor."),
			*GetNameSafe(GetOwner()),
			SourceWidth,
			SourceHeight,
			Anchor->SizeX,
			Anchor->SizeY,
			*GetNameSafe(Anchor));
		bReportedUnexpectedAnchorSize = true;
	}

	return true;
}

bool UStormMaterialBinderComponent::SynchronizeGlobalTextureAnchor(
	EStormMaterialTextureSlot Slot,
	UTexture* SourceTexture) const
{
	UWorld* World = GetWorld();
	UTextureRenderTarget2D* Anchor = ResolveGlobalTextureAnchor(Slot);
	if (!World || !Anchor)
	{
		return false;
	}

	if (SourceTexture == Anchor)
	{
		return true;
	}

	if (!SourceTexture)
	{
		const bool bFlowMap = Slot == EStormMaterialTextureSlot::FlowLower ||
			Slot == EStormMaterialTextureSlot::FlowMiddle ||
			Slot == EStormMaterialTextureSlot::FlowUpper;
		UKismetRenderingLibrary::ClearRenderTarget2D(
			World,
			Anchor,
			bFlowMap ? FLinearColor(0.5f, 0.5f, 0.5f, 0.0f) : FLinearColor::Black);
		return true;
	}

	if (!ValidateGlobalTextureAnchorDimensions(Anchor, SourceTexture))
	{
		return false;
	}

	return FlowMapRenderTargetUtils::BlitToSurface(World, SourceTexture, Anchor);
}

bool UStormMaterialBinderComponent::AcquireGlobalTextureAnchors()
{
	if (GlobalTextureAnchorOwner.Get() == this)
	{
		bOwnsGlobalTextureAnchors = true;
		return true;
	}

	if (UStormMaterialBinderComponent* CurrentOwner = GlobalTextureAnchorOwner.Get())
	{
		if (!IsGlobalBackendWorldPreferred(GetWorld(), CurrentOwner->GetWorld()))
		{
			if (!bReportedGlobalOwnershipConflict)
			{
				UE_LOG(
					LogVolumetricSuperStormRuntime,
					Error,
					TEXT("Storm '%s' cannot acquire the Global MPC texture anchors because they are owned by world '%s'. Use the MID backend for simultaneous in-process worlds."),
					*GetNameSafe(GetOwner()),
					*GetNameSafe(CurrentOwner->GetWorld()));
				bReportedGlobalOwnershipConflict = true;
			}
			return false;
		}

		CurrentOwner->DisableGlobalMaterialState();
		CurrentOwner->CancelGlobalTextureAnchorInitialization();
		CurrentOwner->bOwnsGlobalTextureAnchors = false;
		CurrentOwner->bHasUploadedFullPayload = false;
	}

	GlobalTextureAnchorOwner = this;
	bOwnsGlobalTextureAnchors = true;
	bReportedGlobalOwnershipConflict = false;
	DisableGlobalMaterialState();
	BeginGlobalTextureAnchorInitialization();
	return true;
}

void UStormMaterialBinderComponent::BeginGlobalTextureAnchorInitialization()
{
	bHasUploadedFullPayload = false;
	bGlobalTextureAnchorsReady = false;
	bReportedUnexpectedAnchorSize = false;
	const uint32 InitializationGeneration = ++GlobalTextureAnchorInitializationGeneration;

	auto PrepareAnchor = [](UTextureRenderTarget2D* Anchor, int32 Resolution)
	{
		if (!Anchor)
		{
			return;
		}

		if (Anchor->SizeX != Resolution || Anchor->SizeY != Resolution)
		{
			Anchor->ResizeTarget(
				static_cast<uint32>(Resolution),
				static_cast<uint32>(Resolution));
		}

		// Ensure an asset loaded without a resource initializes using the fixed dimensions.
		Anchor->UpdateResource();
	};

	PrepareAnchor(GlobalShapeAnchor, RenderTargetResolution::Shape);
	PrepareAnchor(GlobalShape2Anchor, RenderTargetResolution::Shape);
	PrepareAnchor(GlobalProfileBottomAnchor, RenderTargetResolution::Profile);
	PrepareAnchor(GlobalProfileTopAnchor, RenderTargetResolution::Profile);
	PrepareAnchor(GlobalProfileAnvilAnchor, RenderTargetResolution::Profile);
	PrepareAnchor(GlobalFlowBottomAnchor, RenderTargetResolution::FlowMap);
	PrepareAnchor(GlobalFlowMiddleAnchor, RenderTargetResolution::FlowMap);
	PrepareAnchor(GlobalFlowTopAnchor, RenderTargetResolution::FlowMap);

	TSharedRef<FRenderCommandFence> ResizeFence = MakeShared<FRenderCommandFence>();
	ResizeFence->BeginFence();

	TWeakObjectPtr<UStormMaterialBinderComponent> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[WeakThis, ResizeFence, InitializationGeneration](float)
			{
				if (!ResizeFence->IsFenceComplete())
				{
					return true;
				}

				UStormMaterialBinderComponent* Binder = WeakThis.Get();
				if (!Binder ||
					Binder->GlobalTextureAnchorInitializationGeneration != InitializationGeneration ||
					!Binder->HasGlobalTextureAnchorOwnership())
				{
					return false;
				}

				Binder->ClearGlobalTextureAnchors();
				Binder->bGlobalTextureAnchorsReady = true;
				if (AVolumetricSuperStormActor* StormOwner = Cast<AVolumetricSuperStormActor>(Binder->GetOwner()))
				{
					StormOwner->RequestRenderDataUpdate(EStormRenderUpdateScope::Full);
				}
				return false;
			}),
		0.0f);
}

void UStormMaterialBinderComponent::CancelGlobalTextureAnchorInitialization()
{
	++GlobalTextureAnchorInitializationGeneration;
	bGlobalTextureAnchorsReady = false;
}

void UStormMaterialBinderComponent::ReleaseGlobalTextureAnchors()
{
	const bool bShouldClearAnchors = bGlobalTextureAnchorsReady;
	CancelGlobalTextureAnchorInitialization();
	if (GlobalTextureAnchorOwner.Get() == this)
	{
		if (bShouldClearAnchors)
		{
			ClearGlobalTextureAnchors();
		}
		GlobalTextureAnchorOwner.Reset();
	}

	bOwnsGlobalTextureAnchors = false;
}

void UStormMaterialBinderComponent::ClearGlobalTextureAnchors() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UKismetRenderingLibrary::ClearRenderTarget2D(World, GlobalShapeAnchor, FLinearColor::Black);
	UKismetRenderingLibrary::ClearRenderTarget2D(World, GlobalShape2Anchor, FLinearColor::Black);
	UKismetRenderingLibrary::ClearRenderTarget2D(World, GlobalProfileBottomAnchor, FLinearColor::Black);
	UKismetRenderingLibrary::ClearRenderTarget2D(World, GlobalProfileTopAnchor, FLinearColor::Black);
	UKismetRenderingLibrary::ClearRenderTarget2D(World, GlobalProfileAnvilAnchor, FLinearColor::Black);
	const FLinearColor NeutralFlow(0.5f, 0.5f, 0.5f, 0.0f);
	UKismetRenderingLibrary::ClearRenderTarget2D(World, GlobalFlowBottomAnchor, NeutralFlow);
	UKismetRenderingLibrary::ClearRenderTarget2D(World, GlobalFlowMiddleAnchor, NeutralFlow);
	UKismetRenderingLibrary::ClearRenderTarget2D(World, GlobalFlowTopAnchor, NeutralFlow);
}

void UStormMaterialBinderComponent::DisableGlobalMaterialState() const
{
	if (UMaterialParameterCollectionInstance* CollectionInstance = ResolveGlobalParameterCollectionInstance())
	{
		CollectionInstance->SetScalarParameterValue(CloudMaterialParams::DensityMultiplier, 0.0f);
		CollectionInstance->SetScalarParameterValue(CloudMaterialParams::FlowMapEnabled, 0.0f);
		CollectionInstance->SetScalarParameterValue(CloudMaterialParams::LightningPulse, 0.0f);
	}
}

bool UStormMaterialBinderComponent::HasGlobalTextureAnchorOwnership() const
{
	return bOwnsGlobalTextureAnchors && GlobalTextureAnchorOwner.Get() == this;
}
