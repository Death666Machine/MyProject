// Copyright 2020 Phyronnaz

#include "VoxelGraphErrorReporter.h"
#include "VoxelNode.h"
#include "VoxelGraphGenerator.h"
#include "IVoxelGraphEditor.h"
#include "Compilation/VoxelCompilationNode.h"
#include "Runtime/VoxelGraphPerfCounter.h"
#include "VoxelNodes/VoxelGraphMacro.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Modules/ModuleManager.h"

FVoxelGraphErrorReporter::FVoxelGraphErrorReporter(const UVoxelGraphGenerator* VoxelGraphGenerator)
	: VoxelGraphGenerator(VoxelGraphGenerator)
	, Parent(nullptr)
	, ErrorPrefix("")
{
	ensure(VoxelGraphGenerator);
}

FVoxelGraphErrorReporter::FVoxelGraphErrorReporter(FVoxelGraphErrorReporter& Parent, const FString& ErrorPrefix)
	: VoxelGraphGenerator(Parent.VoxelGraphGenerator)
	, Parent(&Parent)
	, ErrorPrefix(Parent.ErrorPrefix + ErrorPrefix + ": ")
{
	ensure(VoxelGraphGenerator);
}

FVoxelGraphErrorReporter::~FVoxelGraphErrorReporter()
{
	if (Parent)
	{
		Parent->CopyFrom(*this);
	}
}

void FVoxelGraphErrorReporter::AddError(const FString& Error)
{
	if (!Error.IsEmpty())
	{
		const FString ErrorWithPrefix = AddPrefixToError(Error);
		Messages.Add(FVoxelGraphMessage{ nullptr, Error, EVoxelGraphNodeMessageType::FatalError });
		bHasFatalError = true;
	}
}

void FVoxelGraphErrorReporter::AddInternalError(const FString Error)
{
	ensureMsgf(false, TEXT("Internal voxel graph compiler error: %s"), *Error);

	const bool bOldHasErrors = bHasFatalError;
	AddError("Internal error: " + Error +
		"\nPlease create a bug report here: https://gitlab.com/Phyronnaz/VoxelPluginIssues/issues \n"
		"Don't forget to attach the generated header file");
	bHasFatalError = bOldHasErrors;
}

inline FString& GetErrorString(UVoxelGraphNodeInterface* Node, EVoxelGraphNodeMessageType Type)
{
	switch (Type)
	{
	case EVoxelGraphNodeMessageType::Info:
		return Node->InfoMsg;
	case EVoxelGraphNodeMessageType::Warning:
		return Node->WarningMsg;
	case EVoxelGraphNodeMessageType::Error:
	case EVoxelGraphNodeMessageType::FatalError:
		return Node->ErrorMsg;
	case EVoxelGraphNodeMessageType::Dependencies:
		return Node->DependenciesMsg;
	case EVoxelGraphNodeMessageType::Stats:
		return Node->StatsMsg;
	case EVoxelGraphNodeMessageType::RangeAnalysisWarning:
		return Node->RangeAnalysisWarningMsg;
	case EVoxelGraphNodeMessageType::RangeAnalysisError:
		return Node->RangeAnalysisErrorMsg;
	case EVoxelGraphNodeMessageType::RangeAnalysisDebug:
		return Node->RangeAnalysisDebugMsg;
	default:
		check(false);
		static FString Ref;
		return Ref;
	}
}

void FVoxelGraphErrorReporter::AddMessageToNode(
	const UVoxelNode* Node,
	const FString& Message,
	EVoxelGraphNodeMessageType Severity,
	bool bSelectNode)
{
	check(Node);
	const FString MessageWithPrefix = AddPrefixToError(Message);

	if (Severity == EVoxelGraphNodeMessageType::FatalError)
	{
		bHasFatalError = true;
	}

	switch (Severity)
	{
	case EVoxelGraphNodeMessageType::Info:
	case EVoxelGraphNodeMessageType::Warning:
	case EVoxelGraphNodeMessageType::Error:
	case EVoxelGraphNodeMessageType::FatalError:
	case EVoxelGraphNodeMessageType::RangeAnalysisWarning:
	case EVoxelGraphNodeMessageType::RangeAnalysisError:
	{
		FVoxelGraphMessage NewMessage;
		NewMessage.Node = Node;
		NewMessage.Message = MessageWithPrefix;
		NewMessage.Type = Severity;
		Messages.Add(NewMessage);
		break;
	}
	case EVoxelGraphNodeMessageType::Dependencies:
	case EVoxelGraphNodeMessageType::Stats:
	case EVoxelGraphNodeMessageType::RangeAnalysisDebug:
	default:
		break;
	}

	if (bSelectNode)
	{
		AddNodeToSelect(Node);
	}

#if WITH_EDITORONLY_DATA
	if (auto* GraphNode = Node->GraphNode)
	{
		AddMessageToNodeInternal(Node, MessageWithPrefix, Severity);
		GraphsToRefresh.Add(GraphNode->GetGraph());
	}
#endif
}
void FVoxelGraphErrorReporter::AddMessageToNode(
	const FVoxelCompilationNode* Node,
	const FString& Message,
	EVoxelGraphNodeMessageType Severity,
	bool bSelectNode)
{
	check(Node);

	for (auto* SourceNode : Node->SourceNodes)
	{
		AddMessageToNode(SourceNode, Message, Severity, bSelectNode);
	}
}

void FVoxelGraphErrorReporter::AddMessageToNode(
	const FVoxelComputeNode* Node,
	const FString& Message,
	EVoxelGraphNodeMessageType Severity,
	bool bSelectNode)
{
	check(Node);

	for (auto& SourceNode : Node->SourceNodes)
	{
		if (SourceNode.IsValid())
		{
			AddMessageToNode(SourceNode.Get(), Message, Severity, bSelectNode);
		}
	}
}

void FVoxelGraphErrorReporter::AddNodeToSelect(const UVoxelNode* Node)
{
#if WITH_EDITORONLY_DATA
	if (Node && Node->GraphNode)
	{
		NodesToSelect.Add(Node->GraphNode);
	}
#endif
}
void FVoxelGraphErrorReporter::AddNodeToSelect(const FVoxelCompilationNode* Node)
{
	if (Node->SourceNodes.Num() > 0)
	{
		AddNodeToSelect(Node->SourceNodes.Last());
	}
}

void FVoxelGraphErrorReporter::Apply(bool bSelectNodes)
{
#if WITH_EDITOR
	if (VoxelGraphGenerator && VoxelGraphGenerator->VoxelGraph)
	{
		GraphsToRefresh.Add(VoxelGraphGenerator->VoxelGraph);
		if (auto* VoxelGraphEditor = IVoxelGraphEditor::GetVoxelGraphEditor())
		{
			for (auto* GraphToRefresh : GraphsToRefresh)
			{
				VoxelGraphEditor->RefreshNodesMessages(GraphToRefresh);
			}
			if (NodesToSelect.Num() > 0 && bSelectNodes)
			{
				VoxelGraphEditor->SelectNodesAndZoomToFit(VoxelGraphGenerator->VoxelGraph, NodesToSelect.Array());
			}
			VoxelGraphEditor->AddMessages(VoxelGraphGenerator, Messages);
		}
	}
	else
#endif
	{
		for (auto& Message : Messages)
		{
			if (Message.Type == EVoxelGraphNodeMessageType::FatalError)
			{
				UE_LOG(LogVoxel, Warning, TEXT("%s failed to compile: %s"), VoxelGraphGenerator ? *VoxelGraphGenerator->GetName() : TEXT(""), *Message.Message);
			}
		}
	}
}

void FVoxelGraphErrorReporter::CopyFrom(FVoxelGraphErrorReporter& Other)
{
	check(VoxelGraphGenerator == Other.VoxelGraphGenerator);

	bHasFatalError |= Other.bHasFatalError;
	Messages.Append(Other.Messages);
	NodesToSelect.Append(Other.NodesToSelect);
	GraphsToRefresh.Append(Other.GraphsToRefresh);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelGraphErrorReporter::ClearMessages(const UVoxelGraphGenerator* Graph, bool bClearAll, EVoxelGraphNodeMessageType MessagesToClear)
{
#if WITH_EDITOR
	if (auto* VoxelGraphEditor = IVoxelGraphEditor::GetVoxelGraphEditor())
	{
		VoxelGraphEditor->ClearMessages(Graph, bClearAll, MessagesToClear);
	}
#endif
}

void FVoxelGraphErrorReporter::ClearNodesMessages(const UVoxelGraphGenerator* Graph, bool bRecursive, bool bClearAll, EVoxelGraphNodeMessageType MessagesToClear)
{
#if WITH_EDITOR
	if (!Graph->VoxelGraph)
	{
		return;
	}

	static TSet<const UVoxelGraphGenerator*> Stack;
	if (Stack.Contains(Graph))
	{
		return;
	}
	Stack.Add(Graph);

	TSet<UVoxelGraphMacro*> Macros;
	for (auto* Node : Graph->VoxelGraph->Nodes)
	{
		if (auto* Interface = Cast<UVoxelGraphNodeInterface>(Node))
		{
			for (auto Type :
				{ EVoxelGraphNodeMessageType::Info,
				  EVoxelGraphNodeMessageType::Warning,
				  EVoxelGraphNodeMessageType::Error,
				  EVoxelGraphNodeMessageType::FatalError,
				  EVoxelGraphNodeMessageType::Dependencies,
				  EVoxelGraphNodeMessageType::Stats,
				  EVoxelGraphNodeMessageType::RangeAnalysisWarning,
				  EVoxelGraphNodeMessageType::RangeAnalysisError,
				  EVoxelGraphNodeMessageType::RangeAnalysisDebug })

			{
				if (bClearAll || MessagesToClear == Type)
				{
					GetErrorString(Interface, Type).Empty();
				}
			}
			if (bRecursive)
			{
				if (auto* MacroNode = Cast<UVoxelGraphMacroNode>(Interface->GetVoxelNode()))
				{
					auto* Macro = MacroNode->Macro;
					if (!Macros.Contains(Macro) && Macro)
					{
						Macros.Add(Macro);
						ClearNodesMessages(Macro, bRecursive, bClearAll, MessagesToClear);
					}
				}
			}
		}
	}
	if (auto* VoxelGraphEditor = IVoxelGraphEditor::GetVoxelGraphEditor())
	{
		VoxelGraphEditor->RefreshNodesMessages(Graph->VoxelGraph);
	}

	Stack.Remove(Graph);
#endif
}

void FVoxelGraphErrorReporter::ClearCompilationMessages(const UVoxelGraphGenerator* Graph)
{
	for (auto Type :
		{ EVoxelGraphNodeMessageType::Info,
		  EVoxelGraphNodeMessageType::Warning,
		  EVoxelGraphNodeMessageType::Error,
		  EVoxelGraphNodeMessageType::FatalError,
		  EVoxelGraphNodeMessageType::Dependencies
		})
	{
		ClearMessages(Graph, false, Type);
		ClearNodesMessages(Graph, true, false, Type);
	}
}

inline void SetPerfCounters(const UVoxelNode* Node, uint64 NumCalls, double MeanTime, FVoxelGraphErrorReporter& ErrorReporter)
{
#if WITH_EDITOR
	ErrorReporter.AddMessageToNode(Node, "Calls: " + (NumCalls == 0 ? "-" : FString::FromInt(NumCalls)), EVoxelGraphNodeMessageType::Stats, false);
	ErrorReporter.AddMessageToNode(Node, FString::Printf(TEXT("Time per call: %.2fns"), FMath::RoundToInt(MeanTime * 100) / 100.), EVoxelGraphNodeMessageType::Stats, false);
#endif
}

inline double GetTotalTimeAndAddItToNode(const TWeakObjectPtr<const UVoxelNode>& Node, const FVoxelGraphPerfCounter::FNodePerfTree& Tree, FVoxelGraphErrorReporter& ErrorReporter)
{
	if (Tree.Map.Num() == 0)
	{
		const double Time = Tree.Stats.MeanTime * FPlatformTime::GetSecondsPerCycle64() * 1e9;
		if (Node.IsValid())
		{
			SetPerfCounters(Node.Get(), Tree.NumCalls, Time, ErrorReporter);
		}
		return Time;
	}
	else
	{
		double Time = 0;
		for (auto& It : Tree.Map)
		{
			Time += GetTotalTimeAndAddItToNode(It.Key, *It.Value, ErrorReporter);
		}
		if (Node.IsValid())
		{
			SetPerfCounters(Node.Get(), 0, Time, ErrorReporter);
		}
		return Time;
	}
}

void FVoxelGraphErrorReporter::AddPerfCounters(const UVoxelGraphGenerator* Graph)
{
#if WITH_EDITOR
	if (Graph->bEnableStats)
	{
		FVoxelGraphErrorReporter ErrorReporter(Graph);
		auto& Tree = FVoxelGraphPerfCounter::GetSingletonTree();
		GetTotalTimeAndAddItToNode(nullptr, Tree, ErrorReporter);
		ErrorReporter.Apply(false);
	}
#endif
}

#if WITH_EDITOR
inline void GetStatsImpl(const TSet<UObject*>& SelectedNodes, const FVoxelGraphPerfCounter::FNodePerfTree& Tree, double& OutTotalTimeInSeconds, uint64& OutTotalCalls)
{
	if (Tree.Map.Num() == 0)
	{
		OutTotalTimeInSeconds += Tree.Stats.MeanTime * Tree.NumCalls * FPlatformTime::GetSecondsPerCycle64();
		OutTotalCalls += Tree.NumCalls;
	}
	else
	{
		for (auto& It : Tree.Map)
		{
			if (It.Key.IsValid() && It.Key->GraphNode && SelectedNodes.Contains(It.Key->GraphNode))
			{
				GetStatsImpl(SelectedNodes, *It.Value, OutTotalTimeInSeconds, OutTotalCalls);
			}
		}
	}
}
#endif

void FVoxelGraphErrorReporter::GetStats(const TSet<UObject*>& SelectedNodes, double& OutTotalTimeInSeconds, uint64& OutTotalCalls)
{
#if WITH_EDITOR
	OutTotalTimeInSeconds = 0;
	OutTotalCalls = 0;
	auto& Tree = FVoxelGraphPerfCounter::GetSingletonTree();
	GetStatsImpl(SelectedNodes, Tree, OutTotalTimeInSeconds, OutTotalCalls);
#endif
}

void FVoxelGraphErrorReporter::AddRangeAnalysisErrors(const UVoxelGraphGenerator* Graph)
{
#if WITH_EDITOR
	auto* VoxelGraphEditor = IVoxelGraphEditor::GetVoxelGraphEditor();
	if (Graph->bEnableRangeAnalysis && VoxelGraphEditor)
	{
		FVoxelGraphErrorReporter ErrorReporter(Graph);
		for (auto& It : FVoxelGraphRangeFailuresReporter::GetSingletonMap())
		{
			const auto& Node = It.Key;
			if (Node.IsValid())
			{
				for (const FString& Message : It.Value)
				{
					if (Message.StartsWith("warning: "))
					{
						ErrorReporter.AddMessageToNode(Node.Get(), Message.RightChop(9), EVoxelGraphNodeMessageType::RangeAnalysisWarning);
					}
					else
					{
						ensure(Message.StartsWith("error: "));
						ErrorReporter.AddMessageToNode(Node.Get(), Message.RightChop(7), EVoxelGraphNodeMessageType::RangeAnalysisError);
					}
				}
			}
		}
		ErrorReporter.Apply(false);
	}
#endif
}

void FVoxelGraphErrorReporter::AddMessageToNodeInternal(
	const UVoxelNode* Node,
	const FString& Message,
	EVoxelGraphNodeMessageType Severity)
{
#if WITH_EDITOR
	if (auto* GraphNode = Node->GraphNode)
	{
		FString& Text = GetErrorString(GraphNode, Severity);
		if (!Text.IsEmpty())
		{
			Text += "\n";
		}
		Text += Message;
	}
#endif
}

FString FVoxelGraphErrorReporter::AddPrefixToError(const FString& Error) const
{
	if (!VoxelGraphGenerator || VoxelGraphGenerator->bDetailedErrors)
	{
		return ErrorPrefix + Error;
	}
	else
	{
		return Error;
	}
}

bool FEnsureVoxelGraphHelper::Check(FVoxelGraphErrorReporter& ErrorReporter, const FString& Expr, const FString& File, int32 Line, const FVoxelCompilationNode* Node)
{
	const FString Message = "Internal error: " + Expr + " (" + File + ":" + FString::FromInt(Line) + ")";
	ErrorReporter.AddError(Message);
	if (Node)
	{
		ErrorReporter.AddMessageToNode(Node, Message, EVoxelGraphNodeMessageType::FatalError);
	}
	return true;
}