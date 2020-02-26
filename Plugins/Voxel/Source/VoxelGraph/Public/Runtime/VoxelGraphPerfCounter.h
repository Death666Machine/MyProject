// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Runtime/VoxelComputeNode.h"

struct VOXELGRAPH_API FVoxelGraphPerfCounter : public TThreadSingleton<FVoxelGraphPerfCounter>
{
	struct FNodeStats
	{
		double MeanTime = 0;
		bool bValid = false;
	};

	struct FNodePerfTree
	{
		using FPerfMap = TMap<TWeakObjectPtr<const UVoxelNode>, TVoxelSharedPtr<struct FNodePerfTree>>;

		FNodePerfTree() = default;

		uint64 NumCalls = 0;
		FNodeStats Stats;
		FPerfMap Map;

		inline FNodePerfTree* GetLeaf(const TArray<TWeakObjectPtr<const UVoxelNode>>& Nodes) { return GetLeaf(Nodes, Nodes.Num() - 1); }
		inline FNodePerfTree* GetLeaf(const TArray<TWeakObjectPtr<const UVoxelNode>>& Nodes, int32 Index)
		{
			if (0 <= Index)
			{
				auto& Node = Nodes[Index];
				auto& Child = Map.FindOrAdd(Node);
				if (!Child.IsValid())
				{
					Child = MakeVoxelShared<FNodePerfTree>();
				}
				return Child->GetLeaf(Nodes, Index - 1);
			}
			else
			{
				return this;
			}
		}
		inline void SetNodeStats(const FNodeStats& InStats)
		{
			check(!Stats.bValid);
			check(InStats.bValid);
			Stats = InStats;
		}
		inline void CopyTo(FNodePerfTree& Other)
		{
			Other.NumCalls += NumCalls;
			if (Stats.bValid)
			{
				Other.Stats = Stats;
			}
			for (auto& It : Map)
			{
				auto& OtherTree = Other.Map.FindOrAdd(It.Key);
				if (!OtherTree.IsValid())
				{
					OtherTree = MakeVoxelShared<FNodePerfTree>();
				}
				It.Value->CopyTo(*OtherTree);
			}
		}
		inline void Reset()
		{
			NumCalls = 0;
			Stats.MeanTime = 0;
			Stats.bValid = false;
			Map.Reset();
		}
	};

	~FVoxelGraphPerfCounter()
	{
		CopyLogToMain();
	}
	// Return: should SetNodeStats be called?
	inline bool LogNode(const FVoxelComputeNode* Node)
	{
		auto* Leaf = GetLeaf(Node);
		Leaf->NumCalls++;
		return !Leaf->Stats.bValid;
	}
	inline void SetNodeStats(const FVoxelComputeNode* Node, double MeanTime)
	{
		GetLeaf(Node)->SetNodeStats(FNodeStats{ MeanTime, true });
	}

	inline void CopyLogToMain()
	{
		FScopeLock Lock(&Section);

		Tree.CopyTo(SingletonTree);
		Tree.Reset();
		FastAccess.Empty();
	}

public:
	inline static void Reset()
	{
		SingletonTree.Reset();
	}

	inline static const FNodePerfTree& GetSingletonTree()
	{
		return SingletonTree;
	}

private:
	FNodePerfTree Tree;
	TMap<FName, FNodePerfTree*> FastAccess;

	inline FNodePerfTree* GetLeaf(const FVoxelComputeNode* Node)
	{
		auto*& Leaf = FastAccess.FindOrAdd(Node->UniqueName);
		if (UNLIKELY(!Leaf))
		{
			Leaf = Tree.GetLeaf(Node->SourceNodes);
		}
		return Leaf;
	}

	static FCriticalSection Section;
	static FNodePerfTree SingletonTree;
};

struct FVoxelScopePerfCounter
{
	FVoxelScopePerfCounter(const FVoxelDataComputeNode* Node)
		: Node(Node)
	{
	}
	~FVoxelScopePerfCounter()
	{
		if (FVoxelGraphPerfCounter::Get().LogNode(Node))
		{
			Node->ComputeStats();
		}
	}

private:
	const FVoxelDataComputeNode* Node;
};

///////////////////////////////////////////////////////////////////////////////

struct VOXELGRAPH_API FVoxelGraphRangeFailuresReporter : public TThreadSingleton<FVoxelGraphRangeFailuresReporter>
{
	using FNodeErrorMap = TMap<TWeakObjectPtr<const UVoxelNode>, TSet<FString>>;

	~FVoxelGraphRangeFailuresReporter()
	{
		CopyLogToMain();
	}

	inline void ReportNodes(const TArray<TWeakObjectPtr<const UVoxelNode>>& InNodes, const FString& Error)
	{
		for (auto& Node : InNodes)
		{
			NodesMap.FindOrAdd(Node).Add(Error);
		}
	}

	inline void CopyLogToMain()
	{
		FScopeLock Lock(&Section);

		SingletonNodes.Append(NodesMap);
		NodesMap.Reset();
	}

public:
	inline static void Reset()
	{
		SingletonNodes.Empty();
	}

	inline static const auto& GetSingletonMap()
	{
		return SingletonNodes;
	}

private:
	FNodeErrorMap NodesMap;

	static FCriticalSection Section;
	static FNodeErrorMap SingletonNodes;
};
