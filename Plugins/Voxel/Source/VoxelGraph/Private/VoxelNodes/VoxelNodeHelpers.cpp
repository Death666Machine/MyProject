// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelNodeHelpers.h"
#include "CppTranslation/VoxelCppIds.h"

void FVoxelNodeHelpers::ReplaceInputsOutputs(FString& S, const TArray<FString>& Inputs, const TArray<FString>& Outputs)
{
	TMap<FString, FString> Args;
	for (int32 I = 0; I < Inputs.Num(); I++)
	{
		Args.Add(TEXT("_I") + FString::FromInt(I), Inputs[I]);
	}
	for (int32 I = 0; I < Outputs.Num(); I++)
	{
		Args.Add(TEXT("_O") + FString::FromInt(I), Outputs[I]);
	}
	Args.Add(TEXT("_C0"), FVoxelCppIds::Context);

	TArray<FString> T;
	Args.GenerateKeyArray(T);
	for (auto& Key : T)
	{
		while (S.Contains(Key, ESearchCase::CaseSensitive))
		{
			S = S.Replace(*Key, *Args[Key], ESearchCase::CaseSensitive);
		}
	}
}

FString FVoxelNodeHelpers::GetPrefixOpLoopString(const TArray<FString>& Inputs, const TArray<FString>& Outputs, int32 InputCount, const FString& Op)
{
	check(InputCount > 0);

	FString Line;

	Line += Outputs[0] + " = ";
	for (int32 I = 0; I < InputCount - 1; I++)
	{
		Line += Op + "(" + Inputs[I] + ", ";
	}

	Line += Inputs[InputCount - 1];

	for (int32 I = 0; I < InputCount - 1; I++)
	{
		Line += ")";
	}

	Line += ";";

	return Line;
}

FString FVoxelNodeHelpers::GetInfixOpLoopString(const TArray<FString>& Inputs, const TArray<FString>& Outputs, int32 InputCount, const FString& Op)
{
	check(InputCount > 0);

	FString Line;

	Line += Outputs[0] + " = ";
	for (int32 I = 0; I < InputCount - 1; I++)
	{
		Line += Inputs[I] + " " + Op + " ";
	}
	Line += Inputs[InputCount - 1];
	Line += ";";

	return Line;
}