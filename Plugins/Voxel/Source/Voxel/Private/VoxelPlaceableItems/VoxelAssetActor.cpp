// Copyright 2020 Phyronnaz

#include "VoxelPlaceableItems/VoxelAssetActor.h"
#include "VoxelPlaceableItems/VoxelDefaultItems.h"
#include "VoxelPlaceableItems/VoxelAssetItem.h"
#include "VoxelAssets/VoxelDataAsset.h"
#include "VoxelData/VoxelData.h"
#include "VoxelDebug/VoxelDebugManager.h"
#include "VoxelRender/IVoxelRenderer.h"
#include "VoxelRender/IVoxelLODManager.h"
#include "VoxelRender/LODManager/VoxelFixedResolutionLODManager.h"
#include "VoxelRender/VoxelProceduralMeshComponent.h"
#include "VoxelRender/Renderers/VoxelDefaultRenderer.h"
#include "VoxelWorldGenerators/VoxelEmptyWorldGenerator.h"
#include "VoxelWorld.h"
#include "VoxelDefaultPool.h"
#include "VoxelMessages.h"

#include "Components/BoxComponent.h"

AVoxelAssetActor::AVoxelAssetActor()
{
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

#if WITH_EDITOR
	PrimitiveComponent = CreateDefaultSubobject<UAssetActorPrimitiveComponent>(TEXT("PrimitiveComponent"));
	PrimitiveComponent->SetupAttachment(Root);

	SetActorEnableCollision(true); // To place items on top of it

	Box = CreateDefaultSubobject<UBoxComponent>("Box");
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetupAttachment(Root);

	PrimaryActorTick.bCanEverTick = true;
#endif
}

void AVoxelAssetActor::AddItemToWorld(AVoxelWorld* World) const
{
	check(World);

	if (World->GetPlayType() != EVoxelPlayType::Game)
	{
		return;
	}
	if (!WorldGenerator.IsValid())
	{
		FVoxelMessages::Error("Invalid world generator", this);
		return;
	}

	AddItemToData(World, &World->GetData());
}

int32 AVoxelAssetActor::GetPriority() const
{
	return Priority;
}

FIntBox AVoxelAssetActor::AddItemToData(
	AVoxelWorld* VoxelWorld,
	FVoxelData* VoxelWorldData) const
{
	auto Transform = GetTransform() * VoxelWorld->GetTransform().Inverse();
	Transform.ScaleTranslation(1.f / VoxelWorld->VoxelSize);

	FIntBox WorldBounds;
	if (bOverrideAssetBounds)
	{
		// Might be one-off error there
		WorldBounds = AssetBounds.Translate(FVoxelUtilities::FloorToInt(Transform.GetTranslation()));
	}
	else
	{
		if (auto* DataAsset = Cast<UVoxelDataAsset>(WorldGenerator.GetObject()))
		{
			WorldBounds = DataAsset->GetBounds().ApplyTransform(Transform);
		}
		else
		{
			FVoxelMessages::Error(
				"Voxel Asset Actor: AssetBounds are not overriden, and cannot deduce them from WorldGenerator as it's not a voxel data asset\n"
				"You need to tick the checkbox next to Asset Bounds on the asset actor",
				this);
			WorldBounds = FIntBox(-25, 25).Translate(FVoxelUtilities::FloorToInt(Transform.GetTranslation()));
		}
	}

	if (!VoxelWorldData || !ensure(WorldBounds.IsValid()))
	{
		return WorldBounds;
	}

	auto AssetInstance = WorldGenerator.GetInstance(false);
	auto InitStruct = VoxelWorld->GetInitStruct();
	for (auto& It : Seeds)
	{
		InitStruct.Seeds.Add(It.Key, It.Value);
	}
	AssetInstance->Init(InitStruct);

	if (bImportAsReference)
	{
		FVoxelWriteScopeLock Lock(*VoxelWorldData, WorldBounds, FUNCTION_FNAME);
		VoxelWorldData->AddItem<FVoxelAssetItem>(
			AssetInstance,
			WorldBounds,
			Transform,
			Priority);
	}
	else
	{
		FVoxelWriteScopeLock Lock(*VoxelWorldData, WorldBounds, FUNCTION_FNAME);
		UVoxelAssetTools::ImportAssetImpl(
			*VoxelWorldData,
			WorldBounds,
			Transform,
			*AssetInstance,
			bSubtractiveAsset,
			MergeMode,
			EVoxelMaterialMask::All); // TODO: expose material mask?
	}

	return WorldBounds;
}

#if WITH_EDITOR
void AVoxelAssetActor::UpdatePreview()
{
	if (!PreviewWorld) return;
	if (!WorldGenerator.IsValid()) return;

	if (IsPreviewCreated())
	{
		DestroyPreview();
		CreatePreview();
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void AVoxelAssetActor::BeginPlay()
{
	Super::BeginPlay();

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	PrimaryActorTick.SetTickFunctionEnable(false);
}

void AVoxelAssetActor::BeginDestroy()
{
	Super::BeginDestroy();

	if (IsPreviewCreated())
	{
		DestroyPreview();
	}
}

void AVoxelAssetActor::Tick(float DeltaTime)
{
	if (GetWorld()->WorldType != EWorldType::Editor)
	{
		// Editor preview still ticks
		return;
	}
	if (PreviewWorld)
	{
		if (WorldGenerator.IsValid() && !IsPreviewCreated())
		{
			CreatePreview();
		}
		if (!PreviewWorld->OnPropertyChanged.IsBoundToObject(this))
		{
			PreviewWorld->OnPropertyChanged.AddUObject(this, &AVoxelAssetActor::UpdatePreview);
		}

		UpdateBox();
	}
}

void AVoxelAssetActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PreviewWorld && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		ClampTransform();
	}
	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const auto Name = PropertyChangedEvent.MemberProperty->GetFName();
		if (Name == GET_MEMBER_NAME_STATIC(AVoxelAssetActor, WorldGenerator) ||
			Name == GET_MEMBER_NAME_STATIC(FVoxelTransformableWorldGeneratorPicker, Type) ||
			Name == GET_MEMBER_NAME_STATIC(FVoxelTransformableWorldGeneratorPicker, WorldGeneratorClass) ||
			Name == GET_MEMBER_NAME_STATIC(FVoxelTransformableWorldGeneratorPicker, WorldGeneratorObject) ||
			Name == GET_MEMBER_NAME_STATIC(AVoxelAssetActor, bOverrideAssetBounds) ||
			Name == GET_MEMBER_NAME_STATIC(AVoxelAssetActor, AssetBounds) ||
			Name == GET_MEMBER_NAME_STATIC(AVoxelAssetActor, PreviewLOD) ||
			Name == GET_MEMBER_NAME_STATIC(AVoxelAssetActor, bSubtractiveAsset) ||
			Name == GET_MEMBER_NAME_STATIC(AVoxelAssetActor, bImportAsReference) ||
			Name == GET_MEMBER_NAME_STATIC(AVoxelAssetActor, Seeds) ||
			Name == GET_MEMBER_NAME_STATIC(AVoxelAssetActor, PreviewWorld) ||
			Name == STATIC_FNAME("RelativeScale3D") ||
			Name == STATIC_FNAME("RelativeRotation"))
		{
			UpdatePreview();
		}
	}
}

void AVoxelAssetActor::PostEditMove(const bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (PreviewWorld && WorldGenerator.IsValid())
	{
		if (UpdateType == EVoxelAssetActorPreviewUpdateType::RealTime)
		{
			UpdatePreview();
		}
		if (bFinished)
		{
			ClampTransform();
			if (UpdateType == EVoxelAssetActorPreviewUpdateType::EndOfMove)
			{
				UpdatePreview();
			}
		}
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
bool AVoxelAssetActor::IsPreviewCreated() const
{
	return Data.IsValid();
}

void AVoxelAssetActor::CreatePreview()
{
	BindEditorDelegates(this);

	if (!ensure(PreviewWorld)) return;
	if (!ensure(WorldGenerator.IsValid())) return;
	if (!ensure(!IsPreviewCreated())) return;

	if (!StaticPool.IsValid())
	{
		StaticPool = FVoxelDefaultPool::Create(8, true, {}, {});
	}

	PrimitiveComponent->SetWorldTransform(PreviewWorld->GetTransform());
	const FIntBox Bounds = AddItemToData(PreviewWorld, nullptr);

	{
		auto EmptyGenerator = MakeVoxelShared<FVoxelEmptyWorldGeneratorInstance>(bImportAsReference ? 1 : bSubtractiveAsset ? -1 : 1);
		EmptyGenerator->Init(PreviewWorld->GetInitStruct());
		Data = FVoxelData::Create(
			FVoxelDataSettings(
				Bounds,
				EmptyGenerator,
				false,
				false));

		const auto RealMergeMode = MergeMode;
		MergeMode = EVoxelAssetMergeMode::AllValuesAndAllMaterials;
		AddItemToData(PreviewWorld, Data.Get());
		MergeMode = RealMergeMode;
	}

	DebugManager = FVoxelDebugManager::Create(FVoxelDebugManagerSettings(
		PreviewWorld,
		EVoxelPlayType::Preview,
		StaticPool.ToSharedRef(),
		Data.ToSharedRef(),
		true));

	Renderer = FVoxelDefaultRenderer::Create(FVoxelRendererSettings(
		PreviewWorld,
		EVoxelPlayType::Preview,
		PrimitiveComponent,
		Data.ToSharedRef(),
		StaticPool.ToSharedRef(),
		nullptr,
		DebugManager.ToSharedRef(),
		true));

	LODManager = FVoxelFixedResolutionLODManager::Create(
		FVoxelLODSettings(PreviewWorld,
			EVoxelPlayType::Preview,
			Renderer.ToSharedRef(),
			StaticPool.ToSharedRef(),
			Data.Get()),
		FVoxelUtilities::ClampChunkDepth(PreviewLOD));
}

void AVoxelAssetActor::DestroyPreview()
{
	if (!ensure(IsPreviewCreated())) return;

	Data.Reset();

	DebugManager->Destroy();
	DebugManager.Reset();

	Renderer->Destroy();
	Renderer.Reset();

	LODManager->Destroy();
	LODManager.Reset();

	auto Components = GetComponents(); // need a copy as we are modifying it when destroying comps
	for (auto& Component : Components)
	{
		if (Component && Component->HasAnyFlags(RF_Transient) && Component->IsA<UVoxelProceduralMeshComponent>())
		{
			Component->DestroyComponent();
		}
	}
}

void AVoxelAssetActor::UpdateBox()
{
	if (!ensure(PreviewWorld)) return;

	const FIntBox Bounds = AddItemToData(PreviewWorld, nullptr);

	Box->SetWorldTransform(PreviewWorld->GetTransform());
	Box->SetBoxExtent(FVector(Bounds.Size()) / 2 * PreviewWorld->VoxelSize * PreviewWorld->GetActorScale3D());
	Box->SetWorldLocation(PreviewWorld->LocalToGlobalFloat(Bounds.GetCenter()));
}

void AVoxelAssetActor::ClampTransform()
{
	if (bRoundAssetPosition)
	{
		const FVector WorldLocation = PreviewWorld->GetActorLocation();
		const float VoxelSize = PreviewWorld->VoxelSize;

		FVector Position = GetActorLocation();
		Position -= WorldLocation;
		Position /= VoxelSize;

		Position.X = FMath::RoundToInt(Position.X);
		Position.Y = FMath::RoundToInt(Position.Y);
		Position.Z = FMath::RoundToInt(Position.Z);

		Position *= VoxelSize;
		Position += WorldLocation;

		SetActorLocation(Position);
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void AVoxelAssetActor::OnPrepareToCleanseEditorObject(UObject* Object)
{
	DestroyPreview();
}
#endif

#if WITH_EDITOR
TVoxelSharedPtr<IVoxelPool> AVoxelAssetActor::StaticPool;
#endif