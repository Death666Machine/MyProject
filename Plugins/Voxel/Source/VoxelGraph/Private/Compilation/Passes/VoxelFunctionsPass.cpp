// Copyright 2020 Phyronnaz

#include "Compilation/Passes/VoxelFunctionsPass.h"
#include "Compilation/VoxelGraphCompiler.h"
#include "Compilation/VoxelCompilationNode.h"
#include "Compilation/VoxelGraphCompilerHelpers.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelNode.h"

void FVoxelFindFunctionsPass::Apply(FVoxelGraphCompiler& Compiler, TArray<FVoxelCompilationFunctionDescriptor>& OutFunctions)
{
	check(Compiler.FirstNode);
	check(Compiler.FirstNode->IsA<FVoxelFunctionSeparatorCompilationNode>());

	for (auto& Node : Compiler.GetAllNodes())
	{
		if (auto* FunctionCall = CastVoxel<FVoxelFunctionSeparatorCompilationNode>(Node))
		{
			OutFunctions.Add(FVoxelCompilationFunctionDescriptor(FunctionCall->FunctionId, FunctionCall));
		}
	}

	for (auto& Function : OutFunctions)
	{
		Function.Nodes.Add(Function.FirstNode);

		auto& OutputPin = Function.FirstNode->GetOutputPin(0);
		if (OutputPin.NumLinkedTo() == 0)
		{
			continue;
		}
		check(OutputPin.NumLinkedTo() == 1);

		FVoxelGraphCompilerHelpers::GetFunctionNodes(&OutputPin.GetLinkedTo(0).Node, Function.Nodes);
	}

	TSet<FVoxelCompilationNode*> FunctionSeparators;
	for (auto& Function : OutFunctions)
	{
		FunctionSeparators.Add(Function.FirstNode);
	}

	for (auto& FunctionA : OutFunctions)
	{
		for (auto& FunctionB : OutFunctions)
		{
			if (FunctionA.FirstNode != FunctionB.FirstNode)
			{
				auto IntersectionNodes = FunctionA.Nodes.Intersect(FunctionB.Nodes).Difference(FunctionSeparators);
				if (IntersectionNodes.Num() > 0)
				{
					auto IntersectionNodesHead = FVoxelGraphCompilerHelpers::FilterHeads(IntersectionNodes);
					Compiler.ErrorReporter.AddError("Nodes outputs are used in different functions! Make sure to make all your data go through the function separator if you want to use it in another function");

					for (auto& Node : IntersectionNodesHead)
					{
						Compiler.ErrorReporter.AddMessageToNode(Node, "Node is used by FunctionA and FunctionB", EVoxelGraphNodeMessageType::FatalError);
					}

					auto NodesAToShow = FunctionA.Nodes.Difference(IntersectionNodes).Difference(FunctionSeparators);
					auto NodesBToShow = FunctionB.Nodes.Difference(IntersectionNodes).Difference(FunctionSeparators);
					bool bNodesAShown = false;
					bool bNodesBShown = false;
					for (auto& Node : NodesAToShow)
					{
						if (Node->IsLinkedToOne(IntersectionNodesHead))
						{
							bNodesAShown = true;
							Compiler.ErrorReporter.AddMessageToNode(Node, "FunctionA node", EVoxelGraphNodeMessageType::Info);
						}
					}
					for (auto& Node : NodesBToShow)
					{
						if (Node->IsLinkedToOne(IntersectionNodesHead))
						{
							bNodesBShown = true;
							Compiler.ErrorReporter.AddMessageToNode(Node, "FunctionB node", EVoxelGraphNodeMessageType::Info);
						}
					}
					if (!bNodesAShown)
					{
						for (auto& Node : NodesAToShow)
						{
							Compiler.ErrorReporter.AddMessageToNode(Node, "FunctionA node", EVoxelGraphNodeMessageType::Info);
						}
					}
					if (!bNodesBShown)
					{
						for (auto& Node : NodesBToShow)
						{
							Compiler.ErrorReporter.AddMessageToNode(Node, "FunctionB node", EVoxelGraphNodeMessageType::Info);
						}
					}
					auto* Separator = FVoxelGraphCompilerHelpers::IsDataNodeSuccessor(FunctionA.FirstNode, FunctionB.FirstNode) ? FunctionB.FirstNode : FunctionA.FirstNode;
					Compiler.ErrorReporter.AddMessageToNode(Separator, "separator", EVoxelGraphNodeMessageType::Info);
					return;
				}
			}
		}
	}
}

void FVoxelRemoveNodesOutsideFunction::Apply(FVoxelGraphCompiler& Compiler, TSet<FVoxelCompilationNode*>& FunctionNodes)
{
	for (auto& Node : Compiler.GetAllNodesCopy())
	{
		if (!FunctionNodes.Contains(Node))
		{
			Node->BreakAllLinks();
			Compiler.RemoveNode(Node);
		}
	}
}

void FVoxelAddFirstFunctionPass::Apply(FVoxelGraphCompiler& Compiler)
{
	if (Compiler.FirstNode)
	{
		auto& InputPin = Compiler.FirstNode->GetInputPin(Compiler.FirstNodePinIndex);
		InputPin.BreakAllLinks();

		TSharedRef<FVoxelCompilationNode> FunctionNode = MakeShareable(
			new FVoxelFunctionSeparatorCompilationNode(
				*GetDefault<UVoxelNode>(),
				{ EVoxelPinCategory::Exec },
				{ EVoxelPinCategory::Exec }));

		Compiler.AddNode(FunctionNode);
		FunctionNode->GetOutputPin(0).LinkTo(InputPin);
		Compiler.FirstNode = &FunctionNode.Get();
	}
}

void FVoxelReplaceFunctionSeparatorsPass::Apply(FVoxelGraphCompiler& Compiler)
{
	check(Compiler.FirstNode);

	// Replace first node
	{
		auto* OldNode = Compiler.FirstNode;
		auto* NewNode = Compiler.AddNode(MakeShared<FVoxelFunctionInitCompilationNode>(CastCheckedVoxel<FVoxelFunctionSeparatorCompilationNode>(OldNode)));
		Compiler.FirstNode = NewNode;

		FVoxelGraphCompilerHelpers::MoveOutputPins(*OldNode, *NewNode);
		FVoxelGraphCompilerHelpers::BreakNodeLinks<EVoxelPinIter::Input>(*OldNode);
		OldNode->CheckIsNotLinked(Compiler.ErrorReporter);
		Compiler.RemoveNode(OldNode);
	}

	// Replace function calls
	for (auto* Node : Compiler.GetAllNodesCopy())
	{
		if (auto* Separator = CastVoxel<FVoxelFunctionSeparatorCompilationNode>(Node))
		{
			auto* NewNode = Compiler.AddNode(MakeShared<FVoxelFunctionCallCompilationNode>(*Separator));
			FVoxelGraphCompilerHelpers::MoveInputPins(*Separator, *NewNode);
			FVoxelGraphCompilerHelpers::BreakNodeLinks<EVoxelPinIter::Output>(*Separator);
			Separator->CheckIsNotLinked(Compiler.ErrorReporter);
			Compiler.RemoveNode(Separator);
		}
	}
}