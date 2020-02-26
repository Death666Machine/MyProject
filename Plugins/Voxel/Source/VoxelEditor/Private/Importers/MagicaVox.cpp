// Copyright 2020 Phyronnaz

#include "Importers/MagicaVox.h"
#include "VoxelAssets/VoxelDataAsset.h"
#include "Misc/FileHelper.h"
#include "Misc/MessageDialog.h"
#include "IImageWrapperModule.h"
#include "IImageWrapper.h"
#include "Modules/ModuleManager.h"

inline int32 ReadInt(const TArray<uint8>& Bytes, int32& Position)
{
	int32 Result = Bytes[Position] + 256 * Bytes[Position + 1] + 256 * 256 * Bytes[Position + 2] + 256 * 256 * 256 * Bytes[Position + 3];
	Position += 4;
	return Result;
}

inline bool ReadString(const TArray<uint8>& Bytes, int32& Position, const FString& Chars)
{
	TCHAR Start[4];
	for (int32 i = 0; i < 4; i++)
	{
		Start[i] = Bytes[Position];
		Position++;
	}
	return Start[0] == Chars[0] && Start[1] == Chars[1] && Start[2] == Chars[2] && Start[3] == Chars[3];
}

inline uint8 ReadByte(const TArray<uint8>& Bytes, int32& Position)
{
	uint8 Result = Bytes[Position];
	Position++;
	return Result;
}

bool MagicaVox::ImportToAsset(const FString& Filename, const FString& PaletteFilename, bool bUsePalette, FVoxelDataAssetData& Asset)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	auto ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> PaletteBytes;
	TArray<FColor> PaletteColors;
	if (bUsePalette)
	{
		if (!FFileHelper::LoadFileToArray(PaletteBytes, *PaletteFilename))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Error when opening the palette file")));
			return false;
		}
		if (!ImageWrapper->SetCompressed(PaletteBytes.GetData(), PaletteBytes.Num()))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Error when reading the palette file")));
			return false;
		}
		if (ImageWrapper->GetWidth() != 256 || ImageWrapper->GetHeight() != 1)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FString::Printf(
				TEXT("Wrong palette size: should be (256, 1), is (%d, %d)"),
				ImageWrapper->GetWidth(),
				ImageWrapper->GetHeight())));
			return false;
		}
		if (ImageWrapper->GetBitDepth() != 8)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("The palette must be an 8 bit PNG")));
			return false;
		}
		const TArray<uint8>* RawData = nullptr;
		if (!ImageWrapper->GetRaw(ERGBFormat::RGBA, 8, RawData))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Error when reading the palette file")));
			return false;
		}
		auto& Data = *RawData;
		if (Data.Num() != 256 * 4)
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Error when reading the palette file (internal error)")));
			return false;
		}
		for (int32 Index = 0; Index < 256; Index++)
		{
			PaletteColors.Emplace(
				Data[4 * Index + 0],
				Data[4 * Index + 1],
				Data[4 * Index + 2],
				Data[4 * Index + 3]);
		}
	}

	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *Filename))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Error when opening the file")));
		return false;
	}

	int32 Position = 0;

	if (!ReadString(Bytes, Position, TEXT("VOX ")))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("File is corrupted")));
		return false;
	}

	int32 Version = ReadInt(Bytes, Position);

	if (!ReadString(Bytes, Position, TEXT("MAIN")))
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("File is corrupted")));
		return false;
	}

	Position += 8; // Unknown

	int32 PackCount;
	if (ReadString(Bytes, Position, TEXT("PACK")))
	{
		Position += 8; // Unknown
		PackCount = ReadInt(Bytes, Position);
	}
	else
	{
		Position -= 4;
		PackCount = 1;
	}

	for (int32 i = 0; i < 1 /*PackCount*/; i++) // TODO MAGICA VOX IMPROVEMENT: PackCount != 1
	{
		if (!ReadString(Bytes, Position, TEXT("SIZE")))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("File is corrupted")));
			return false;
		}
		Position += 8; // Unknown

		const int32 SizeX = ReadInt(Bytes, Position);
		const int32 SizeY = ReadInt(Bytes, Position);
		const int32 SizeZ = ReadInt(Bytes, Position);

		Asset.SetSize(FIntVector(SizeY, SizeX, SizeZ), true);

		TArray<bool> Blocks;
		TArray<uint8> Colors;
		Blocks.SetNum(SizeX * SizeY * SizeZ);
		Colors.SetNum(SizeX * SizeY * SizeZ);

		if (!ReadString(Bytes, Position, TEXT("XYZI")))
		{
			FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("File is corrupted")));
			return false;
		}
		Position += 8; // Unknown

		const int32 N = ReadInt(Bytes, Position);

		for (int32 K = 0; K < N; K++)
		{
			int32 X = ReadByte(Bytes, Position);
			int32 Y = ReadByte(Bytes, Position);
			int32 Z = ReadByte(Bytes, Position);
			int32 Color = ReadByte(Bytes, Position);

			check(Color > 0);
			Blocks[X + SizeX * Y + SizeX * SizeY * Z] = true;
			Colors[X + SizeX * Y + SizeX * SizeY * Z] = Color - 1;
		}

		for (int32 Z = 0; Z < SizeZ; Z++)
		{
			for (int32 Y = 0; Y < SizeY; Y++)
			{
				for (int32 X = 0; X < SizeX; X++)
				{
					int32 Index = X + SizeX * Y + SizeX * SizeY * Z;

					Asset.SetValue(Y, X, Z, Blocks[Index] ? FVoxelValue::Full() : FVoxelValue::Empty());
					FVoxelMaterial Material(ForceInit);
					if (bUsePalette)
					{
						Material.SetColor(PaletteColors[Colors[Index]]);
					}
					else
					{
						Material.SetSingleIndex_Index(Colors[Index]);
					}
					Asset.SetMaterial(Y, X, Z, Material);
				}
			}
		}
	}

	return true;
}