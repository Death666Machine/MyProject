// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/IToolkit.h"

class UVoxelDataAsset;
class UVoxelMaterialCollection;
class IToolkitHost;

class IVoxelEditorModule : public IModuleInterface
{
public:
	virtual void CreateVoxelDataAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UVoxelDataAsset* DataAsset) = 0;

	virtual bool GenerateSingleMaterials(UVoxelMaterialCollection* Collection, FString& OutError) = 0;
	virtual bool GenerateDoubleMaterials(UVoxelMaterialCollection* Collection, FString& OutError) = 0;
	virtual bool GenerateTripleMaterials(UVoxelMaterialCollection* Collection, FString& OutError) = 0;

	virtual void RefreshVoxelWorlds(UObject* MatchingGenerator = nullptr) = 0;
};