// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelGetMaterialCollectionIndexNode.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelNodeFunctions.h"
#include "VoxelWorldGeneratorInit.h"
#include "VoxelRender/VoxelMaterialCollection.h"
#include "VoxelGraphGenerator.h"

#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "UObject/Package.h"
#include "AssetData.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"

UVoxelNode_GetMaterialCollectionIndex::UVoxelNode_GetMaterialCollectionIndex()
{
	SetOutputs(EVoxelPinCategory::Int);
}

FText UVoxelNode_GetMaterialCollectionIndex::GetTitle() const
{
	return FText::Format(NSLOCTEXT("Voxel", "GetMaterialCollectionIndexTitle", "Get Material Collection Index: {0}"), Super::GetTitle());
}

UObject* UVoxelNode_GetMaterialCollectionIndex::GetAsset() const
{
	return MaterialObject;
}

UClass* UVoxelNode_GetMaterialCollectionIndex::GetAssetClass() const
{
	return UObject::StaticClass();
}

void UVoxelNode_GetMaterialCollectionIndex::SetAsset(UObject* Object)
{
	MaterialObject = Object;
}

bool UVoxelNode_GetMaterialCollectionIndex::ShouldFilterAsset(const FAssetData& Asset) const
{
	UClass* Class = Asset.GetClass();
	return
		!Class->IsChildOf(UMaterialFunction::StaticClass()) &&
		!Class->IsChildOf(UMaterialInstanceConstant::StaticClass());
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GetMaterialCollectionIndex::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_GetMaterialCollectionIndex& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Name(Node.MaterialObject ? Node.MaterialObject->GetFName() : "")
			, IndexVariable("int32", UniqueName.ToString() + "_Index")
			, ExposedVariable(MakeShared<FVoxelMaterialObjectVariable>(Node, Node.MaterialObject))
		{
		}

		virtual void Init(Seed Inputs[], const FVoxelWorldGeneratorInit& InitStruct) override
		{
			auto* Collection = Cast<UVoxelMaterialCollection>(InitStruct.MaterialCollection);
			if (Collection)
			{
				Index = Collection->GetMaterialIndex(Name);
				if (Index == -1)
				{
					TSharedRef<FTokenizedMessage> Message = FTokenizedMessage::Create(EMessageSeverity::Warning);
					Message->AddToken(FTextToken::Create(FText::Format(
						NSLOCTEXT("Voxel", "InvalidMaterialObject", "GetMaterialCollectionIndex: Material Object {0} not found in "),
						FText::FromName(Name))));
					Message->AddToken(FUObjectToken::Create(Collection));

					Message->AddToken(FTextToken::Create(NSLOCTEXT("Voxel", "", "Graph:")));

					for (auto& SourceNode : SourceNodes)
					{
						if (SourceNode.IsValid())
						{
							Message->AddToken(FUObjectToken::Create(SourceNode->Graph));
						}
					}
					if (Node.IsValid())
					{
						FVoxelGraphErrorReporter ErrorReporter(Node->Graph);
						ErrorReporter.AddMessageToNode(Node.Get(), "material index not found", EVoxelGraphNodeMessageType::Warning);
						ErrorReporter.Apply(false);
					}
					FMessageLog("PIE").AddMessage(Message);
				}
			}
		}
		virtual void InitCpp(const TArray<FString> & Inputs, FVoxelCppConstructor & Constructor) const override
		{
			Constructor.AddLinef(TEXT("if (auto* Collection = Cast<UVoxelMaterialCollection>(%s.MaterialCollection))"), *FVoxelCppIds::InitStruct);
			Constructor.StartBlock();
			Constructor.AddLinef(TEXT("%s = Collection->GetMaterialIndex(%s);"), *IndexVariable.Name, *ExposedVariable->Name);
			Constructor.EndBlock();
			Constructor.AddLine("else");
			Constructor.StartBlock();
			Constructor.AddLinef(TEXT("%s = -1;"), *IndexVariable.Name);
			Constructor.EndBlock();
		}

		virtual void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<int32>() = Index;
		}
		virtual void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<int32>() = Index;
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLinef(TEXT("%s = %s;"), *Outputs[0], *IndexVariable.Name);
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}

		virtual void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(ExposedVariable);
			Config.AddInclude("VoxelRender/VoxelMaterialCollection.h");
		}
		virtual void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override
		{
			PrivateVariables.Add(IndexVariable);
		}

	private:
		int32 Index = -1;
		const FName Name;
		const FVoxelVariable IndexVariable;
		const TSharedRef<FVoxelMaterialObjectVariable> ExposedVariable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}