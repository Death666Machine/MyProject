// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelNodeVariables.h"
#include "CppTranslation/VoxelCppUtils.h"

#include "VoxelAssets/VoxelHeightmapAsset.h"
#include "VoxelAssets/VoxelDataAsset.h"
#include "VoxelWorldGeneratorPicker.h"

#include "Engine/Texture2D.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"

FVoxelColorTextureVariable::FVoxelColorTextureVariable(const UVoxelExposedNode& Node, UTexture2D* Texture)
	: FVoxelExposedVariable(
		Node,
		"TVoxelTexture<FColor>",
		FVoxelCppUtils::SoftObjectPtrString<UTexture2D>(),
		FVoxelCppUtils::ObjectDefaultString(Texture))
{
}

FString FVoxelColorTextureVariable::GetLocalVariableFromExposedOne() const
{
	return "FVoxelTextureUtilities::CreateFromTexture(" + FVoxelCppUtils::LoadObjectString(Name) + ")";
}

///////////////////////////////////////////////////////////////////////////////

FVoxelFloatTextureVariable::FVoxelFloatTextureVariable(const UVoxelExposedNode& Node)
	: FVoxelExposedVariable(
		Node,
		"TVoxelTexture<float>",
		"FVoxelFloatTexture",
		"")
{
}

FString FVoxelFloatTextureVariable::GetLocalVariableFromExposedOne() const
{
	return Name + ".Texture";
}

///////////////////////////////////////////////////////////////////////////////

FVoxelCurveVariable::FVoxelCurveVariable(const UVoxelExposedNode& Node, UCurveFloat* Curve)
	: FVoxelExposedVariable(
		Node,
		"FVoxelRichCurve",
		FVoxelCppUtils::SoftObjectPtrString<UCurveFloat>(),
		FVoxelCppUtils::ObjectDefaultString(Curve))
{
}

FString FVoxelCurveVariable::GetLocalVariableFromExposedOne() const
{
	return FString::Printf(TEXT("FVoxelRichCurve(%s)"), *FVoxelCppUtils::LoadObjectString(Name));
}

///////////////////////////////////////////////////////////////////////////////

FVoxelColorCurveVariable::FVoxelColorCurveVariable(const UVoxelExposedNode& Node, UCurveLinearColor* Curve)
	: FVoxelExposedVariable(
		Node,
		"FVoxelColorRichCurve",
		FVoxelCppUtils::SoftObjectPtrString<UCurveLinearColor>(),
		FVoxelCppUtils::ObjectDefaultString(Curve))
{
}

FString FVoxelColorCurveVariable::GetLocalVariableFromExposedOne() const
{
	return FString::Printf(TEXT("FVoxelColorRichCurve(%s)"), *FVoxelCppUtils::LoadObjectString(Name));
}

///////////////////////////////////////////////////////////////////////////////

FVoxelHeightmapVariable::FVoxelHeightmapVariable(const UVoxelExposedNode& Node, UVoxelHeightmapAssetFloat* Heightmap)
	: FVoxelExposedVariable(
		Node,
		"TVoxelHeightmapAssetSamplerWrapper<float>",
		FVoxelCppUtils::SoftObjectPtrString<UVoxelHeightmapAssetFloat>(),
		FVoxelCppUtils::ObjectDefaultString(Heightmap))
{
}

FVoxelHeightmapVariable::FVoxelHeightmapVariable(const UVoxelExposedNode& Node, UVoxelHeightmapAssetUINT16* Heightmap)
	: FVoxelExposedVariable(
		Node,
		"TVoxelHeightmapAssetSamplerWrapper<uint16>",
		FVoxelCppUtils::SoftObjectPtrString<UVoxelHeightmapAssetUINT16>(),
		FVoxelCppUtils::ObjectDefaultString(Heightmap))
{
}

FString FVoxelHeightmapVariable::GetLocalVariableFromExposedOne() const
{
	return Name;
}

///////////////////////////////////////////////////////////////////////////////

FVoxelDataAssetVariable::FVoxelDataAssetVariable(const UVoxelExposedNode & Node, UVoxelDataAsset * Asset)
	: FVoxelExposedVariable(
		Node,
		"TVoxelSharedRef<const FVoxelDataAssetData>",
		FVoxelCppUtils::SoftObjectPtrString<UVoxelDataAsset>(),
		FVoxelCppUtils::ObjectDefaultString(Asset))
{
}

FString FVoxelDataAssetVariable::GetLocalVariableFromExposedOne() const
{
	return FString::Printf(
		TEXT("%s ? %s->GetData() : MakeVoxelShared<FVoxelDataAssetData>(nullptr)"),
		*FVoxelCppUtils::LoadObjectString(Name),
		*FVoxelCppUtils::LoadObjectString(Name));
}

///////////////////////////////////////////////////////////////////////////////

FVoxelWorldGeneratorVariable::FVoxelWorldGeneratorVariable(const UVoxelExposedNode& Node, const FVoxelWorldGeneratorPicker& WorldGenerator)
	: FVoxelExposedVariable(
		Node,
		"TVoxelSharedRef<FVoxelWorldGeneratorInstance>",
		"FVoxelWorldGeneratorPicker",
		FVoxelCppUtils::PickerDefaultString(WorldGenerator))
{
}

FString FVoxelWorldGeneratorVariable::GetLocalVariableFromExposedOne() const
{
	return Name + ".GetInstance()";
}

///////////////////////////////////////////////////////////////////////////////

inline FString GetGeneratorArraysDefaultValue(const TArray<FVoxelWorldGeneratorPicker>& Pickers)
{
	FString Result = "{\n";
	for (auto& Picker : Pickers)
	{
		Result += "\t\t" + FVoxelCppUtils::PickerDefaultString(Picker) + ",\n";
	}
	Result += "\t}";
	return Result;
}

FVoxelWorldGeneratorArrayVariable::FVoxelWorldGeneratorArrayVariable(const UVoxelExposedNode& Node, const TArray<FVoxelWorldGeneratorPicker>& WorldGenerators)
	: FVoxelExposedVariable(
		Node,
		"TArray<TVoxelSharedPtr<FVoxelWorldGeneratorInstance>>",
		"TArray<FVoxelWorldGeneratorPicker>",
		GetGeneratorArraysDefaultValue(WorldGenerators))
{
}

FString FVoxelWorldGeneratorArrayVariable::GetLocalVariableFromExposedOne() const
{
	return "FVoxelNodeFunctions::CreateWorldGeneratorArray(" + Name + ")";
}

///////////////////////////////////////////////////////////////////////////////

FVoxelMaterialObjectVariable::FVoxelMaterialObjectVariable(const UVoxelExposedNode& Node, UObject* Object)
	: FVoxelExposedVariable(
		Node,
		"FName",
		FVoxelCppUtils::SoftObjectPtrString<UObject>(),
		FVoxelCppUtils::ObjectDefaultString(Object))
{
}

FString FVoxelMaterialObjectVariable::GetLocalVariableFromExposedOne() const
{
	return "*" + Name + ".GetAssetName()";
}

TMap<FName, FString> FVoxelMaterialObjectVariable::GetExposedVariableDefaultMetadata() const
{
	return { { "AllowedClasses","MaterialFunction,MaterialInstanceConstant" } };
}