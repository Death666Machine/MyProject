// Copyright 2020 Phyronnaz

#pragma once

#include <cmath>
#include <queue>
#include <limits>

#include "CoreMinimal.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "PrimitiveSceneProxy.h"
#include "StaticMeshResources.h"
#include "DistanceFieldAtlas.h"

#include "ShaderCore.h"
#include "GlobalShader.h"
#include "UniformBuffer.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "ShaderParameterUtils.h"
#include "Tickable.h"

#include "Async/Async.h"
#include "Containers/StaticArray.h"
#include "Curves/RichCurve.h"
#include "Templates/SubclassOf.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Net/DataBunch.h"
#include "Stats/StatsMisc.h"

#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"

#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"

#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BodySetupEnums.h"

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetRenderingLibrary.h"

#include "Runtime/Launch/Resources/Version.h"
#include "Runtime/Engine/Private/InstancedStaticMesh.h"

#include "Components/MeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ActorComponent.h"

#include "Engine/Engine.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Engine/LatentActionManager.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Engine/Channel.h"
#include "Engine/NetworkDelegates.h"
#include "Engine/World.h"
#include "Engine/DeveloperSettings.h"
#include "Engine/NetworkDelegates.h"
#include "Engine/NetDriver.h"
#include "Engine/DemoNetDriver.h"

#include "HAL/Event.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformFilemanager.h"
#include "HAL/PlatformAffinity.h"
#include "HAL/IConsoleManager.h"
#include "HAL/ThreadSafeBool.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

#include "UObject/WeakObjectPtr.h"
#include "UObject/GCObject.h"
#include "UObject/TextProperty.h"
#include "UObject/CoreOnline.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"

#include "Misc/Guid.h"
#include "Misc/Compression.h"
#include "Misc/MessageDialog.h"
#include "Misc/UObjectToken.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeTryLock.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/ConfigCacheIni.h"

#include "VoxelGlobals.h"
#include "VoxelValue.h"
#include "VoxelMaterial.h"
#include "VoxelFoliage.h"
#include "IntBox.h"
#include "SafeSparseArray.h"
#include "QueueWithNum.h"
#include "VoxelPlaceableItems/VoxelPlaceableItem.h"
#include "VoxelConfigEnums.h"
#include "VoxelMathUtilities.h"
#include "VoxelIntVectorUtilities.h"
#include "VoxelQueryZone.h"
#include "VoxelData/VoxelSave.h"
#include "VoxelData/VoxelDataCell.h"
#include "VoxelData/VoxelData.h"
#include "VoxelWorldGeneratorPicker.h"
#include "VoxelMessages.h"
#include "VoxelOctree.h"
#include "VoxelTexture.h"
#include "VoxelRender/VoxelRawStaticIndexBuffer.h"

#include "RayTracingDefinitions.h"
#include "RayTracingInstance.h"