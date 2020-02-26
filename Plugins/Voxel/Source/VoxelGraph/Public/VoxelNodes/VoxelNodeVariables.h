// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "CppTranslation/VoxelVariables.h"

class UVoxelDataAsset;
class UTexture2D;
class UCurveFloat;
class UCurveLinearColor;
class UVoxelHeightmapAssetFloat;
class UVoxelHeightmapAssetUINT16;
class UVoxelNode_WorldGeneratorMerge;
struct FVoxelWorldGeneratorPicker;

class VOXELGRAPH_API FVoxelColorTextureVariable : public FVoxelExposedVariable
{
public:
	FVoxelColorTextureVariable(const UVoxelExposedNode& Node, UTexture2D* Texture);

	virtual FString GetLocalVariableFromExposedOne() const override;
};

class VOXELGRAPH_API FVoxelFloatTextureVariable : public FVoxelExposedVariable
{
public:
	FVoxelFloatTextureVariable(const UVoxelExposedNode& Node);

	virtual FString GetLocalVariableFromExposedOne() const override;
};

class VOXELGRAPH_API FVoxelCurveVariable : public FVoxelExposedVariable
{
public:
	FVoxelCurveVariable(const UVoxelExposedNode& Node, UCurveFloat* Curve);

	virtual FString GetLocalVariableFromExposedOne() const override;
};

class VOXELGRAPH_API FVoxelColorCurveVariable : public FVoxelExposedVariable
{
public:
	FVoxelColorCurveVariable(const UVoxelExposedNode& Node, UCurveLinearColor* Curve);

	virtual FString GetLocalVariableFromExposedOne() const override;
};

class VOXELGRAPH_API FVoxelHeightmapVariable : public FVoxelExposedVariable
{
public:
	FVoxelHeightmapVariable(const UVoxelExposedNode& Node, UVoxelHeightmapAssetFloat* Heightmap);
	FVoxelHeightmapVariable(const UVoxelExposedNode& Node, UVoxelHeightmapAssetUINT16* Heightmap);

	virtual FString GetLocalVariableFromExposedOne() const override;
};

class VOXELGRAPH_API FVoxelDataAssetVariable : public FVoxelExposedVariable
{
public:
	FVoxelDataAssetVariable(const UVoxelExposedNode& Node, UVoxelDataAsset* Asset);

	virtual FString GetLocalVariableFromExposedOne() const override;
};

class VOXELGRAPH_API FVoxelWorldGeneratorVariable : public FVoxelExposedVariable
{
public:
	FVoxelWorldGeneratorVariable(const UVoxelExposedNode& Node, const FVoxelWorldGeneratorPicker& WorldGenerator);

	virtual FString GetLocalVariableFromExposedOne() const override;
};

class VOXELGRAPH_API FVoxelWorldGeneratorArrayVariable : public FVoxelExposedVariable
{
public:
	FVoxelWorldGeneratorArrayVariable(const UVoxelExposedNode& Node, const TArray<FVoxelWorldGeneratorPicker>& WorldGenerators);

	virtual FString GetLocalVariableFromExposedOne() const override;
};

class VOXELGRAPH_API FVoxelMaterialObjectVariable : public FVoxelExposedVariable
{
public:
	FVoxelMaterialObjectVariable(const UVoxelExposedNode& Node, UObject* Object);

	virtual FString GetLocalVariableFromExposedOne() const override;
	virtual TMap<FName, FString> GetExposedVariableDefaultMetadata() const override;
};
