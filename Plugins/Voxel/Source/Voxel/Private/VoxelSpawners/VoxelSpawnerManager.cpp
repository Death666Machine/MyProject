// Copyright 2020 Phyronnaz

#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelSpawners/VoxelSpawnerUtilities.h"
#include "VoxelSpawners/VoxelSpawner.h"
#include "VoxelSpawners/VoxelSpawnerEmbreeRayHandler.h"
#include "VoxelSpawners/VoxelSpawnerRayHandler.h"
#include "VoxelProcGen/VoxelProcGenManager.h"
#include "VoxelData/VoxelData.h"
#include "VoxelData/VoxelDataAccelerator.h"
#include "VoxelRender/IVoxelRenderer.h"
#include "VoxelDebug/VoxelDebugManager.h"
#include "VoxelWorld.h"
#include "IVoxelPool.h"
#include "VoxelAsyncWork.h"
#include "VoxelPriorityHandler.h"
#include "VoxelMessages.h"
#include "VoxelThreadingUtilities.h"

#include "DrawDebugHelpers.h"
#include "Async/Async.h"

static TAutoConsoleVariable<int32> CVarShowVoxelSpawnerRays(
	TEXT("voxel.spawners.ShowRays"),
	0,
	TEXT("If true, will show the voxel spawner rays"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarShowVoxelSpawnerHits(
	TEXT("voxel.spawners.ShowHits"),
	0,
	TEXT("If true, will show the voxel spawner rays hits"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarShowVoxelSpawnerPositions(
	TEXT("voxel.spawners.ShowPositions"),
	0,
	TEXT("If true, will show the positions sent to spawners"),
	ECVF_Default);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template<bool bIsHeightTask>
class TVoxelFoliageBuildTask : public FVoxelAsyncWork
{
public:
	const TVoxelWeakPtr<FVoxelSpawnerManager> SpawnerManagerPtr;
	const FIntBox Bounds;
	const int32 Index;
	const FVoxelPriorityHandler PriorityHandler;

	TVoxelFoliageBuildTask(
		FVoxelSpawnerManager& SpawnerManager,
		const FIntBox& Bounds,
		int32 Index)
		: FVoxelAsyncWork(STATIC_FNAME("Foliage Build"), SpawnerManager.Settings.PriorityDuration, true)
		, SpawnerManagerPtr(SpawnerManager.AsShared())
		, Bounds(Bounds)
		, Index(Index)
		, PriorityHandler(Bounds, SpawnerManager.Settings.Renderer->GetInvokersPositions())
	{
	}

	virtual void DoWork() override
	{
		auto SpawnerManager = SpawnerManagerPtr.Pin();
		if (!SpawnerManager.IsValid()) return;

		if (bIsHeightTask)
		{
			SpawnerManager->SpawnHeightGroup_AnyThread(Bounds, Index);
		}
		else
		{
			SpawnerManager->SpawnRayGroup_AnyThread(Bounds, Index);
		}

		SpawnerManager->FlushAnyThreadQueue();

		FVoxelUtilities::DeleteOnGameThread_AnyThread(SpawnerManager);
	}
	virtual uint32 GetPriority() const override
	{
		return PriorityHandler.GetPriority();
	}
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSpawnerSettings::FVoxelSpawnerSettings(
	const AVoxelWorld* World,
	const TVoxelSharedRef<IVoxelPool>& Pool,
	const TVoxelSharedRef<FVoxelDebugManager>& DebugManager,
	const TVoxelSharedRef<FVoxelData>& Data,
	const TVoxelSharedRef<IVoxelLODManager>& LODManager,
	const TVoxelSharedRef<IVoxelRenderer>& Renderer,
	const TVoxelSharedRef<FVoxelInstancedMeshManager>& MeshManager,
	const TVoxelSharedRef<FVoxelProcGenManager>& ProcGenManager)
	: VoxelWorldInterface(World)
	, Pool(Pool)
	, DebugManager(DebugManager)
	, Data(Data)
	, MeshManager(MeshManager)
	, ProcGenManager(ProcGenManager)
	, LODManager(LODManager)
	, Renderer(Renderer)
	, Config(World->SpawnerConfig)
	, VoxelSize(World->VoxelSize)
	, Seeds(World->Seeds)
	, PriorityDuration(World->PriorityDuration)
{
}

TVoxelSharedRef<FVoxelSpawnerManager> FVoxelSpawnerManager::Create(const FVoxelSpawnerSettings& Settings)
{
	VOXEL_FUNCTION_COUNTER();

	TVoxelSharedRef<FVoxelSpawnerManager> Manager = MakeShareable(new FVoxelSpawnerManager(Settings));

	if (!Settings.Config.IsValid()) return Manager;

	UVoxelSpawnerConfig& Config = *Settings.Config;
	FVoxelSpawnerThreadSafeConfig& ThreadSafeConfig = Manager->ThreadSafeConfig;

	ThreadSafeConfig.WorldType = Config.WorldType;
	ThreadSafeConfig.RayGroups = Config.RaySpawners;
	ThreadSafeConfig.HeightGroups = Config.HeightSpawners;

	const auto ResetManager = [&]()
	{
		ThreadSafeConfig.RayGroups.Reset();
		ThreadSafeConfig.HeightGroups.Reset();
	};

	TSet<UVoxelSpawner*> Spawners;
	// Setup elements, and gather their spawners
	{
		const auto& WorldGenerator = *Settings.Data->WorldGenerator;

		const auto CheckFloatOutputExists = [&](FName Name)
		{
			if (!WorldGenerator.FloatOutputsPtr.Contains(Name))
			{
				FString Types;
				for (auto& It : WorldGenerator.FloatOutputsPtr)
				{
					if (!Types.IsEmpty()) Types += ", ";
					Types += It.Key.ToString();
				}
				FVoxelMessages::Warning(
					FString::Printf(TEXT("No voxel graph output named %s and with type float found! Valid names: %s"),
						*Name.ToString(),
						*Types),
					&Config);
			}
		};
		const auto SetupElement = [&](auto& Element)
		{
			if (!Element.Spawner)
			{
				FVoxelMessages::Error("Spawner is null!", &Config);
				return false;
			}

			Spawners.Add(Element.Spawner);

			Element.DistanceBetweenInstancesInVoxel = Element.Spawner->GetDistanceBetweenInstancesInVoxel();

			if (Element.DensityGraphOutputName.Name != STATIC_FNAME("Constant 0") &&
				Element.DensityGraphOutputName.Name != STATIC_FNAME("Constant 1"))
			{
				CheckFloatOutputExists(Element.DensityGraphOutputName);
			}

			if (const int32* Seed = Settings.Seeds.Find(Element.Advanced.SeedName))
			{
				Element.FinalSeed = *Seed;
			}
			else
			{
				Element.FinalSeed = Element.Advanced.DefaultSeed;
			}

			return true;
		};

		for (auto& RayGroup : ThreadSafeConfig.RayGroups)
		{
			for (auto& RayElement : RayGroup.Spawners)
			{
				if (!SetupElement(RayElement))
				{
					ResetManager();
					return Manager;
				}
			}
		}
		for (auto& HeightGroup : ThreadSafeConfig.HeightGroups)
		{
			CheckFloatOutputExists(HeightGroup.HeightGraphOutputName);
			for (auto& HeightElement : HeightGroup.Spawners)
			{
				if (!SetupElement(HeightElement))
				{
					ResetManager();
					return Manager;
				}
			}
		}
	}

	// Gather all the spawners and the ones they reference
	{
		TArray<UVoxelSpawner*> QueuedSpawners = Spawners.Array();
		TSet<UVoxelSpawner*> ProcessedSpawners;

		while (QueuedSpawners.Num() > 0)
		{
			auto* Spawner = QueuedSpawners.Pop();
			if (ProcessedSpawners.Contains(Spawner)) continue;
			ProcessedSpawners.Add(Spawner);

			TSet<UVoxelSpawner*> NewSpawners;
			if (!Spawner->GetSpawners(NewSpawners))
			{
				ResetManager();
				return Manager;
			}
			QueuedSpawners.Append(NewSpawners.Array());
		}

		Spawners = MoveTemp(ProcessedSpawners);
	}

	// Create the proxies
	for (auto* Spawner : Spawners)
	{
		check(Spawner);
		Manager->SpawnersMap.Add(Spawner, Spawner->GetSpawnerProxy(*Manager));
	}

	// Call post spawn
	for (auto& It : Manager->SpawnersMap)
	{
		It.Value->PostSpawn();
	}

	// Bind delegates
	for (int32 ElementIndex = 0; ElementIndex < ThreadSafeConfig.HeightGroups.Num(); ElementIndex++)
	{
		auto& HeightGroup = ThreadSafeConfig.HeightGroups[ElementIndex];
		Settings.ProcGenManager->BindGenerationEvent(
			true,
			HeightGroup.ChunkSize,
			HeightGroup.GenerationDistanceInChunks,
			FChunkDelegate::CreateThreadSafeSP(Manager, &FVoxelSpawnerManager::SpawnHeightGroup_GameThread, ElementIndex));
	}
	for (int32 ElementIndex = 0; ElementIndex < ThreadSafeConfig.RayGroups.Num(); ElementIndex++)
	{
		auto& RayGroup = ThreadSafeConfig.RayGroups[ElementIndex];
		Settings.ProcGenManager->BindGenerationEvent(
			true,
			RENDER_CHUNK_SIZE << RayGroup.LOD,
			RayGroup.GenerationDistanceInChunks,
			FChunkDelegate::CreateThreadSafeSP(Manager, &FVoxelSpawnerManager::SpawnRayGroup_GameThread, ElementIndex));
	}

	return Manager;
}

void FVoxelSpawnerManager::Destroy()
{
	CancelTasksCounter.Store(1);
	StopTicking();
}

FVoxelSpawnerManager::~FVoxelSpawnerManager()
{
	ensure(IsInGameThread());
}

FVoxelSpawnerManager::FVoxelSpawnerManager(const FVoxelSpawnerSettings& Settings)
	: Settings(Settings)
{
	CancelTasksCounter = 0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelSharedPtr<FVoxelSpawnerProxy> FVoxelSpawnerManager::GetSpawner(UVoxelSpawner* Spawner) const
{
	return SpawnersMap.FindRef(Spawner);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnerManager::Tick(float DeltaTime)
{
	VOXEL_FUNCTION_COUNTER();

	// TODO: time limit?
	FlushGameThreadQueue();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnerManager::SpawnHeightGroup_GameThread(FIntBox Bounds, int32 HeightGroupIndex)
{
	VOXEL_FUNCTION_COUNTER();

	Settings.Pool->QueueTask(EVoxelTaskType::FoliageBuild, new TVoxelFoliageBuildTask<true>(*this, Bounds, HeightGroupIndex));

	TaskCounter.Increment();
	UpdateTaskCount();
}

void FVoxelSpawnerManager::SpawnHeightGroup_AnyThread(const FIntBox& Bounds, const int32 HeightGroupIndex)
{
	VOXEL_FUNCTION_COUNTER();

	const FIntBox LockedBounds = Bounds.Extend(2); // For neighbors: +1; For max included vs excluded: +1
	FVoxelReadScopeLock Lock(*Settings.Data, LockedBounds, FUNCTION_FNAME);
	const FVoxelConstDataAccelerator Accelerator(*Settings.Data, LockedBounds);

	TMap<UVoxelSpawner*, TArray<FVoxelSpawnerHit>> HitsMap;
	FVoxelSpawnerUtilities::SpawnWithHeight(CancelTasksCounter, Accelerator, ThreadSafeConfig, HeightGroupIndex, Bounds, HitsMap);

	ProcessHits(Bounds, HitsMap, Accelerator);

	TaskCounter.Decrement();
	UpdateTaskCount();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnerManager::SpawnRayGroup_GameThread(FIntBox Bounds, int32 RayGroupIndex)
{
	VOXEL_FUNCTION_COUNTER();

	Settings.Pool->QueueTask(EVoxelTaskType::FoliageBuild, new TVoxelFoliageBuildTask<false>(*this, Bounds, RayGroupIndex));

	TaskCounter.Increment();
	UpdateTaskCount();
}

void FVoxelSpawnerManager::SpawnRayGroup_AnyThread(const FIntBox& Bounds, int32 RayGroupIndex)
{
	VOXEL_FUNCTION_COUNTER();

	const auto& RayGroup = ThreadSafeConfig.RayGroups[RayGroupIndex];

	check(Bounds.Size() == FIntVector(RENDER_CHUNK_SIZE << RayGroup.LOD));

	TArray<uint32> Indices;
	TArray<FVector> Vertices;
	Settings.Renderer->CreateGeometry_AnyThread(RayGroup.LOD, Bounds.Min, Indices, Vertices);

	const bool bShowDebugRays = CVarShowVoxelSpawnerRays.GetValueOnAnyThread() != 0;
	const bool bShowDebugHits = CVarShowVoxelSpawnerHits.GetValueOnAnyThread() != 0;

	TUniquePtr<FVoxelSpawnerRayHandler> RayHandler;

#if USE_EMBREE_VOXEL
	RayHandler = MakeUnique<FVoxelSpawnerEmbreeRayHandler>(bShowDebugRays || bShowDebugHits, MoveTemp(Indices), MoveTemp(Vertices));
#else
	UE_LOG(LogVoxel, Error, TEXT("Embree is required for voxel spawners!"));
	return;
#endif

	if (!ensure(!RayHandler->HasError())) return;

	const FIntBox LockedBounds = Bounds.Extend(2); // For neighbors: +1; For max included vs excluded: +1
	FVoxelReadScopeLock Lock(*Settings.Data, LockedBounds, FUNCTION_FNAME);
	const FVoxelConstDataAccelerator Accelerator(*Settings.Data, LockedBounds);

	TMap<UVoxelSpawner*, TArray<FVoxelSpawnerHit>> HitsMap;
	FVoxelSpawnerUtilities::SpawnWithRays(CancelTasksCounter, Accelerator, ThreadSafeConfig, RayGroupIndex, Bounds, *RayHandler, HitsMap);

	if (bShowDebugRays || bShowDebugHits)
	{
		RayHandler->ShowDebug(Settings.VoxelWorldInterface, Bounds.Min, bShowDebugRays, bShowDebugHits);
	}

	ProcessHits(Bounds, HitsMap, Accelerator);

	TaskCounter.Decrement();
	UpdateTaskCount();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelSpawnerManager::UpdateTaskCount() const
{
	Settings.DebugManager->ReportFoliageTaskCount(TaskCounter.GetValue());
}

void FVoxelSpawnerManager::ProcessHits(
	const FIntBox& Bounds,
	const TMap<UVoxelSpawner*, TArray<FVoxelSpawnerHit>>& HitsMap,
	const FVoxelConstDataAccelerator& Accelerator)
{
	VOXEL_FUNCTION_COUNTER();

	if (CancelTasksCounter.Load(EMemoryOrder::Relaxed)) return;

	if (CVarShowVoxelSpawnerPositions.GetValueOnAnyThread() != 0)
	{
		VOXEL_SCOPE_COUNTER("Debug Hits");
		AsyncTask(ENamedThreads::GameThread, [Hits = HitsMap, VoxelWorld = Settings.VoxelWorldInterface]()
		{
			if (VoxelWorld.IsValid())
			{
				if (UWorld* World = VoxelWorld->GetWorld())
				{
					for (auto& It : Hits)
					{
						auto& Color = GColorList.GetFColorByIndex(FMath::RandRange(0, GColorList.GetColorsNum() - 1));
						for (auto& Hit : It.Value)
						{
							auto Position = VoxelWorld->LocalToGlobalFloat(Hit.Position);
							DrawDebugPoint(World, Position, 5, Color, true, 1000.f);
							DrawDebugLine(World, Position, Position + 50 * Hit.Normal, Color, true, 1000.f);
						}
					}
				}
			}
		});
	}

	for (auto& It : HitsMap)
	{
		auto& Spawner = SpawnersMap[It.Key];
		auto& Hits = It.Value;
		if (Hits.Num() == 0) continue;

		TUniquePtr<FVoxelSpawnerProxyResult> UniqueResult = Spawner->ProcessHits(Bounds, Hits, Accelerator);
		if (!UniqueResult.IsValid()) continue;

		const auto SharedResult = TVoxelSharedRef<FVoxelSpawnerProxyResult>(UniqueResult.Release());

		{
			FScopeLock Lock(&ApplyAnyThreadQueueSection);
			ApplyAnyThreadQueue.Add(SharedResult);
		}
		ApplyGameThreadQueue.Enqueue(SharedResult);
	}
}

void FVoxelSpawnerManager::FlushAnyThreadQueue()
{
	TArray<TVoxelSharedPtr<FVoxelSpawnerProxyResult>> Results;
	{
		FScopeLock Lock(&ApplyAnyThreadQueueSection);
		Results = MoveTemp(ApplyAnyThreadQueue);
	}

	for (auto& Result : Results)
	{
		check(Result.IsValid());
		Result->Apply_AnyThread();
	}
}

void FVoxelSpawnerManager::FlushGameThreadQueue()
{
	check(IsInGameThread());
	TVoxelSharedPtr<FVoxelSpawnerProxyResult> Result;
	while (ApplyGameThreadQueue.Dequeue(Result))
	{
		check(Result.IsValid());
		Result->Apply_GameThread();
	}
}