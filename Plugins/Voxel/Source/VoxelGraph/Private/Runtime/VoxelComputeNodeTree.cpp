// Copyright 2020 Phyronnaz

#include "Runtime/VoxelComputeNodeTree.h"
#include "Runtime/VoxelComputeNode.h"
#include "Runtime/VoxelGraphFunction.h"
#include "Runtime/VoxelGraphVMUtils.h"
#include "Runtime/VoxelDefaultComputeNodes.h"
#include "Runtime/VoxelGraphPerfCounter.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelNodes/VoxelIfNode.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelNode.h"

void FVoxelComputeNodeTree::Init(const FVoxelWorldGeneratorInit& InitStruct, FVoxelGraphVMInitBuffers& Buffers) const
{
	for (auto& Node : SeedNodes)
	{
		Seed NodeInputBuffer[MAX_VOXELNODE_PINS];
		Seed NodeOutputBuffer[MAX_VOXELNODE_PINS];
		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		Node->Init(NodeInputBuffer, NodeOutputBuffer, InitStruct);
		Node->CopyOutputsToVariables(NodeOutputBuffer, Buffers.Variables);
	}

	for (auto& Node : DataNodes)
	{
		Seed NodeInputBuffer[MAX_VOXELNODE_PINS];
		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		Node->Init(NodeInputBuffer, InitStruct);
		Node->CacheFunctionPtr();
	}

	if (ExecNode)
	{
		// Used only for materials node to check that the right config is used
		ExecNode->Init(InitStruct);
	}

	for (auto& Child : Children)
	{
		Child.Init(InitStruct, Buffers);
	}
}

template<bool bEnableStats>
const FVoxelGraphFunction* FVoxelComputeNodeTree::Compute(const FVoxelContext& Context, FVoxelGraphVMComputeBuffers& Buffers) const
{
	for (int32 Index = 0; Index < DataNodes.Num(); Index++)
	{
		FVoxelNodeType NodeInputBuffer[MAX_VOXELNODE_PINS];
		FVoxelNodeType NodeOutputBuffer[MAX_VOXELNODE_PINS];

		auto* Node = DataNodes.GetData()[Index];
		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		if (bEnableStats)
		{
			FVoxelScopePerfCounter Counter(Node);
			(Node->*(Node->ComputeFunctionPtr))(NodeInputBuffer, NodeOutputBuffer, Context);
		}
		else
		{
			(Node->*(Node->ComputeFunctionPtr))(NodeInputBuffer, NodeOutputBuffer, Context);
		}
		Node->CopyOutputsToVariables(NodeOutputBuffer, Buffers.Variables);
	}

	if (!ExecNode)
	{
		return nullptr;
	}

	switch (ExecNode->ExecType)
	{
	case EVoxelComputeNodeExecType::FunctionInit:
	case EVoxelComputeNodeExecType::Passthrough:
	{
		if (Children.Num() > 0)
		{
			checkVoxelGraph(Children.Num() == 1);
			return Children.GetData()[0].Compute<bEnableStats>(Context, Buffers);
		}
		else
		{
			return nullptr;
		}
	}
	case EVoxelComputeNodeExecType::If:
	{
		checkVoxelGraph(Children.Num() == 2);
		const int32 InputId = ExecNode->GetInputId(0);
		const bool bCondition = InputId == -1 ? ExecNode->GetDefaultValue<FVoxelNodeType>(0).Get<bool>() : Buffers.Variables[InputId].Get<bool>();
		return Children.GetData()[bCondition ? 0 : 1].Compute<bEnableStats>(Context, Buffers);
	}
	case EVoxelComputeNodeExecType::Setter:
	{
		FVoxelNodeType NodeInputBuffer[MAX_VOXELNODE_PINS];
		ExecNode->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		static_cast<FVoxelSetterComputeNode*>(ExecNode)->ComputeSetterNode(NodeInputBuffer, Buffers.GraphOutputs);
		if (Children.Num() > 0)
		{
			checkVoxelGraph(Children.Num() == 1);
			return Children.GetData()[0].Compute<bEnableStats>(Context, Buffers);
		}
		else
		{
			return nullptr;
		}
	}
	case EVoxelComputeNodeExecType::FunctionCall:
	{
		checkVoxelGraph(Children.Num() == 0);
		ExecNode->CopyVariablesToInputs(Buffers.Variables, Buffers.FunctionInputsOutputs);
		return static_cast<FVoxelFunctionCallComputeNode*>(ExecNode)->GetFunction();
	}
	default:
	{
		checkVoxelSlow(false);
		return nullptr;
	}
	}
}

template const FVoxelGraphFunction* FVoxelComputeNodeTree::Compute<true>(const FVoxelContext&, FVoxelGraphVMComputeBuffers&) const;
template const FVoxelGraphFunction* FVoxelComputeNodeTree::Compute<false>(const FVoxelContext&, FVoxelGraphVMComputeBuffers&) const;

inline FString ToString(FVoxelNodeRangeType Type, EVoxelPinCategory Category)
{
	switch (Category)
	{
	case EVoxelPinCategory::Exec:
		return "";
	case EVoxelPinCategory::Boolean:
		return Type.Get<bool>().ToString();
	case EVoxelPinCategory::Int:
		return Type.Get<int32>().ToString();
	case EVoxelPinCategory::Float:
		return Type.Get<v_flt>().ToString();
	case EVoxelPinCategory::Material:
		return "Material";
	case EVoxelPinCategory::Color:
		return "Color";
	case EVoxelPinCategory::Seed:
		return "";
	case EVoxelPinCategory::Wildcard:
	default:
		return "";
	}
}

template<bool bEnableRangeAnalysisDebug>
const FVoxelGraphFunction* FVoxelComputeNodeTree::ComputeRange(const FVoxelContextRange& Context, FVoxelGraphVMComputeRangeBuffers& Buffers) const
{
	auto& RangeFailStatus = FVoxelRangeFailStatus::Get();
	for (auto& Node : DataNodes)
	{
		ensureVoxelSlow(!RangeFailStatus.HasFailed());

		FVoxelNodeRangeType NodeInputBuffer[MAX_VOXELNODE_PINS];
		FVoxelNodeRangeType NodeOutputBuffer[MAX_VOXELNODE_PINS];

		Node->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		Node->ComputeRange(NodeInputBuffer, NodeOutputBuffer, Context);
		Node->CopyOutputsToVariables(NodeOutputBuffer, Buffers.Variables);
		if (RangeFailStatus.HasFailed())
		{
			FVoxelGraphRangeFailuresReporter::Get().ReportNodes(Node->SourceNodes, RangeFailStatus.GetError());
			return nullptr;
		}
		if (RangeFailStatus.HasWarning())
		{
			FVoxelGraphRangeFailuresReporter::Get().ReportNodes(Node->SourceNodes, RangeFailStatus.GetError());
			RangeFailStatus.Reset();
		}
		if (bEnableRangeAnalysisDebug && Node->SourceNodes.Num() > 0)
		{
			FString Message;
			auto& SourceNode = Node->SourceNodes[0];
			if (SourceNode.IsValid())
			{
				for (int32 Index = 0; Index < Node->InputCount; Index++)
				{
					FName InputName = SourceNode->GetInputPinName(Index);
					if (!Message.IsEmpty())
					{
						Message += "\n";
					}
					if (InputName != NAME_None)
					{
						Message += InputName.ToString() + ": ";
					}
					else
					{
						Message += "Input: ";
					}
					Message += ToString(NodeInputBuffer[Index], Node->GetInputCategory(Index));
				}
				for (int32 Index = 0; Index < Node->OutputCount; Index++)
				{
					FName OutputName = SourceNode->GetOutputPinName(Index);
					if (!Message.IsEmpty())
					{
						Message += "\n";
					}
					if (OutputName != NAME_None)
					{
						Message += OutputName.ToString() + ": ";
					}
					else
					{
						Message += "Output: ";
					}
					Message += ToString(NodeOutputBuffer[Index], Node->GetOutputCategory(Index));
				}

				FVoxelGraphErrorReporter::AddMessageToNodeInternal(SourceNode.Get(), Message, EVoxelGraphNodeMessageType::RangeAnalysisDebug);
			}
		}
	}

	if (!ExecNode)
	{
		return nullptr;
	}

	switch (ExecNode->ExecType)
	{
	case EVoxelComputeNodeExecType::FunctionInit:
	case EVoxelComputeNodeExecType::Passthrough:
	{
		if (Children.Num() > 0)
		{
			return Children[0].ComputeRange<bEnableRangeAnalysisDebug>(Context, Buffers);
		}
		else
		{
			return nullptr;
		}
	}
	case EVoxelComputeNodeExecType::If:
	{
		checkVoxelSlow(Children.Num() == 2);
		const int32 InputId = ExecNode->GetInputId(0);
		bool bCondition = InputId == -1 ? bool(ExecNode->GetDefaultValue<FVoxelNodeType>(0).Get<bool>()) : bool(Buffers.Variables[InputId].Get<bool>());
		if (RangeFailStatus.HasFailed()) // The bool is unknown
		{
			auto* IfNode = static_cast<FVoxelIfComputeNode*>(ExecNode);
			if (IfNode->BranchToUseForRangeAnalysis != EVoxelNodeIfBranchToUseForRangeAnalysis::None)
			{
				bCondition = IfNode->BranchToUseForRangeAnalysis == EVoxelNodeIfBranchToUseForRangeAnalysis::UseTrue;
				check(bCondition || IfNode->BranchToUseForRangeAnalysis == EVoxelNodeIfBranchToUseForRangeAnalysis::UseFalse);
				RangeFailStatus.Reset();
				RangeFailStatus.ResetNeedReport();
			}
			else
			{
				if (IfNode->bIgnoreRangeAnalysisErrors)
				{
					RangeFailStatus.Reset();
					RangeFailStatus.ResetNeedReport();
				}
				else
				{
					FVoxelGraphRangeFailuresReporter::Get().ReportNodes(ExecNode->SourceNodes, RangeFailStatus.GetError());
				}
				return nullptr;
			}
		}
		return Children[bCondition ? 0 : 1].ComputeRange<bEnableRangeAnalysisDebug>(Context, Buffers);
	}
	case EVoxelComputeNodeExecType::Setter:
	{
		FVoxelNodeRangeType NodeInputBuffer[MAX_VOXELNODE_PINS];
		ExecNode->CopyVariablesToInputs(Buffers.Variables, NodeInputBuffer);
		if (bEnableRangeAnalysisDebug && ExecNode->SourceNodes.Num() > 0)
		{
			FString Message;
			auto& SourceNode = ExecNode->SourceNodes[0];
			if (SourceNode.IsValid())
			{
				Message += "Input: ";
				Message += ToString(NodeInputBuffer[0], ExecNode->GetInputCategory(0));
				FVoxelGraphErrorReporter::AddMessageToNodeInternal(SourceNode.Get(), Message, EVoxelGraphNodeMessageType::RangeAnalysisDebug);
			}
		}
		static_cast<FVoxelSetterComputeNode*>(ExecNode)->ComputeRangeSetterNode(NodeInputBuffer, Buffers.GraphOutputs);
		if (Children.Num() > 0)
		{
			return Children[0].ComputeRange<bEnableRangeAnalysisDebug>(Context, Buffers);
		}
		else
		{
			return nullptr;
		}
	}
	case EVoxelComputeNodeExecType::FunctionCall:
	{
		checkVoxelSlow(Children.Num() == 0);
		ExecNode->CopyVariablesToInputs(Buffers.Variables, Buffers.FunctionInputsOutputs);
		return static_cast<FVoxelFunctionCallComputeNode*>(ExecNode)->GetFunction();
	}
	default:
	{
		check(false);
		return nullptr;
	}
	}
}

template const FVoxelGraphFunction* FVoxelComputeNodeTree::ComputeRange<true>(const FVoxelContextRange&, FVoxelGraphVMComputeRangeBuffers&) const;
template const FVoxelGraphFunction* FVoxelComputeNodeTree::ComputeRange<false>(const FVoxelContextRange&, FVoxelGraphVMComputeRangeBuffers&) const;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelComputeNodeTree::GetNodes(TSet<FVoxelComputeNode*>& Nodes) const
{
	for (auto& Node : DataNodes)
	{
		Nodes.Add(Node);
	}

	if (ExecNode)
	{
		Nodes.Add(ExecNode);
	}

	for (auto& Child : Children)
	{
		Child.GetNodes(Nodes);
	}
}

void FVoxelComputeNodeTree::InitCpp(FVoxelCppConstructor& Constructor) const
{
	for (auto& Node : SeedNodes)
	{
		Constructor.QueueComment("// Init of " + Node->PrettyName);
		Node->CallInitCpp(Constructor);
		Constructor.EndComment();
	}

	for (auto& Node : DataNodes)
	{
		Constructor.QueueComment("// Init of " + Node->PrettyName);
		Node->CallInitCpp(Constructor);
		Constructor.EndComment();
	}

	for (auto& Child : Children)
	{
		Child.InitCpp(Constructor);
	}
}

void FVoxelComputeNodeTree::ComputeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs)
{
	for (auto& Node : DataNodes)
	{
		Constructor.QueueComment(FString::Printf(TEXT("// %s"), *Node->PrettyName));
		Node->CallComputeCpp(Constructor, VariableInfo);
		Constructor.EndComment();
	}

	if (!ExecNode)
	{
		return;
	}

	switch (ExecNode->ExecType)
	{
	case EVoxelComputeNodeExecType::FunctionInit:
	case EVoxelComputeNodeExecType::Passthrough:
	{
		if (Children.Num() > 0)
		{
			Children[0].ComputeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		break;
	}
	case EVoxelComputeNodeExecType::If:
	{
		const int32 InputId = ExecNode->GetInputId(0);
		const FString Condition = InputId == -1 ? ExecNode->GetDefaultValueString(0) : Constructor.GetVariable(InputId, ExecNode);
		Constructor.AddLine("if (" + Condition + ")");
		Constructor.StartBlock();
		{
			FVoxelCppVariableScope Scope(Constructor);
			Children[0].ComputeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		Constructor.EndBlock();
		Constructor.AddLine("else");
		Constructor.StartBlock();
		{
			FVoxelCppVariableScope Scope(Constructor);
			Children[1].ComputeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		Constructor.EndBlock();
		break;
	}
	case EVoxelComputeNodeExecType::Setter:
	{
		static_cast<FVoxelSetterComputeNode*>(ExecNode)->CallComputeSetterNodeCpp(Constructor, VariableInfo, GraphOutputs);
		if (Children.Num() > 0) // Only a setter can have no children (cf FVoxelRemoveUnusedExecsPass)
		{
			Children[0].ComputeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		break;
	}
	case EVoxelComputeNodeExecType::FunctionCall:
	{
		check(Children.Num() == 0);
		auto* Function = static_cast<FVoxelFunctionCallComputeNode*>(ExecNode)->GetFunction();
		Function->Call(Constructor, ExecNode->GetInputsNamesCpp(Constructor), EVoxelFunctionType::Compute);
		break;
	}
	default:
	{
		check(false);
		break;
	}
	}
}

void FVoxelComputeNodeTree::ComputeRangeCpp(FVoxelCppConstructor& Constructor, const FVoxelVariableAccessInfo& VariableInfo, const TArray<FString>& GraphOutputs)
{
	for (auto& Node : DataNodes)
	{
		Constructor.QueueComment(FString::Printf(TEXT("// %s"), *Node->PrettyName));
		Node->CallComputeRangeCpp(Constructor, VariableInfo);
		Constructor.EndComment();
	}

	if (!ExecNode)
	{
		return;
	}

	switch (ExecNode->ExecType)
	{
	case EVoxelComputeNodeExecType::FunctionInit:
	case EVoxelComputeNodeExecType::Passthrough:
	{
		if (Children.Num() > 0)
		{
			Children[0].ComputeRangeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		break;
	}
	case EVoxelComputeNodeExecType::If:
	{
		const int32 InputId = ExecNode->GetInputId(0);
		const FString Condition = InputId == -1 ? ExecNode->GetDefaultValueString(0) : Constructor.GetVariable(InputId, ExecNode);
		auto* IfNode = static_cast<FVoxelIfComputeNode*>(ExecNode);
		if (IfNode->BranchToUseForRangeAnalysis != EVoxelNodeIfBranchToUseForRangeAnalysis::None)
		{
			const bool bCondition = IfNode->BranchToUseForRangeAnalysis == EVoxelNodeIfBranchToUseForRangeAnalysis::UseTrue;
			check(bCondition || IfNode->BranchToUseForRangeAnalysis == EVoxelNodeIfBranchToUseForRangeAnalysis::UseFalse);
			Constructor.AddLine("if (FVoxelBoolRange::If(" + Condition + ", " + FString(bCondition ? "true" : "false") + "))");
		}
		else
		{
			Constructor.AddLine("if (" + Condition + ")");
		}
		Constructor.StartBlock();
		{
			FVoxelCppVariableScope Scope(Constructor);
			Children[0].ComputeRangeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		Constructor.EndBlock();
		Constructor.AddLine("else");
		Constructor.StartBlock();
		{
			FVoxelCppVariableScope Scope(Constructor);
			Children[1].ComputeRangeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		Constructor.EndBlock();
		break;
	}
	case EVoxelComputeNodeExecType::Setter:
	{
		static_cast<FVoxelSetterComputeNode*>(ExecNode)->CallComputeRangeSetterNodeCpp(Constructor, VariableInfo, GraphOutputs);
		if (Children.Num() > 0) // Only a setter can have no children (cf FVoxelRemoveUnusedExecsPass)
		{
			Children[0].ComputeRangeCpp(Constructor, VariableInfo, GraphOutputs);
		}
		break;
	}
	case EVoxelComputeNodeExecType::FunctionCall:
	{
		check(Children.Num() == 0);
		auto* Function = static_cast<FVoxelFunctionCallComputeNode*>(ExecNode)->GetFunction();
		Function->Call(Constructor, ExecNode->GetInputsNamesCpp(Constructor), EVoxelFunctionType::Compute);
		break;
	}
	default:
	{
		check(false);
		break;
	}
	}
}