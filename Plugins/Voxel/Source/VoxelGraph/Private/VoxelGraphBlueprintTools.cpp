// Copyright 2020 Phyronnaz

#include "VoxelGraphBlueprintTools.h"
#include "VoxelWorldGenerator.h"
#include "VoxelWorldGeneratorInstance.h"
#include "VoxelGraphGenerator.h"
#include "VoxelMessages.h"

#define LOCTEXT_NAMESPACE "Voxel"

#define CHECK_WORLDGENERATOR() \
	if (!WorldGenerator) \
	{ \
		FVoxelMessages::Error(FUNCTION_ERROR("Invalid WorldGenerator!")); \
		return; \
	}

#define CANNOT_FIND_PROPERTY() \
			FVoxelMessages::Error( \
				FText::Format(LOCTEXT("CouldNotFindProperty", "{0}: Could not find property {1}!"), \
					FText::FromString(__FUNCTION__), \
					FText::FromName(UniqueName)));

void UVoxelGraphBlueprintTools::SetVoxelGraphFloatParameter(UVoxelWorldGenerator* WorldGenerator, FName UniqueName, float Value)
{
	VOXEL_PRO_ONLY_VOID();
	CHECK_WORLDGENERATOR();

	if (WorldGenerator->IsA(UVoxelGraphGenerator::StaticClass()))
	{
		CastChecked<UVoxelGraphGenerator>(WorldGenerator)->FloatParameters.Add(UniqueName, Value);
	}
	else
	{
		UFloatProperty* Prop = FindField<UFloatProperty>(WorldGenerator->GetClass(), UniqueName);
		if (Prop)
		{
			Prop->SetPropertyValue_InContainer(WorldGenerator, Value);
		}
		else
		{
			CANNOT_FIND_PROPERTY();
		}
	}
}

void UVoxelGraphBlueprintTools::SetVoxelGraphIntParameter(UVoxelWorldGenerator* WorldGenerator, FName UniqueName, int32 Value)
{
	VOXEL_PRO_ONLY_VOID();
	CHECK_WORLDGENERATOR();

	if (WorldGenerator->IsA(UVoxelGraphGenerator::StaticClass()))
	{
		CastChecked<UVoxelGraphGenerator>(WorldGenerator)->IntParameters.Add(UniqueName, Value);
	}
	else
	{
		UIntProperty* Prop = FindField<UIntProperty>(WorldGenerator->GetClass(), UniqueName);
		if (Prop)
		{
			Prop->SetPropertyValue_InContainer(WorldGenerator, Value);
		}
		else
		{
			CANNOT_FIND_PROPERTY();
		}
	}
}

void UVoxelGraphBlueprintTools::SetVoxelGraphBoolParameter(UVoxelWorldGenerator* WorldGenerator, FName UniqueName, bool Value)
{
	VOXEL_PRO_ONLY_VOID();
	CHECK_WORLDGENERATOR();

	if (WorldGenerator->IsA(UVoxelGraphGenerator::StaticClass()))
	{
		CastChecked<UVoxelGraphGenerator>(WorldGenerator)->BoolParameters.Add(UniqueName, Value);
	}
	else
	{
		UBoolProperty* Prop = FindField<UBoolProperty>(WorldGenerator->GetClass(), UniqueName);
		if (Prop)
		{
			Prop->SetPropertyValue_InContainer(WorldGenerator, Value);
		}
		else
		{
			CANNOT_FIND_PROPERTY();
		}
	}
}

void UVoxelGraphBlueprintTools::SetVoxelGraphColorParameter(UVoxelWorldGenerator* WorldGenerator, FName UniqueName, FLinearColor Value)
{
	VOXEL_PRO_ONLY_VOID();
	CHECK_WORLDGENERATOR();

	if (WorldGenerator->IsA(UVoxelGraphGenerator::StaticClass()))
	{
		CastChecked<UVoxelGraphGenerator>(WorldGenerator)->ColorParameters.Add(UniqueName, Value);
	}
	else
	{
		UStructProperty* Prop = FindField<UStructProperty>(WorldGenerator->GetClass(), UniqueName);
		if (Prop && Prop->GetCPPType(nullptr, 0) == "FLinearColor")
		{
			*Prop->ContainerPtrToValuePtr<FLinearColor>(WorldGenerator) = Value;
		}
		else
		{
			CANNOT_FIND_PROPERTY();
		}
	}
}

void UVoxelGraphBlueprintTools::SetVoxelGraphVoxelTextureParameter(UVoxelWorldGenerator* WorldGenerator, FName UniqueName, FVoxelFloatTexture Value)
{
	VOXEL_PRO_ONLY_VOID();
	CHECK_WORLDGENERATOR();

	if (WorldGenerator->IsA(UVoxelGraphGenerator::StaticClass()))
	{
		CastChecked<UVoxelGraphGenerator>(WorldGenerator)->VoxelTextureParameters.Add(UniqueName, Value);
	}
	else
	{
		UStructProperty* Prop = FindField<UStructProperty>(WorldGenerator->GetClass(), UniqueName);
		if (Prop && Prop->GetCPPType(nullptr, 0) == "FVoxelFloatTexture")
		{
			*Prop->ContainerPtrToValuePtr<FVoxelFloatTexture>(WorldGenerator) = Value;
		}
		else
		{
			CANNOT_FIND_PROPERTY();
		}
	}
}

void UVoxelGraphBlueprintTools::ClearVoxelGraphParametersOverrides(UVoxelWorldGenerator* WorldGenerator)
{
	VOXEL_PRO_ONLY_VOID();
	auto* Result = Cast<UVoxelGraphGenerator>(WorldGenerator);
	if (Result)
	{
		Result->ClearParametersOverrides();
	}
}

#undef LOCTEXT_NAMESPACE