// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelCurveNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelNodeFunctions.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"

UVoxelNode_Curve::UVoxelNode_Curve()
{
	SetInputs(EC::Float);
	SetOutputs(EC::Float);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_Curve::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_Curve& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Curve(Node.Curve)
			, Variable(MakeShared<FVoxelCurveVariable>(Node, Node.Curve))
		{
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve, Inputs[0].Get<v_flt>());
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve, Inputs[0].Get<v_flt>());
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = FVoxelNodeFunctions::GetCurveValue(" + Variable->Name + ", " + Inputs[0] + ");");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(Variable);
			Config.AddInclude("Curves/CurveFloat.h");
		}

	private:
		const FVoxelRichCurve Curve;
		const TSharedRef<FVoxelCurveVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

FText UVoxelNode_Curve::GetTitle() const
{
	return FText::Format(NSLOCTEXT("Voxel", "Curve", "Float Curve: {0}"), Super::GetTitle());
}

void UVoxelNode_Curve::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);
	if (!Curve)
	{
		ErrorReporter.AddMessageToNode(this, "invalid curve", EVoxelGraphNodeMessageType::FatalError);
	}
}

#if WITH_EDITOR
bool UVoxelNode_Curve::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	return TryImportObject(Property, Object, Curve);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_CurveColor::UVoxelNode_CurveColor()
{
	SetInputs(EC::Float);
	SetOutputs(
		{ "R", EC::Float, "Red between 0 and 1" },
		{ "G", EC::Float, "Green between 0 and 1" },
		{ "B", EC::Float, "Blue between 0 and 1" },
		{ "A", EC::Float, "Alpha between 0 and 1" });
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_CurveColor::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_CurveColor& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Curve(Node.Curve)
			, Variable(MakeShared<FVoxelColorCurveVariable>(Node, Node.Curve))
		{
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[0], Inputs[0].Get<v_flt>());
			Outputs[1].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[1], Inputs[0].Get<v_flt>());
			Outputs[2].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[2], Inputs[0].Get<v_flt>());
			Outputs[3].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[3], Inputs[0].Get<v_flt>());
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[0], Inputs[0].Get<v_flt>());
			Outputs[1].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[1], Inputs[0].Get<v_flt>());
			Outputs[2].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[2], Inputs[0].Get<v_flt>());
			Outputs[3].Get<v_flt>() = FVoxelNodeFunctions::GetCurveValue(Curve.Curves[3], Inputs[0].Get<v_flt>());
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = FVoxelNodeFunctions::GetCurveValue(" + Variable->Name + ".Curves[0], " + Inputs[0] + ");");
			Constructor.AddLine(Outputs[1] + " = FVoxelNodeFunctions::GetCurveValue(" + Variable->Name + ".Curves[1], " + Inputs[0] + ");");
			Constructor.AddLine(Outputs[2] + " = FVoxelNodeFunctions::GetCurveValue(" + Variable->Name + ".Curves[2], " + Inputs[0] + ");");
			Constructor.AddLine(Outputs[3] + " = FVoxelNodeFunctions::GetCurveValue(" + Variable->Name + ".Curves[3], " + Inputs[0] + ");");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			Config.AddExposedVariable(Variable);
			Config.AddInclude("Curves/CurveLinearColor.h");
		}

	private:
		const FVoxelColorRichCurve Curve;
		const TSharedRef<FVoxelColorCurveVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

FText UVoxelNode_CurveColor::GetTitle() const
{
	return FText::Format(NSLOCTEXT("Voxel", "ColorCurve", "Color Curve: {0}"), Super::GetTitle());
}

void UVoxelNode_CurveColor::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);
	if (!Curve)
	{
		ErrorReporter.AddMessageToNode(this, "invalid color curve", EVoxelGraphNodeMessageType::FatalError);
	}
}

#if WITH_EDITOR
bool UVoxelNode_CurveColor::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	return TryImportObject(Property, Object, Curve);
}
#endif