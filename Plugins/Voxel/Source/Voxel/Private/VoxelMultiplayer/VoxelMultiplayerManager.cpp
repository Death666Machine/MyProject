// Copyright 2020 Phyronnaz

#include "VoxelMultiplayer/VoxelMultiplayerManager.h"
#include "VoxelMultiplayer/VoxelMultiplayerInterface.h"
#include "VoxelData/VoxelData.h"
#include "VoxelData/VoxelSaveUtilities.h"
#include "VoxelRender/IVoxelLODManager.h"
#include "VoxelDebug/VoxelDebugManager.h"
#include "VoxelWorld.h"
#include "VoxelMessages.h"

// TODO https://github.com/Phyronnaz/VoxelPrivate/blob/82df5f5c96f124139a13cbbf88453841e2bee0fe/Source/Voxel/Public/VoxelMultiplayer/VoxelMultiplayerManager.h#L1
// TODO https://github.com/Phyronnaz/VoxelPrivate/blob/82df5f5c96f124139a13cbbf88453841e2bee0fe/Source/Voxel/Private/VoxelMultiplayer/VoxelMultiplayerManager.cpp#L1

FVoxelMultiplayerSettings::FVoxelMultiplayerSettings(
	const AVoxelWorld* InWorld,
	const TVoxelSharedRef<FVoxelData>& InData,
	const TVoxelSharedRef<FVoxelDebugManager>& InDebugManager,
	const TVoxelSharedRef<IVoxelLODManager>& InLODManager)
	: Data(InData)
	, DebugManager(InDebugManager)
	, LODManager(InLODManager)
	, VoxelWorld(InWorld)
	, MultiplayerSyncRate(FMath::Max(SMALL_NUMBER, InWorld->MultiplayerSyncRate))
{
}

TVoxelSharedRef<FVoxelMultiplayerManager> FVoxelMultiplayerManager::Create(const FVoxelMultiplayerSettings& Settings)
{
	TVoxelSharedPtr<IVoxelMultiplayerServer> Server;
	TVoxelSharedPtr<IVoxelMultiplayerClient> Client;

	auto* MultiplayerInterfaceInstance = Settings.VoxelWorld->GetMultiplayerInterfaceInstance();
	if (MultiplayerInterfaceInstance)
	{
		if (MultiplayerInterfaceInstance->IsServer())
		{
			Server = MultiplayerInterfaceInstance->CreateServer();
		}
		else
		{
			Client = MultiplayerInterfaceInstance->CreateClient();
		}
	}
	else
	{
		FVoxelMessages::Error(
			"bEnableMultiplayer = true, but the multiplayer instance is not created! "
			"You need to call CreateMultiplayerInterfaceInstance before creating the voxel world.",
			Settings.VoxelWorld.Get());
	}

	TVoxelSharedRef<FVoxelMultiplayerManager> MultiplayerManager = MakeShareable(new FVoxelMultiplayerManager(Settings, Server, Client));
	if (Server.IsValid())
	{
		Server->OnConnection.BindThreadSafeSP(MultiplayerManager, &FVoxelMultiplayerManager::OnConnection);
	}
	return MultiplayerManager;
}

void FVoxelMultiplayerManager::Destroy()
{
	StopTicking();
}

FVoxelMultiplayerManager::FVoxelMultiplayerManager(
	const FVoxelMultiplayerSettings& Settings,
	TVoxelSharedPtr<IVoxelMultiplayerServer> Server,
	TVoxelSharedPtr<IVoxelMultiplayerClient> Client)
	: Settings(Settings)
	, Server(Server)
	, Client(Client)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMultiplayerManager::Tick(float DeltaTime)
{
	const double Time = FPlatformTime::Seconds();
	if (Server.IsValid() && Time - LastSyncTime > 1. / Settings.MultiplayerSyncRate)
	{
		LastSyncTime = Time;
		SendData();
	}
	if (Client.IsValid())
	{
		ReceiveData();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelMultiplayerManager::ReceiveData() const
{
	VOXEL_FUNCTION_COUNTER();

	check(Client.IsValid());
	if (!Client->IsValid()) return;

	if (!ensure(Settings.VoxelWorld.IsValid())) return;

	const EVoxelMultiplayerNextLoadType NextLoadType = Client->GetNextLoadType();
	if (!Client->IsValid()) return;

	switch (NextLoadType)
	{
	case EVoxelMultiplayerNextLoadType::Save:
	{
		FVoxelCompressedWorldSave Save;
		if (Client->ReceiveSave(Save))
		{
			FVoxelUncompressedWorldSave DecompressedSave;
			UVoxelSaveUtilities::DecompressVoxelSave(Save, DecompressedSave);

			TArray<FIntBox> ModifiedBounds;
			Settings.Data->LoadFromSave(Settings.VoxelWorld.Get(), DecompressedSave, ModifiedBounds);

			Settings.LODManager->UpdateBounds(ModifiedBounds);
		}
		break;
	}
	case EVoxelMultiplayerNextLoadType::Diffs:
	{
		TArray<TVoxelChunkDiff<FVoxelValue>> ValueDiffs;
		TArray<TVoxelChunkDiff<FVoxelMaterial>> MaterialDiffs;
		if (Client->ReceiveDiffs(ValueDiffs, MaterialDiffs))
		{
			TArray<FIntBox> ModifiedBounds;
			// TODO Async?
			Settings.Data->LoadFromDiffs(ValueDiffs, MaterialDiffs, ModifiedBounds);

			Settings.LODManager->UpdateBounds(ModifiedBounds);
			Settings.DebugManager->ReportMultiplayerSyncedChunks([&]() { return ModifiedBounds; });
		}
		break;
	}
	case EVoxelMultiplayerNextLoadType::Unknown:
		break;
	default:
		check(false);
		break;
	}
}

void FVoxelMultiplayerManager::SendData() const
{
	VOXEL_FUNCTION_COUNTER();

	check(Server.IsValid());
	if (!Server->IsValid()) return;

	TArray<TVoxelChunkDiff<FVoxelValue>> ValueDiffs;
	TArray<TVoxelChunkDiff<FVoxelMaterial>> MaterialDiffs;
	Settings.Data->GetDiffs(ValueDiffs, MaterialDiffs);

	if (ValueDiffs.Num() > 0 || MaterialDiffs.Num() > 0)
	{
		Server->SendDiffs(ValueDiffs, MaterialDiffs);
	}
}

void FVoxelMultiplayerManager::OnConnection()
{
	VOXEL_FUNCTION_COUNTER();

	check(Server.IsValid());
	if (!Server->IsValid()) return;

	UE_LOG(LogVoxel, Log, TEXT("Sending world to clients"));

	FVoxelUncompressedWorldSave Save;
	Settings.Data->GetSave(Save);

	FVoxelCompressedWorldSave CompressedSave;
	UVoxelSaveUtilities::CompressVoxelSave(Save, CompressedSave);

	Server->SendSave(CompressedSave, false);

	OnClientConnection.Broadcast();
}