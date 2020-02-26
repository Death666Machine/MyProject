// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelParameterNodes.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "VoxelNodes/VoxelNodeColors.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelGraphGenerator.h"

UVoxelNode_FloatParameter::UVoxelNode_FloatParameter()
{
	SetOutputs(EC::Float);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_FloatParameter::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_FloatParameter& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Value(Node.bExposeToBP ? GetGraph()->GetFloatParameter(Node.UniqueName, Node.Value) : Node.Value)
			, bExposeToBP(Node.bExposeToBP)
			, Variable(MakeShared<FVoxelExposedVariable>(Node, "float", "float", LexToString(Node.Value)))
		{
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = Value;
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = Value;
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + (bExposeToBP ? Variable->Name : Variable->DefaultValue) + ";");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			if (bExposeToBP)
			{
				Config.AddExposedVariable(Variable);
			}
		}

	private:
		const float Value;
		const bool bExposeToBP;
		const TSharedRef<FVoxelExposedVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

FString UVoxelNode_FloatParameter::GetValueString() const
{
	return FString::SanitizeFloat(Value);
}

FLinearColor UVoxelNode_FloatParameter::GetNotExposedColor() const
{
	return FVoxelNodeColors::FloatNode;
}

#if WITH_EDITOR
bool UVoxelNode_FloatParameter::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	if (auto* Prop = Cast<UFloatProperty>(Property))
	{
		Value = *Prop->ContainerPtrToValuePtr<float>(Object);
		return true;
	}
	return false;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_IntParameter::UVoxelNode_IntParameter()
{
	SetOutputs(EC::Int);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_IntParameter::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_IntParameter& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Value(Node.bExposeToBP ? GetGraph()->GetIntParameter(Node.UniqueName, Node.Value) : Node.Value)
			, bExposeToBP(Node.bExposeToBP)
			, Variable(MakeShared<FVoxelExposedVariable>(Node, "int32", "int32", LexToString(Node.Value)))
		{
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<int32>() = Value;
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<int32>() = Value;
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + (bExposeToBP ? Variable->Name : Variable->DefaultValue) + ";");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			if (bExposeToBP)
			{
				Config.AddExposedVariable(Variable);
			}
		}

	private:
		const int32 Value;
		const bool bExposeToBP;
		const TSharedRef<FVoxelExposedVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

FString UVoxelNode_IntParameter::GetValueString() const
{
	return FString::FromInt(Value);
}

FLinearColor UVoxelNode_IntParameter::GetNotExposedColor() const
{
	return FVoxelNodeColors::IntNode;
}

#if WITH_EDITOR
bool UVoxelNode_IntParameter::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	if (auto* Prop = Cast<UIntProperty>(Property))
	{
		Value = *Prop->ContainerPtrToValuePtr<int32>(Object);
		return true;
	}
	return false;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_ColorParameter::UVoxelNode_ColorParameter()
{
	SetOutputs(EC::Color);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_ColorParameter::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_ColorParameter& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Color((Node.bExposeToBP ? GetGraph()->GetColorParameter(Node.UniqueName, Node.Color) : Node.Color).ToFColor(false))
			, bExposeToBP(Node.bExposeToBP)
			, Variable(MakeShared<FVoxelExposedVariable>(Node, "FColor", "FColor", FString::Printf(TEXT("FColor(%d, %d, %d, %d)"), Color.R, Color.G, Color.B, Color.A)))
		{
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<FColor>() = Color;
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<FColor>() = Color;
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + (bExposeToBP ? Variable->Name : Variable->DefaultValue) + ";");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			if (bExposeToBP)
			{
				Config.AddExposedVariable(Variable);
			}
		}

	private:
		const FColor Color;
		const bool bExposeToBP;
		const TSharedRef<FVoxelExposedVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

FString UVoxelNode_ColorParameter::GetValueString() const
{
	return FString::Printf(TEXT("%.2g,%.2g,%.2g,%.2g"), Color.R, Color.G, Color.B, Color.A);
}

FLinearColor UVoxelNode_ColorParameter::GetNotExposedColor() const
{
	return FVoxelNodeColors::ColorNode;
}

#if WITH_EDITOR
bool UVoxelNode_ColorParameter::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	if (auto* Prop = Cast<UStructProperty>(Property))
	{
		if (Prop->GetCPPType(nullptr, 0) == "FLinearColor")
		{
			Color = *Prop->ContainerPtrToValuePtr<FLinearColor>(Object);
			return true;
		}
	}
	return false;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_BoolParameter::UVoxelNode_BoolParameter()
{
	SetOutputs(EC::Boolean);
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_BoolParameter::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_BoolParameter& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Value(Node.bExposeToBP ? GetGraph()->GetBoolParameter(Node.UniqueName, Node.Value) : Node.Value)
			, bExposeToBP(Node.bExposeToBP)
			, Variable(MakeShared<FVoxelExposedVariable>(Node, "bool", "bool", LexToString(Node.Value)))
		{
		}

		void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<bool>() = Value;
		}
		void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<bool>() = Value;
		}
		void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.AddLine(Outputs[0] + " = " + (bExposeToBP ? Variable->Name : Variable->DefaultValue) + ";");
		}
		void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}
		void SetupCpp(FVoxelCppConfig& Config) const override
		{
			if (bExposeToBP)
			{
				Config.AddExposedVariable(Variable);
			}
		}

	private:
		const bool Value;
		const bool bExposeToBP;
		const TSharedRef<FVoxelExposedVariable> Variable;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

FString UVoxelNode_BoolParameter::GetValueString() const
{
	return LexToString(Value);
}

FLinearColor UVoxelNode_BoolParameter::GetNotExposedColor() const
{
	return FVoxelNodeColors::BoolNode;
}

#if WITH_EDITOR
bool UVoxelNode_BoolParameter::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	if (auto* Prop = Cast<UBoolProperty>(Property))
	{
		auto* Data = Prop->ContainerPtrToValuePtr<bool>(Object);
		Value = Prop->GetPropertyValue(Data);
		return true;
	}
	return false;
}
#endif