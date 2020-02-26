// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelWorldGeneratorSamplerNodes.h"
#include "VoxelNodes/VoxelNodeVariables.h"
#include "VoxelNodes/VoxelNodeHelpers.h"
#include "Runtime/VoxelComputeNode.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelCppUtils.h"
#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "VoxelWorldGenerators/VoxelFlatWorldGenerator.h"
#include "Compilation/VoxelDefaultCompilationNodes.h"
#include "VoxelGraphGenerator.h"
#include "VoxelWorldGeneratorInstance.inl"
#include "VoxelNodeFunctions.h"

EVoxelPinCategory UVoxelNode_WorldGeneratorSamplerBase::GetInputPinCategory(int32 PinIndex) const
{
	const int32 NumDefaultInputPins = Super::GetMinInputPins();
	if (PinIndex < NumDefaultInputPins)
	{
		return Super::GetInputPinCategory(PinIndex);
	}
	PinIndex -= NumDefaultInputPins;
	if (CustomData.IsValidIndex(PinIndex))
	{
		return EC::Float;
	}
	PinIndex -= CustomData.Num();
	if (Seeds.IsValidIndex(PinIndex))
	{
		return EC::Seed;
	}
	return EC::Float;
}

FName UVoxelNode_WorldGeneratorSamplerBase::GetInputPinName(int32 PinIndex) const
{
	const int32 NumDefaultInputPins = Super::GetMinInputPins();
	if (PinIndex < NumDefaultInputPins)
	{
		return Super::GetInputPinName(PinIndex);
	}
	PinIndex -= NumDefaultInputPins;
	if (CustomData.IsValidIndex(PinIndex))
	{
		return CustomData[PinIndex];
	}
	PinIndex -= CustomData.Num();
	if (Seeds.IsValidIndex(PinIndex))
	{
		return Seeds[PinIndex];
	}
	return "ERROR";
}

int32 UVoxelNode_WorldGeneratorSamplerBase::GetMinInputPins() const
{
	return Super::GetMinInputPins() + CustomData.Num() + Seeds.Num();
}

int32 UVoxelNode_WorldGeneratorSamplerBase::GetMaxInputPins() const
{
	return GetMinInputPins();
}

TSharedPtr<FVoxelCompilationNode> UVoxelNode_WorldGeneratorSamplerBase::GetCompilationNode() const
{
	auto Result = MakeShared<FVoxelAxisDependenciesCompilationNode>(*this);
	Result->DefaultDependencies = EVoxelAxisDependenciesFlags::X;
	return Result;
}

#if WITH_EDITOR
void UVoxelNode_WorldGeneratorSamplerBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Graph && GraphNode && PropertyChangedEvent.Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		GraphNode->ReconstructNode();
		Graph->CompileVoxelNodesFromGraphNodes();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_SingleWorldGeneratorSamplerBase::UVoxelNode_SingleWorldGeneratorSamplerBase()
{
	WorldGenerator = UVoxelFlatWorldGenerator::StaticClass();

	SetInputs(
		{ "X", EC::Float, "X" },
		{ "Y", EC::Float, "Y" },
		{ "Z", EC::Float, "Z" });
}

FText UVoxelNode_SingleWorldGeneratorSamplerBase::GetTitle() const
{
	return FText::Format(
		NSLOCTEXT("Voxel", "World Generator", "World Generator: {0}"),
		FText::FromString(UniqueName.ToString()));
}

void UVoxelNode_SingleWorldGeneratorSamplerBase::LogErrors(FVoxelGraphErrorReporter& ErrorReporter)
{
	Super::LogErrors(ErrorReporter);

	if (!WorldGenerator.IsValid())
	{
		ErrorReporter.AddMessageToNode(this, "invalid world generator", EVoxelGraphNodeMessageType::FatalError);
	}
}

#if WITH_EDITOR
bool UVoxelNode_SingleWorldGeneratorSamplerBase::TryImportFromProperty(UProperty* Property, UObject* Object)
{
	if (auto* Prop = Cast<UStructProperty>(Property))
	{
		if (Prop->GetCPPType(nullptr, 0) == "FVoxelWorldGeneratorPicker")
		{
			WorldGenerator = *Prop->ContainerPtrToValuePtr<FVoxelWorldGeneratorPicker>(Object);
			return true;
		}
	}
	return false;
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class FVoxelWorldGeneratorSamplerComputeNode : public FVoxelDataComputeNode, public FVoxelComputeNodeWithCustomData
{
public:
	FVoxelWorldGeneratorSamplerComputeNode(const UVoxelNode_SingleWorldGeneratorSamplerBase& Node, const FVoxelCompilationNode& CompilationNode)
		: FVoxelDataComputeNode(Node, CompilationNode)
		, FVoxelComputeNodeWithCustomData(Node.CustomData, 3)
		, WorldGenerator(Node.WorldGenerator.GetInstance(false))
		, SeedsNames(Node.Seeds)
		, Variable(MakeShared<FVoxelWorldGeneratorVariable>(Node, Node.WorldGenerator))
	{
	}

	void Init(Seed Inputs[], const FVoxelWorldGeneratorInit& InitStruct) override
	{
		FVoxelWorldGeneratorInit InitStructCopy = InitStruct;

		for (int32 I = 0; I < SeedsNames.Num(); I++)
		{
			InitStructCopy.Seeds.Add(SeedsNames[I], Inputs[NumDefaultInputPins + CustomData.Num() + I]);
		}

		WorldGenerator->Init(InitStructCopy);
	}
	void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override
	{
		Constructor.StartBlock();
		Constructor.AddLine("FVoxelWorldGeneratorInit InitCopy = " + FVoxelCppIds::InitStruct + ";");
		FVoxelCppUtils::CreateMapString(Constructor, "InitCopy.Seeds", SeedsNames, Inputs, NumDefaultInputPins + CustomData.Num());
		Constructor.AddLine(Variable->Name + "->Init(InitCopy);");
		Constructor.EndBlock();
	}
	void SetupCpp(FVoxelCppConfig& Config) const override
	{
		Config.AddExposedVariable(Variable);
		Config.AddInclude("VoxelWorldGeneratorPicker.h");
	}

protected:
	const TVoxelSharedRef<FVoxelWorldGeneratorInstance> WorldGenerator;
	const TArray<FName> SeedsNames;
	const TSharedRef<FVoxelWorldGeneratorVariable> Variable;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_GetWorldGeneratorValue::UVoxelNode_GetWorldGeneratorValue()
{
	SetOutputs({ "", EC::Float, "Value" });
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GetWorldGeneratorValue::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelWorldGeneratorSamplerComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		using FVoxelWorldGeneratorSamplerComputeNode::FVoxelWorldGeneratorSamplerComputeNode;

		virtual void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			const auto CustomDataMap = GetCustomData(Inputs, nullptr);
			Outputs[0].Get<v_flt>() = WorldGenerator->GetValue(
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context.LOD,
				FVoxelItemStack(Context.Items.ItemHolder).WithCustomData(&CustomDataMap));
		}
		virtual void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange & Context) const override
		{
			const auto CustomDataMap = GetCustomData(Inputs, nullptr);
			Outputs[0].Get<v_flt>() = WorldGenerator->GetValueRange(
				FVoxelNodeFunctions::BoundsFromRanges(
					Inputs[0].Get<v_flt>(),
					Inputs[1].Get<v_flt>(),
					Inputs[2].Get<v_flt>()),
				Context.LOD,
				FVoxelItemStack(Context.Items.ItemHolder).WithCustomData(&CustomDataMap));
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			GetCustomData(Constructor, Inputs, false, false);
			Constructor.AddLinef(TEXT("%s = %s->GetValue(%s, %s, %s, %s.LOD, FVoxelItemStack(%s.Items.ItemHolder).WithCustomData(&CustomDataMap));"),
				*Outputs[0],
				*Variable->Name,
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context,
				*FVoxelCppIds::Context);
			Constructor.EndBlock();
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			GetCustomData(Constructor, Inputs, true, false);
			Constructor.AddLinef(TEXT("%s = %s->GetValueRange(FVoxelNodeFunctions::BoundsFromRanges(%s, %s, %s), %s.LOD, FVoxelItemStack(%s.Items.ItemHolder).WithCustomData(&CustomDataMap));"),
				*Outputs[0],
				*Variable->Name,
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context,
				*FVoxelCppIds::Context);
			Constructor.EndBlock();
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_GetWorldGeneratorMaterial::UVoxelNode_GetWorldGeneratorMaterial()
{
	SetOutputs({ "", EC::Material, "Material" });
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GetWorldGeneratorMaterial::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelWorldGeneratorSamplerComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		using FVoxelWorldGeneratorSamplerComputeNode::FVoxelWorldGeneratorSamplerComputeNode;

		virtual void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			const auto CustomDataMap = GetCustomData(Inputs, nullptr);
			Outputs[0].Get<FVoxelMaterial>() = WorldGenerator->GetMaterial(
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context.LOD,
				FVoxelItemStack(Context.Items.ItemHolder));
		}
		virtual void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			GetCustomData(Constructor, Inputs, false, false);
			Constructor.AddLinef(TEXT("%s = %s->GetMaterial(%s, %s, %s, %s.LOD, FVoxelItemStack(%s.Items.ItemHolder));"),
				*Outputs[0],
				*Variable->Name,
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context,
				*FVoxelCppIds::Context);
			Constructor.EndBlock();
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
		}
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_GetWorldGeneratorCustomOutput::UVoxelNode_GetWorldGeneratorCustomOutput()
{
	SetOutputs({ "", EC::Float, "Custom Output Value" });
}

FText UVoxelNode_GetWorldGeneratorCustomOutput::GetTitle() const
{
	return FText::FromString("Get World Generator Custom Output: " + OutputName.ToString());
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GetWorldGeneratorCustomOutput::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelWorldGeneratorSamplerComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY();

		FLocalVoxelComputeNode(const UVoxelNode_GetWorldGeneratorCustomOutput& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelWorldGeneratorSamplerComputeNode(Node, CompilationNode)
			, OutputName(Node.OutputName)
		{
		}

		virtual void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			const auto CustomDataMap = GetCustomData(Inputs, nullptr);
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetWorldGeneratorCustomOutput(
				*WorldGenerator,
				OutputName,
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context,
				CustomDataMap);
		}
		virtual void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			const auto CustomDataMap = GetCustomData(Inputs, nullptr);
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetWorldGeneratorCustomOutput(
				*WorldGenerator,
				OutputName,
				Inputs[0].Get<v_flt>(),
				Inputs[1].Get<v_flt>(),
				Inputs[2].Get<v_flt>(),
				Context,
				CustomDataMap);
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			GetCustomData(Constructor, Inputs, false, false);
			FVoxelCppUtils::DeclareStaticName(Constructor, OutputName);
			Constructor.AddLinef(TEXT("%s = FVoxelNodeFunctions::GetWorldGeneratorCustomOutput(%s, StaticName, %s, %s, %s, %s, CustomDataMap);"),
				*Outputs[0],
				*Variable->Name,
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context);
			Constructor.EndBlock();
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			GetCustomData(Constructor, Inputs, true, false);
			FVoxelCppUtils::DeclareStaticName(Constructor, OutputName);
			Constructor.AddLinef(TEXT("%s = FVoxelNodeFunctions::GetWorldGeneratorCustomOutput(%s, StaticName, %s, %s, %s, %s, CustomDataMap);"),
				*Outputs[0],
				*Variable->Name,
				*Inputs[0],
				*Inputs[1],
				*Inputs[2],
				*FVoxelCppIds::Context);
			Constructor.EndBlock();
		}

	private:
		const FName OutputName;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_GetCustomData::UVoxelNode_GetCustomData()
{
	SetOutputs(EC::Float);
}

FText UVoxelNode_GetCustomData::GetTitle() const
{
	return FText::FromString("Custom Data: " + Name.ToString());
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_GetCustomData::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

			FLocalVoxelComputeNode(const UVoxelNode_GetCustomData& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Name(Node.Name)
		{
		}

		virtual void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetCustomData(Context, Name);
		}
		virtual void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<v_flt>() = FVoxelNodeFunctions::GetCustomData(Context, Name);
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			FVoxelCppUtils::DeclareStaticName(Constructor, Name);
			Constructor.AddLinef(TEXT("%s = FVoxelNodeFunctions::GetCustomData(%s, StaticName);"), *Outputs[0], *FVoxelCppIds::Context);
			Constructor.EndBlock();
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}

	private:
		const FName Name;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_IsCustomDataSet::UVoxelNode_IsCustomDataSet()
{
	SetOutputs(EC::Boolean);
}

FText UVoxelNode_IsCustomDataSet::GetTitle() const
{
	return FText::FromString("Is Custom Data Set: " + Name.ToString());
}

TVoxelSharedPtr<FVoxelComputeNode> UVoxelNode_IsCustomDataSet::GetComputeNode(const FVoxelCompilationNode& InCompilationNode) const
{
	class FLocalVoxelComputeNode : public FVoxelDataComputeNode
	{
	public:
		GENERATED_DATA_COMPUTE_NODE_BODY()

			FLocalVoxelComputeNode(const UVoxelNode_IsCustomDataSet& Node, const FVoxelCompilationNode& CompilationNode)
			: FVoxelDataComputeNode(Node, CompilationNode)
			, Name(Node.Name)
		{
		}

		virtual void Compute(const FVoxelNodeType Inputs[], FVoxelNodeType Outputs[], const FVoxelContext& Context) const override
		{
			Outputs[0].Get<bool>() = FVoxelNodeFunctions::IsCustomDataSet(Context, Name);
		}
		virtual void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override
		{
			Outputs[0].Get<bool>() = FVoxelNodeFunctions::IsCustomDataSet(Context, Name);
		}
		virtual void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			Constructor.StartBlock();
			FVoxelCppUtils::DeclareStaticName(Constructor, Name);
			Constructor.AddLinef(TEXT("%s = FVoxelNodeFunctions::IsCustomDataSet(%s, StaticName);"), *Outputs[0], *FVoxelCppIds::Context);
			Constructor.EndBlock();
		}
		virtual void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override
		{
			ComputeCpp(Inputs, Outputs, Constructor);
		}

	private:
		const FName Name;
	};
	return MakeShareable(new FLocalVoxelComputeNode(*this, InCompilationNode));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelComputeNodeWithCustomData::FVoxelComputeNodeWithCustomData(const TArray<FName>& CustomData, int32 NumDefaultInputPins)
	: CustomData(CustomData)
	, NumDefaultInputPins(NumDefaultInputPins)
{
}

FVoxelGraphCustomData FVoxelComputeNodeWithCustomData::GetCustomData(const FVoxelNodeType Inputs[], const FVoxelItemStack* Items) const
{
	FVoxelGraphCustomData CustomDataMap;
	if (Items && Items->CustomData) CustomDataMap.Append(*reinterpret_cast<FVoxelGraphCustomData*>(Items->CustomData));
	for (int32 Index = 0; Index < CustomData.Num(); Index++)
	{
		CustomDataMap.Add(CustomData[Index], Inputs[NumDefaultInputPins + Index].Get<v_flt>());
	}
	return CustomDataMap;
}

FVoxelGraphCustomDataRange FVoxelComputeNodeWithCustomData::GetCustomData(const FVoxelNodeRangeType Inputs[], const FVoxelItemStack* Items) const
{
	FVoxelGraphCustomDataRange CustomDataMap;
	if (Items && Items->CustomData) CustomDataMap.Append(*reinterpret_cast<FVoxelGraphCustomDataRange*>(Items->CustomData));
	for (int32 Index = 0; Index < CustomData.Num(); Index++)
	{
		CustomDataMap.Add(CustomData[Index], Inputs[NumDefaultInputPins + Index].Get<v_flt>());
	}
	return CustomDataMap;
}

void FVoxelComputeNodeWithCustomData::GetCustomData(FVoxelCppConstructor& Constructor, const TArray<FString>& Inputs, bool bIsRange, bool bCopyFromItems) const
{
	const FString CustomDataString = bIsRange ? "FVoxelGraphCustomDataRange" : "FVoxelGraphCustomData";
	Constructor.AddLinef(TEXT("%s CustomDataMap;"), *CustomDataString);
	if (bCopyFromItems)
	{
		Constructor.AddLinef(TEXT("if (%s.Items.CustomData) CustomDataMap.Append(*reinterpret_cast<%s*>(%s.Items.CustomData));"),
			*FVoxelCppIds::Context,
			*CustomDataString,
			*FVoxelCppIds::Context);
	}
	FVoxelCppUtils::CreateMapString(Constructor, "CustomDataMap", CustomData, Inputs, NumDefaultInputPins);
}