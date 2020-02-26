// Copyright 2020 Phyronnaz

#include "VoxelDataAssetEditorManager.h"
#include "VoxelMathUtilities.h"
#include "VoxelGlobals.h"
#include "VoxelWorld.h"
#include "VoxelDebugUtilities.h"
#include "VoxelData/VoxelData.h"
#include "VoxelData/VoxelDataAccelerator.h"
#include "VoxelTools/VoxelDataTools.h"
#include "VoxelTools/VoxelBlueprintLibrary.h"
#include "VoxelAssets/VoxelDataAsset.h"
#include "VoxelRender/VoxelMaterialCollection.h"
#include "VoxelSettings.h"

#include "DrawDebugHelpers.h"
#include "PreviewScene.h"
#include "EngineUtils.h"
#include "Misc/ScopedSlowTask.h"
#include "Materials/MaterialInterface.h"

#define LOCTEXT_NAMESPACE "Voxel"

FVoxelDataAssetEditorManager::FVoxelDataAssetEditorManager(UVoxelDataAsset* DataAsset, FPreviewScene& PreviewScene)
	: DataAsset(DataAsset)
{
	check(DataAsset);

	if (!DataAsset->VoxelWorldTemplate)
	{
		auto* NewWorld = NewObject<AVoxelWorld>(DataAsset);
		NewWorld->MaterialCollection = LoadObject<UVoxelMaterialCollection>(NewWorld, TEXT("/Voxel/Examples/Materials/TriplanarExampleCollection/TriplanarExampleCollection"));
		NewWorld->VoxelMaterial = LoadObject<UMaterialInterface>(NewWorld, TEXT("/Voxel/Examples/Materials/RGB/M_VoxelMaterial_Colors"));
		NewWorld->TessellatedVoxelMaterial = LoadObject<UMaterialInterface>(NewWorld, TEXT("/Voxel/Examples/Materials/RGB/M_VoxelMaterial_Colors_Tess"));

		DataAsset->VoxelWorldTemplate = NewWorld;
		DataAsset->MarkPackageDirty();
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Template = DataAsset->VoxelWorldTemplate;
	World = PreviewScene.GetWorld()->SpawnActor<AVoxelWorld>(SpawnParameters);

	CreateWorld();
}

FVoxelDataAssetEditorManager::~FVoxelDataAssetEditorManager()
{
	World->GetData().ClearDirtyFlag(); // Avoid annoying save popup from the voxel world
	World->DestroyWorld();

	check(DataAsset->VoxelWorldTemplate);
	DataAsset->VoxelWorldTemplate->ReinitializeProperties(World);
}

void FVoxelDataAssetEditorManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(World);
	check(World);
}

AVoxelWorld& FVoxelDataAssetEditorManager::GetVoxelWorld() const
{
	check(World);
	return *World;
}

void FVoxelDataAssetEditorManager::Save(bool bShowDebug)
{
	FScopedSlowTask Progress(6);

	auto& Data = World->GetData();

	Progress.EnterProgressFrame(1, LOCTEXT("RoundingVoxels", "Rounding voxels"));
	if (GetDefault<UVoxelSettings>()->bRoundBeforeSaving)
	{
		UVoxelDataTools::RoundVoxels(World, FIntBox::Infinite);
	}

	Progress.EnterProgressFrame(1, LOCTEXT("FindingDirtyVoxels", "Finding dirty voxels"));
	FIntBoxWithValidity OptionalDirtyBounds;
	bool bHasMaterials = false;
	{
		FVoxelReadScopeLock Lock(Data, FIntBox::Infinite, "data asset save");
		FVoxelOctreeUtilities::IterateAllLeaves(Data.GetOctree(), [&](FVoxelDataOctreeLeaf& Leaf)
		{
			bHasMaterials |= Leaf.Materials.IsDirty();
			if (Leaf.Values.IsDirty() || Leaf.Materials.IsDirty())
			{
				OptionalDirtyBounds += Leaf.GetBounds();
			}
		});
	}

	// Should always have at least one dirty voxel, else it would mean that the original data asset had a size of 0 which is invalid
	if (!ensure(OptionalDirtyBounds.IsValid())) return;

	const auto DirtyBounds = OptionalDirtyBounds.GetBox();

	const bool bSubtractiveAsset = DataAsset->bSubtractiveAsset;

	Progress.EnterProgressFrame(1, LOCTEXT("FindingVoxelsToSave", "Finding voxels to save"));
	FIntBox BoundsToSave;
	TArray<FIntVector> PointsAlone;
	{
		FVoxelReadScopeLock Lock(Data, DirtyBounds, "data asset save");
		FVoxelConstDataAccelerator OctreeAccelerator(Data, DirtyBounds);
		DirtyBounds.Iterate([&](int32 X, int32 Y, int32 Z)
		{
			const FVoxelValue Value = OctreeAccelerator.Get<FVoxelValue>(X, Y, Z, 0);
			if ((bSubtractiveAsset && !Value.IsTotallyFull()) || (!bSubtractiveAsset && !Value.IsTotallyEmpty()))
			{
				if (!BoundsToSave.IsValid())
				{
					BoundsToSave = FIntBox(X, Y, Z);
				}
				else if (!BoundsToSave.Contains(X, Y, Z))
				{
					BoundsToSave = BoundsToSave + FIntVector(X, Y, Z);
					PointsAlone.Emplace(X, Y, Z);
				}
			}
		});
	}

	const FIntVector PositionOffset = BoundsToSave.Min;
	const FIntVector Size = BoundsToSave.Size();

	auto AssetData = DataAsset->MakeData();
	AssetData->SetSize(Size, bHasMaterials);

	{
		FVoxelReadScopeLock Lock(Data, BoundsToSave, "Data Asset Save");

		Progress.EnterProgressFrame(1, LOCTEXT("CopyingValues", "Copying values"));
		{
			TVoxelQueryZone<FVoxelValue> QueryZone(BoundsToSave, AssetData->GetRawValues());
			Data.Get<FVoxelValue>(QueryZone, 0);
		}

		Progress.EnterProgressFrame(1, LOCTEXT("CopyingMaterials", "Copying materials"));
		if (bHasMaterials)
		{
			TVoxelQueryZone<FVoxelMaterial> QueryZone(BoundsToSave, AssetData->GetRawMaterials());
			Data.Get<FVoxelMaterial>(QueryZone, 0);
		}
	}

	Progress.EnterProgressFrame(1, LOCTEXT("Compressing", "Compressing"));
	DataAsset->PositionOffset = PositionOffset;
	DataAsset->SetData(AssetData);

	Data.ClearDirtyFlag();

	UE_LOG(LogVoxel, Log, TEXT("Data asset saved. Has materials: %s"), bHasMaterials ? TEXT("yes") : TEXT("no"));

	if (bShowDebug)
	{
		UVoxelDebugUtilities::DrawDebugIntBox(World, BoundsToSave, 10, 100, FColor::Red);
		for (auto& Point : PointsAlone)
		{
			DrawDebugPoint(World->GetWorld(), World->LocalToGlobal(Point), 10, FColor::Magenta, false, 10);
		}
	}
}

void FVoxelDataAssetEditorManager::RecreateWorld()
{
	World->DestroyWorld();
	CreateWorld();
}

bool FVoxelDataAssetEditorManager::IsDirty() const
{
	return World->GetData().IsDirty();
}

void FVoxelDataAssetEditorManager::CreateWorld()
{
	check(!World->IsCreated());
	World->SetWorldGeneratorObject(DataAsset);
	World->CreateInEditor();
	UVoxelBlueprintLibrary::SetBoxAsDirty(World, DataAsset->GetBounds(), true, DataAsset->GetData()->HasMaterials());
}
#undef LOCTEXT_NAMESPACE