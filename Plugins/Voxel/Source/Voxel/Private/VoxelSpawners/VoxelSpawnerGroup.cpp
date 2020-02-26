// Copyright 2020 Phyronnaz

#include "VoxelSpawners/VoxelSpawnerGroup.h"
#include "VoxelSpawners/VoxelSpawnerManager.h"
#include "VoxelMessages.h"

#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

FVoxelSpawnerGroupProxyResult::FVoxelSpawnerGroupProxyResult(const FVoxelSpawnerProxy& Proxy, TArray<TUniquePtr<FVoxelSpawnerProxyResult>>&& Results)
	: FVoxelSpawnerProxyResult(Proxy)
	, Results(MoveTemp(Results))
{
}

void FVoxelSpawnerGroupProxyResult::Apply_AnyThread()
{
	for (auto& Result : Results)
	{
		check(Result.IsValid());
		Result->Apply_AnyThread();
	}
}

void FVoxelSpawnerGroupProxyResult::Apply_GameThread()
{
	for (auto& Result : Results)
	{
		check(Result.IsValid());
		Result->Apply_GameThread();
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelSpawnerGroupProxy::FVoxelSpawnerGroupProxy(UVoxelSpawnerGroup* Spawner, FVoxelSpawnerManager& Manager)
	: FVoxelSpawnerProxy(Spawner, Manager)
	, SpawnerGroup(Spawner)
{
}

TUniquePtr<FVoxelSpawnerProxyResult> FVoxelSpawnerGroupProxy::ProcessHits(
	const FIntBox& Bounds,
	const TArray<FVoxelSpawnerHit>& Hits,
	const FVoxelConstDataAccelerator& Accelerator) const
{
	TArray<TUniquePtr<FVoxelSpawnerProxyResult>> Results;
	if (Children.Num() > 0)
	{
		TArray<TArray<FVoxelSpawnerHit>> ChildrenHits;
		ChildrenHits.SetNum(Children.Num());
		for (auto& ChildHits : ChildrenHits)
		{
			ChildHits.Reserve(Hits.Num());
		}

		const uint32 Seed = Bounds.GetMurmurHash() ^ SpawnerSeed;
		const FRandomStream Stream(Seed);

		for (auto& Hit : Hits)
		{
			ChildrenHits[GetChild(Stream.GetFraction())].Add(Hit);
		}

		for (int32 Index = 0; Index < Children.Num(); Index++)
		{
			if (ChildrenHits[Index].Num() == 0) continue;
			auto Result = Children[Index].Spawner->ProcessHits(Bounds, ChildrenHits[Index], Accelerator);
			if (Result.IsValid())
			{
				Results.Emplace(MoveTemp(Result));
			}
		}
	}
	if (Results.Num() == 0)
	{
		return nullptr;
	}
	else
	{
		return MakeUnique<FVoxelSpawnerGroupProxyResult>(*this, MoveTemp(Results));
	}
}

void FVoxelSpawnerGroupProxy::PostSpawn()
{
	check(IsInGameThread());

	double ChildrenSum = 0;
	for (auto& Child : SpawnerGroup->Children)
	{
		ChildrenSum += Child.Probability;
	}
	if (ChildrenSum == 0)
	{
		ChildrenSum = 1;
	}

	double ProbabilitySum = 0;
	for (auto& Child : SpawnerGroup->Children)
	{
		const auto ChildSpawner = Manager.GetSpawner(Child.Spawner);
		if (!ChildSpawner.IsValid())
		{
			Children.Reset();
			return;
		}
		ProbabilitySum += Child.Probability / ChildrenSum;
		Children.Emplace(FChild{ ChildSpawner, float(ProbabilitySum) });
	}

	ensure(ChildrenSum == 1 || FMath::IsNearlyEqual(ProbabilitySum, 1));
}

int32 FVoxelSpawnerGroupProxy::GetChild(float RandomNumber) const
{
	for (int32 Index = 0; Index < Children.Num(); Index++)
	{
		if (RandomNumber < Children[Index].ProbabilitySum)
		{
			return Index;
		}
	}
	return Children.Num() - 1;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TVoxelSharedRef<FVoxelSpawnerProxy> UVoxelSpawnerGroup::GetSpawnerProxy(FVoxelSpawnerManager& Manager)
{
	return MakeVoxelShared<FVoxelSpawnerGroupProxy>(this, Manager);
}

bool UVoxelSpawnerGroup::GetSpawners(TSet<UVoxelSpawner*>& OutSpawners)
{
	static TArray<UVoxelSpawnerGroup*> Stack;

	struct FScopeStack
	{
		FScopeStack(UVoxelSpawnerGroup* This) : This(This)
		{
			Stack.Add(This);
		}
		~FScopeStack()
		{
			ensure(Stack.Pop() == This);
		}
		UVoxelSpawnerGroup* const This;
	};

	if (Stack.Contains(this))
	{
		TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Error);
		Message->AddToken(FTextToken::Create(NSLOCTEXT("Voxel", "RecursiveSpawnersGroup", "Recursive spawner group! Spawners in stack: ")));
		for (auto* Spawner : Stack)
		{
			Message->AddToken(FUObjectToken::Create(Spawner));
		}
		FMessageLog("PIE").AddMessage(Message);
		return false;
	}

	FScopeStack ScopeStack(this);

	OutSpawners.Add(this);
	for (auto& Child : Children)
	{
		if (!Child.Spawner)
		{
			FVoxelMessages::Error("Invalid Child Spawner!", this);
			return false;
		}
		if (!Child.Spawner->GetSpawners(OutSpawners))
		{
			return false;
		}
	}

	return true;
}

#if WITH_EDITOR
void UVoxelSpawnerGroup::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	if (bNormalizeProbabilitiesOnEdit &&
		PropertyChangedEvent.Property &&
		(PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive ||
			PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet))
	{
		const int32 EditedIndex = PropertyChangedEvent.GetArrayIndex(GET_MEMBER_NAME_STRING_CHECKED(UVoxelSpawnerGroup, Children));
		if (Children.IsValidIndex(EditedIndex))
		{
			double Sum = 0;
			for (int32 Index = 0; Index < Children.Num(); Index++)
			{
				if (Index != EditedIndex)
				{
					Sum += Children[Index].Probability;
				}
			}
			if (Sum == 0)
			{
				for (int32 Index = 0; Index < Children.Num(); Index++)
				{
					if (Index != EditedIndex)
					{
						ensure(Children[Index].Probability == 0);
						Children[Index].Probability = (1 - Children[EditedIndex].Probability) / (Children.Num() - 1);
					}
				}
			}
			else
			{
				for (int32 Index = 0; Index < Children.Num(); Index++)
				{
					if (Index != EditedIndex)
					{
						Children[Index].Probability *= (1 - Children[EditedIndex].Probability) / Sum;
					}
				}
			}
		}
	}
}
#endif