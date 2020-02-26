// Copyright 2020 Phyronnaz

#include "VoxelImporters/VoxelMeshImporter.h"

#include "VoxelAssets/VoxelDataAsset.h"
#include "VoxelMathUtilities.h"
#include "VoxelMessages.h"

#include "SDFGen/makelevelset3.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "UObject/ConstructorHelpers.h"

static void GetMergedSectionFromStaticMesh(
	UStaticMesh* InMesh,
	int32 LODIndex,
	TArray<FVector>& Vertices,
	TArray<FIntVector>& Triangles,
	TArray<FVector2D>& UVs)
{
	VOXEL_FUNCTION_COUNTER();

	if (!ensure(InMesh->RenderData) || !ensure(InMesh->RenderData->LODResources.IsValidIndex(LODIndex))) return;

	const bool bAllowCPUAccess = InMesh->bAllowCPUAccess;
	InMesh->bAllowCPUAccess = true;

	const FStaticMeshLODResources& LODResources = InMesh->RenderData->LODResources[LODIndex];
	const auto& IndexBuffer = LODResources.IndexBuffer;
	const auto& PositionVertexBuffer = LODResources.VertexBuffers.PositionVertexBuffer;
	const auto& StaticMeshVertexBuffer = LODResources.VertexBuffers.StaticMeshVertexBuffer;
	const int32 NumTextureCoordinates = StaticMeshVertexBuffer.GetNumTexCoords();

	Vertices.Reserve(PositionVertexBuffer.GetNumVertices());
	if (NumTextureCoordinates > 0)
	{
		UVs.Reserve(StaticMeshVertexBuffer.GetNumVertices());
	}
	Triangles.Reserve(IndexBuffer.GetNumIndices());

	TMap<int32, int32> MeshToNewVertices;
	MeshToNewVertices.Reserve(IndexBuffer.GetNumIndices());

	for (auto& Section : LODResources.Sections)
	{
		for (uint32 TriangleIndex = 0; TriangleIndex < Section.NumTriangles; TriangleIndex++)
		{
			FIntVector NewTriangle;
			for (uint32 TriangleVertexIndex = 0; TriangleVertexIndex < 3; TriangleVertexIndex++)
			{
				int32 IndexInNewVertices;
				{
					const int32 Index = IndexBuffer.GetIndex(Section.FirstIndex + 3 * TriangleIndex + TriangleVertexIndex);
					int32* NewIndexPtr = MeshToNewVertices.Find(Index);
					if (NewIndexPtr)
					{
						IndexInNewVertices = *NewIndexPtr;
					}
					else
					{
						const FVector Vertex = PositionVertexBuffer.VertexPosition(Index);
						IndexInNewVertices = Vertices.Add(Vertex);
						if (NumTextureCoordinates > 0)
						{
							const FVector2D UV = StaticMeshVertexBuffer.GetVertexUV(Index, 0);
							ensure(IndexInNewVertices == UVs.Add(UV));
						}
						MeshToNewVertices.Add(Index, IndexInNewVertices);
					}
				}
				NewTriangle[TriangleVertexIndex] = IndexInNewVertices;
			}
			Triangles.Add(NewTriangle);
		}
	}

	InMesh->bAllowCPUAccess = bAllowCPUAccess;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelMeshImporterSettings::FVoxelMeshImporterSettings()
{
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ColorMaterialFinder(TEXT("/Voxel/Examples/Importers/Chair/VoxelExample_M_Chair_Emissive_Color"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UVsMaterialFinder(TEXT("/Voxel/Examples/Importers/Chair/VoxelExample_M_Chair_Emissive_UVs"));
	ColorsMaterial = ColorMaterialFinder.Object;
	UVsMaterial = UVsMaterialFinder.Object;
}

void UVoxelMeshImporterLibrary::CreateMeshDataFromStaticMesh(UStaticMesh* StaticMesh, FVoxelMeshImporterInputData& Data)
{
	VOXEL_PRO_ONLY_VOID();
	VOXEL_FUNCTION_COUNTER();

	check(StaticMesh);
	Data.Vertices.Reset();
	Data.Triangles.Reset();
	Data.UVs.Reset();
	const int32 LOD = 0;
	GetMergedSectionFromStaticMesh(StaticMesh, LOD, Data.Vertices, Data.Triangles, Data.UVs);
}

bool UVoxelMeshImporterLibrary::ConvertMeshToVoxels(
	UObject* WorldContextObject,
	const FVoxelMeshImporterInputData& Mesh,
	const FTransform& Transform,
	const FVoxelMeshImporterSettings& Settings,
	FVoxelMeshImporterRenderTargetCache& RenderTargetCache,
	FVoxelDataAssetData& OutAsset,
	FIntVector& OutOffset,
	int32& OutNumLeaks)
{
	VOXEL_PRO_ONLY();
	VOXEL_FUNCTION_COUNTER();

	if (RenderTargetCache.LastRenderedRenderTargetSize != Settings.RenderTargetSize)
	{
		RenderTargetCache.LastRenderedColorsMaterial = nullptr;
		RenderTargetCache.LastRenderedUVsMaterial = nullptr;
		RenderTargetCache.ColorsRenderTarget = nullptr;
		RenderTargetCache.UVsRenderTarget = nullptr;
	}

	TVoxelTexture<FColor> ColorTexture;
	TVoxelTexture<FColor> UVTexture;
	if (Settings.bPaintColors)
	{
		if (!Settings.ColorsMaterial)
		{
			FVoxelMessages::Error(FUNCTION_ERROR("PaintColors = true but ColorsMaterial = nullptr"));
			return false;
		}
		if (!RenderTargetCache.ColorsRenderTarget || Settings.ColorsMaterial != RenderTargetCache.LastRenderedColorsMaterial)
		{
			FVoxelTextureUtilities::ClearCache(RenderTargetCache.ColorsRenderTarget);
			RenderTargetCache.LastRenderedColorsMaterial = Settings.ColorsMaterial;
			RenderTargetCache.ColorsRenderTarget = CreateTextureFromMaterial(
				WorldContextObject,
				Settings.ColorsMaterial,
				Settings.RenderTargetSize,
				Settings.RenderTargetSize);
		}
		ColorTexture = FVoxelTextureUtilities::CreateFromTexture_Color(RenderTargetCache.ColorsRenderTarget);
	}
	if (Settings.bPaintUVs)
	{
		if (!Settings.UVsMaterial)
		{
			FVoxelMessages::Error(FUNCTION_ERROR("PaintUVChannels = true but UVChannelsMaterial = nullptr"));
			return false;
		}
		if (!RenderTargetCache.UVsRenderTarget || Settings.UVsMaterial != RenderTargetCache.LastRenderedUVsMaterial)
		{
			FVoxelTextureUtilities::ClearCache(RenderTargetCache.UVsRenderTarget);
			RenderTargetCache.LastRenderedUVsMaterial = Settings.UVsMaterial;
			RenderTargetCache.UVsRenderTarget = CreateTextureFromMaterial(
				WorldContextObject,
				Settings.UVsMaterial,
				Settings.RenderTargetSize,
				Settings.RenderTargetSize);
		}
		UVTexture = FVoxelTextureUtilities::CreateFromTexture_Color(RenderTargetCache.UVsRenderTarget);
	}

	OutNumLeaks = 0;

	TArray<FVector> Vertices;
	Vertices.Reserve(Mesh.Vertices.Num());
	FBox Box(ForceInit);
	for (auto& Vertex : Mesh.Vertices)
	{
		const auto NewVertex = Transform.TransformPosition(Vertex);
		Vertices.Add(NewVertex);
		Box += NewVertex;
	}
	Box = Box.ExpandBy(Settings.VoxelSize);

	const FVector SizeFloat = Box.GetSize() / Settings.VoxelSize;
	const FIntVector Size(FVoxelUtilities::CeilToInt(SizeFloat));
	const FVector Origin = Box.Min;

	FMakeLevelSet3Settings LevelSet3Settings;
	LevelSet3Settings.Vertices = Vertices;
	LevelSet3Settings.UVs = Mesh.UVs;
	LevelSet3Settings.Triangles = Mesh.Triangles;
	LevelSet3Settings.bDoSweep = Settings.bComputeExactDistance;
	LevelSet3Settings.bComputeSign = true;
	LevelSet3Settings.bHideLeaks = Settings.bHideLeaks;
	LevelSet3Settings.Origin = Origin;
	LevelSet3Settings.Delta = Settings.VoxelSize;
	LevelSet3Settings.Size = Size;
	LevelSet3Settings.ExactBand = Settings.MaxVoxelDistanceFromTriangle;
	LevelSet3Settings.bExportUVs = false;
	if (Settings.bPaintColors)
	{
		LevelSet3Settings.ColorTextures.Add(ColorTexture);
	}
	if (Settings.bPaintUVs)
	{
		LevelSet3Settings.ColorTextures.Add(UVTexture);
	}

	TArray3<float> Result;
	TArray3<FVector2D> UVs;
	TArray<TArray3<FColor>> Colors;
	MakeLevelSet3(LevelSet3Settings, Result, UVs, Colors, OutNumLeaks);

	OutOffset = FVoxelUtilities::RoundToInt(Box.Min / Settings.VoxelSize);
	const bool bHasMaterials =
		Settings.bPaintColors ||
		Settings.bPaintUVs ||
		Settings.bSetSingleIndex ||
		Settings.bSetDoubleIndex;
	OutAsset.SetSize(Size, bHasMaterials);

	for (int32 X = 0; X < Size.X; X++)
	{
		for (int32 Y = 0; Y < Size.Y; Y++)
		{
			for (int32 Z = 0; Z < Size.Z; Z++)
			{
				OutAsset.SetValue(X, Y, Z, FVoxelValue((Result(X, Y, Z) / Settings.VoxelSize + Settings.DistanceFieldOffset) / Settings.DistanceDivisor));
				if (bHasMaterials)
				{
					FVoxelMaterial Material(ForceInit);
					if (Settings.bPaintColors)
					{
						const FColor Color = Colors[0](X, Y, Z);
						Material.SetColor(Color);
					}
					if (Settings.bPaintUVs)
					{
						const FColor Color = Colors.Last()(X, Y, Z);
						Material.SetU(uint8(0), Color.R);
						Material.SetV(uint8(0), Color.G);
						Material.SetU(uint8(1), Color.B);
						Material.SetV(uint8(1), Color.A);
					}
					if (Settings.bSetSingleIndex)
					{
						Material.SetSingleIndex_Index(Settings.SingleIndex);
					}
					if (Settings.bSetDoubleIndex)
					{
						Material.SetDoubleIndex_IndexA(0);
						Material.SetDoubleIndex_IndexB(Settings.DoubleIndex);
						Material.SetDoubleIndex_Blend_AsFloat(1.f);
					}
					OutAsset.SetMaterial(X, Y, Z, Material);
				}
			}
		}
	}

	return true;
}

UVoxelMeshImporterInputData* UVoxelMeshImporterLibrary::CreateMeshDataFromStaticMesh(UStaticMesh* StaticMesh)
{
	VOXEL_PRO_ONLY();
	VOXEL_FUNCTION_COUNTER();

	if (!StaticMesh)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Invalid StaticMesh"));
		return nullptr;
	}
	auto* Object = NewObject<UVoxelMeshImporterInputData>(GetTransientPackage());
	CreateMeshDataFromStaticMesh(StaticMesh, Object->Data);
	return Object;
}

UTextureRenderTarget2D* UVoxelMeshImporterLibrary::CreateTextureFromMaterial(
	UObject* WorldContextObject,
	UMaterialInterface* Material,
	int32 Width,
	int32 Height)
{
	VOXEL_PRO_ONLY();
	VOXEL_FUNCTION_COUNTER();

	if (!WorldContextObject)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Invalid WorldContextObject"));
		return nullptr;
	}
	if (!Material)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Invalid Material"));
		return nullptr;
	}
	if (Width <= 0)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Width <= 0"));
		return nullptr;
	}
	if (Height <= 0)
	{
		FVoxelMessages::Error(FUNCTION_ERROR("Height <= 0"));
		return nullptr;
	}

	UTextureRenderTarget2D* RenderTarget2D = UKismetRenderingLibrary::CreateRenderTarget2D(WorldContextObject, Width, Height, ETextureRenderTargetFormat::RTF_RGBA8);
	UKismetRenderingLibrary::DrawMaterialToRenderTarget(WorldContextObject, RenderTarget2D, Material);
	return RenderTarget2D;
}

void UVoxelMeshImporterLibrary::ConvertMeshToVoxels(
	UObject* WorldContextObject,
	UVoxelMeshImporterInputData* Mesh,
	FTransform Transform,
	bool bSubtractive,
	FVoxelMeshImporterSettings Settings,
	FVoxelMeshImporterRenderTargetCache& RenderTargetCache,
	UVoxelDataAsset*& Asset,
	int32& NumLeaks)
{
	VOXEL_PRO_ONLY_VOID();
	VOXEL_FUNCTION_COUNTER();

	if (!Mesh)
	{
		FVoxelMessages::Error(NSLOCTEXT("Voxel", "", "ConvertMeshToVoxels: Invalid Mesh"));
		return;
	}

	if (bSubtractive)
	{
		Settings.DistanceDivisor *= -1;
	}

	Asset = NewObject<UVoxelDataAsset>(GetTransientPackage());
	Asset->bSubtractiveAsset = bSubtractive;
	const auto Data = Asset->MakeData();
	if (!ConvertMeshToVoxels(WorldContextObject, Mesh->Data, Transform, Settings, RenderTargetCache, *Data, Asset->PositionOffset, NumLeaks))
	{
		return;
	}
	Asset->SetData(Data);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AVoxelMeshImporter::AVoxelMeshImporter()
{
#if WITH_EDITOR
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("Mesh");
	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(TEXT("/Voxel/Examples/Importers/Chair/VoxelExample_SM_Chair"));

	StaticMesh = MeshFinder.Object;
	MeshComponent->SetStaticMesh(StaticMesh);
	MeshComponent->SetRelativeScale3D(FVector(100.f));
	RootComponent = MeshComponent;

	PrimaryActorTick.bCanEverTick = true;
#endif
}

void AVoxelMeshImporter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (GetWorld()->WorldType != EWorldType::Editor)
	{
		Destroy();
	}

	if (StaticMesh)
	{
		if (CachedStaticMesh != StaticMesh)
		{
			CachedStaticMesh = StaticMesh;

			TArray<FIntVector> Triangles;
			TArray<FVector2D> UVs;
			CachedVertices.Reset();
			GetMergedSectionFromStaticMesh(StaticMesh, 0, CachedVertices, Triangles, UVs);
		}

		// TODO: Use PostEditMove
		const FTransform Transform = GetTransform();
		if (CachedTransform.ToMatrixWithScale() != Transform.ToMatrixWithScale())
		{
			CachedTransform = Transform;

			CachedBox = FBox(ForceInit);
			for (auto& Vertex : CachedVertices)
			{
				CachedBox += Transform.TransformPosition(Vertex);
			}
			CachedBox = CachedBox.ExpandBy(Settings.VoxelSize);

			InitMaterialInstance();
			MaterialInstance->SetVectorParameterValue("Offset", CachedBox.Min);
		}

		UpdateSizes();
	}
}

#if WITH_EDITOR
void AVoxelMeshImporter::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	MeshComponent->SetStaticMesh(StaticMesh);
	InitMaterialInstance();
	MaterialInstance->SetScalarParameterValue("VoxelSize", Settings.VoxelSize);
	UpdateSizes();
}
#endif

void AVoxelMeshImporter::InitMaterialInstance()
{
	if (MaterialInstance)
	{
		return;
	}
	auto* Material = LoadObject<UMaterial>(GetTransientPackage(), TEXT("Material'/Voxel/MaterialHelpers/MeshImporterMaterial.MeshImporterMaterial'"));
	MaterialInstance = UMaterialInstanceDynamic::Create(Material, GetTransientPackage());
	MeshComponent->SetMaterial(0, MaterialInstance);
	MaterialInstance->SetScalarParameterValue("VoxelSize", Settings.VoxelSize); // To have it on start
}

void AVoxelMeshImporter::UpdateSizes()
{
	const FVector SizeFloat = CachedBox.GetSize() / Settings.VoxelSize;
	SizeX = FMath::CeilToInt(SizeFloat.X);
	SizeY = FMath::CeilToInt(SizeFloat.Y);
	SizeZ = FMath::CeilToInt(SizeFloat.Z);
	NumberOfVoxels = SizeX * SizeY * SizeZ;
	const bool bHasMaterials =
		Settings.bPaintColors ||
		Settings.bPaintUVs ||
		Settings.bSetSingleIndex ||
		Settings.bSetDoubleIndex;
	SizeInMB = double(NumberOfVoxels) * (sizeof(FVoxelValue) + (bHasMaterials ? sizeof(FVoxelMaterial) : 0)) / double(1 << 20);
}