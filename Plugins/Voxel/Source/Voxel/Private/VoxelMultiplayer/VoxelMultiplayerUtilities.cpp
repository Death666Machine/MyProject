// Copyright 2020 Phyronnaz

#include "VoxelMultiplayer/VoxelMultiplayerUtilities.h"
#include "VoxelData/VoxelSave.h"
#include "VoxelSerializationUtilities.h"
#include "VoxelGlobals.h"
#include "VoxelValue.h"
#include "VoxelMaterial.h"

#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

inline void SerializeDataHeader(FArchive& Archive, bool& bValues, uint32& ItemCount)
{
	Archive << bValues << ItemCount;
}

void FVoxelMultiplayerUtilities::ReadDiffs(const TArray<uint8>& Data, TArray<TVoxelChunkDiff<FVoxelValue>>& OutValueDiffs, TArray<TVoxelChunkDiff<FVoxelMaterial>>& OutMaterialDiffs)
{
	VOXEL_FUNCTION_COUNTER();

	check(Data.Num() > 0);

	TArray<uint8> UncompressedData;
	FVoxelSerializationUtilities::DecompressData(Data, UncompressedData);
	check(UncompressedData.Num() >= sizeof(bool) + sizeof(uint32));

	FMemoryReader Reader(UncompressedData);

	bool bValues;
	uint32 ItemCount;
	SerializeDataHeader(Reader, bValues, ItemCount);

	if (bValues)
	{
		for (uint32 Index = 0; Index < ItemCount; Index++)
		{
			TVoxelChunkDiff<FVoxelValue> Diff;
			Reader << Diff;
			OutValueDiffs.Add(Diff);
		}
	}
	else
	{
		for (uint32 Index = 0; Index < ItemCount; Index++)
		{
			TVoxelChunkDiff<FVoxelMaterial> Diff;
			Reader << Diff;
			OutMaterialDiffs.Add(Diff);
		}
	}
}

template<typename T>
void WriteDiffsImpl(TArray<uint8>& Data, const TArray<TVoxelChunkDiff<T>>& Diffs)
{
	VOXEL_FUNCTION_COUNTER();

	TArray<uint8> UncompressedData;
	FMemoryWriter Writer(UncompressedData);

	bool bValue = TIsSame<T, FVoxelValue>::Value;
	uint32 SizeToSend = Diffs.Num();

	check((bValue || TIsSame<T, FVoxelMaterial>::Value));

	SerializeDataHeader(Writer, bValue, SizeToSend);

	for (uint32 Index = 0; Index < SizeToSend; Index++)
	{
		Writer << const_cast<TVoxelChunkDiff<T>&>(Diffs[Index]);
	}

	TArray<uint8> CompressedData;
	FVoxelSerializationUtilities::CompressData(UncompressedData, CompressedData, FVoxelMultiplayerUtilities::CompressionFlags);
	Data.Append(CompressedData);
}

void FVoxelMultiplayerUtilities::WriteDiffs(TArray<uint8>& Data, const TArray<TVoxelChunkDiff<FVoxelValue>>& ValueDiffs, const TArray<TVoxelChunkDiff<FVoxelMaterial>>& MaterialDiffs)
{
	VOXEL_FUNCTION_COUNTER();

	if (ValueDiffs.Num() > 0)
	{
		WriteDiffsImpl(Data, ValueDiffs);
	}
	if (MaterialDiffs.Num() > 0)
	{
		WriteDiffsImpl(Data, MaterialDiffs);
	}
}

void FVoxelMultiplayerUtilities::ReadSave(const TArray<uint8>& Data, FVoxelCompressedWorldSave& OutSave)
{
	VOXEL_FUNCTION_COUNTER();

	FMemoryReader Reader(Data);
	OutSave.Serialize(Reader);
}

void FVoxelMultiplayerUtilities::WriteSave(TArray<uint8>& Data, const FVoxelCompressedWorldSave& Save)
{
	VOXEL_FUNCTION_COUNTER();

	FMemoryWriter Writer(Data);
	const_cast<FVoxelCompressedWorldSave&>(Save).Serialize(Writer);
}