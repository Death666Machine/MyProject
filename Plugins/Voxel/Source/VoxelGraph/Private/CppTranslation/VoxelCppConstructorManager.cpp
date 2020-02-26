// Copyright 2020 Phyronnaz

#include "CppTranslation/VoxelCppConstructorManager.h"
#include "VoxelGraphOutputs.h"
#include "VoxelGraphGenerator.h"
#include "VoxelGraphErrorReporter.h"
#include "VoxelGraphConstants.h"

#include "CppTranslation/VoxelVariables.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppConfig.h"

#include "Runtime/VoxelGraph.h"
#include "Runtime/VoxelComputeNode.h"
#include "Runtime/VoxelCompiledGraphs.h"

struct FVoxelCppStructConfig
{
	const FVoxelGraphPermutationArray Permutation;
	const FString Name;
	const FString StructName;
	const TArray<FVoxelGraphOutput> Outputs;

	FVoxelCppStructConfig(const FVoxelGraphPermutationArray& Permutation, const FString& InName, const TArray<FVoxelGraphOutput>& Outputs)
		: Permutation(Permutation)
		, Name(InName)
		, StructName("FLocalComputeStruct_" + Name)
		, Outputs(Outputs)
	{
	}

	inline bool IsSingleOutput(EVoxelDataPinCategory Category) const
	{
		return Outputs.Num() == 1 && Outputs[0].Category == Category;
	}
	inline bool IsSingleOutputRange(EVoxelDataPinCategory Category) const
	{
		return
			Outputs.Num() == 2 &&
			Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex) &&
			GetRangeGraphOutput().Category == Category;
	}
	// With range analysis there's a dummy output, ignore it
	inline const FVoxelGraphOutput& GetRangeGraphOutput() const
	{
		check(Outputs.Num() == 2 && Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex));
		if (Outputs[0].Index == FVoxelGraphOutputsIndices::RangeAnalysisIndex)
		{
			return Outputs[1];
		}
		else
		{
			check(Outputs[1].Index == FVoxelGraphOutputsIndices::RangeAnalysisIndex);
			return Outputs[0];
		}
	}
};

inline void AddConstructor(
	FVoxelCppConstructor& Cpp,
	const FString& ClassName,
	const TArray<TSharedRef<FVoxelExposedVariable>>& Variables,
	const TArray<FString>& AdditionalInputs = {},
	const FString& ParentInit = "",
	const TArray<FString>& AdditionalInits = {})
{
	// We need to add In to parameters, as else refs are made to them instead of class vars
	{
		FString Parameters;
		for (auto& Variable : Variables)
		{
			if (!Parameters.IsEmpty())
			{
				Parameters += ", ";
			}
			Parameters += "const " + Variable->Type + "& In" + Variable->Name;
		}
		for (auto& Input : AdditionalInputs)
		{
			if (!Parameters.IsEmpty())
			{
				Parameters += ", ";
			}
			Parameters += Input;
		}

		Cpp.AddLine(ClassName + "(" + Parameters + ")");
	}
	Cpp.Indent();
	{
		bool bFirst = true;
		if (!ParentInit.IsEmpty())
		{
			bFirst = false;
			Cpp.AddLine(": " + ParentInit);
		}

		for (auto& Variable : Variables)
		{
			Cpp.AddLine((bFirst ? ": " : ", ") + Variable->Name + "(" + "In" + Variable->Name + ")");
			bFirst = false;
		}

		for (auto& AdditionalInit : AdditionalInits)
		{
			Cpp.AddLine((bFirst ? ": " : ", ") + AdditionalInit);
			bFirst = false;
		}
	}
	Cpp.Unindent();
	Cpp.AddLine("{");
	Cpp.AddLine("}");
}

inline void AddCppStruct(FVoxelCppConstructor& Cpp, FVoxelCppConstructor& GlobalScopeCpp, const FVoxelCppConfig& Config, const FVoxelGraph& Graph, const FVoxelCppStructConfig& StructConfig)
{
	TArray<FVoxelGraphOutput> Outputs;
	for (auto& Output : StructConfig.Outputs)
	{
		if (Output.Index != FVoxelGraphOutputsIndices::RangeAnalysisIndex)
		{
			Outputs.Add(Output);
		}
	}

	TArray<FString> GraphOutputs;
	for (auto& Output : Outputs)
	{
		if (uint32(GraphOutputs.Num()) <= Output.Index)
		{
			GraphOutputs.SetNum(Output.Index + 1);
		}
		GraphOutputs[Output.Index] = FVoxelCppIds::GraphOutputs + "." + Output.Name.ToString();
	}

	Cpp.AddLine("class " + StructConfig.StructName);
	Cpp.EnterNamedScope(StructConfig.StructName);
	Cpp.StartBlock();
	Cpp.Public();
	{
		FVoxelCppVariableScope Scope(Cpp);

		// GraphOutputs struct
		Cpp.AddLine("struct " + FVoxelCppIds::GraphOutputsType);
		Cpp.EnterNamedScope(FVoxelCppIds::GraphOutputsType);
		Cpp.StartBlock();
		{
			Cpp.AddLine(FVoxelCppIds::GraphOutputsType + "() {}");
			Cpp.NewLine();

			Cpp.AddLine("template<typename T, uint32 Index>");
			Cpp.AddLine("inline auto& GetRef()");
			Cpp.StartBlock();
			Cpp.AddLine("unimplemented();");
			if (StructConfig.Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex))
			{
				Cpp.AddLine("return *(TVoxelRange<T>*)nullptr;");
			}
			else
			{
				Cpp.AddLine("return *(T*)nullptr;");
			}
			Cpp.EndBlock();

			for (auto& Output : Outputs)
			{
				GlobalScopeCpp.AddLine("template<>");
				GlobalScopeCpp.AddLine("inline auto& " + Cpp.GetScopeAccessor() + "GetRef<" + FVoxelPinCategory::GetTypeString(Output.Category) + ", " + FString::FromInt(Output.Index) + ">()");
				GlobalScopeCpp.StartBlock();
				GlobalScopeCpp.AddLine("return " + Output.Name.ToString() + ";");
				GlobalScopeCpp.EndBlock();
			}

			Cpp.NewLine();

			for (auto& Output : Outputs)
			{
				Cpp.AddLine(Output.GetDeclaration(Cpp) + ";");
			}
		}
		Cpp.EndBlock(true);
		Cpp.ExitNamedScope(FVoxelCppIds::GraphOutputsType);

		// Cache structs
		for (auto Dependency : {
			EVoxelAxisDependencies::Constant,
			EVoxelAxisDependencies::X,
			EVoxelAxisDependencies::XY })
		{
			Cpp.AddLine("struct " + FVoxelCppIds::GetCacheType(Dependency));
			Cpp.StartBlock();
			{
				Cpp.AddLine(FVoxelCppIds::GetCacheType(Dependency) + "() {}");
				Cpp.NewLine();

				if (Dependency == EVoxelAxisDependencies::Constant)
				{
					TSet<FVoxelComputeNode*> ConstantNodes;
					Graph.GetConstantNodes(ConstantNodes);
					for (auto* Node : ConstantNodes)
					{
						check(Node->Type == EVoxelComputeNodeType::Data);
						Node->DeclareOutputs(Cpp, FVoxelVariableAccessInfo::StructDeclaration(Dependency));
					}
				}
				else
				{
					TSet<FVoxelComputeNode*> Nodes;
					Graph.GetNotConstantNodes(Nodes);
					for (auto* Node : Nodes)
					{
						// We don't want to cache functions or seed outputs
						if (Node->Type == EVoxelComputeNodeType::Data)
						{
							Node->DeclareOutputs(Cpp, FVoxelVariableAccessInfo::StructDeclaration(Dependency));
						}
						else
						{
							check(Node->Type == EVoxelComputeNodeType::Exec || Node->Type == EVoxelComputeNodeType::Seed);
						}
					}
				}
			}
			Cpp.EndBlock(true);
			Cpp.NewLine();
		}

		// Constructor
		AddConstructor(Cpp, StructConfig.StructName, Config.GetExposedVariables());
		Cpp.NewLine();

		// Init
		Cpp.AddLine("void Init(const FVoxelWorldGeneratorInit& " + FVoxelCppIds::InitStruct + ")");
		Cpp.StartBlock();
		{
			Cpp.AddLine("////////////////////////////////////////////////////");
			Cpp.AddLine("//////////////////// Init nodes ////////////////////");
			Cpp.AddLine("////////////////////////////////////////////////////");
			Cpp.StartBlock();
			Graph.Init(Cpp);
			Cpp.EndBlock();
			Cpp.NewLine();

			Cpp.AddLine("////////////////////////////////////////////////////");
			Cpp.AddLine("//////////////// Compute constants /////////////////");
			Cpp.AddLine("////////////////////////////////////////////////////");
			Cpp.StartBlock();
			Graph.ComputeConstants(Cpp);
			Cpp.EndBlock();
		}
		Cpp.EndBlock();

		// Computes
		for (EVoxelFunctionAxisDependencies Dependencies : FVoxelAxisDependencies::GetAllFunctionDependencies())
		{
			if (StructConfig.Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex) &&
				Dependencies != EVoxelFunctionAxisDependencies::XYZWithoutCache)
			{
				continue;
			}

			FString Line = "void Compute" + FVoxelAxisDependencies::ToString(Dependencies) + "(";
			Line += "const " + Cpp.GetContextTypeString() + "& " + FVoxelCppIds::Context;

			if (Dependencies == EVoxelFunctionAxisDependencies::X ||
				Dependencies == EVoxelFunctionAxisDependencies::XYWithoutCache || // Still need to compute X variables
				Dependencies == EVoxelFunctionAxisDependencies::XYWithCache ||
				Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
			{
				Line += ", ";
				if (Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
				{
					Line += "const ";
				}
				Line += FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::X) + "& " + FVoxelCppIds::GetCacheName(EVoxelAxisDependencies::X);
			}

			if (Dependencies == EVoxelFunctionAxisDependencies::XYWithoutCache ||
				Dependencies == EVoxelFunctionAxisDependencies::XYWithCache ||
				Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
			{
				Line += ", ";
				if (Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache)
				{
					Line += "const ";
				}
				Line += FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::XY) + "& " + FVoxelCppIds::GetCacheName(EVoxelAxisDependencies::XY);
			}

			if (Dependencies == EVoxelFunctionAxisDependencies::XYZWithCache || Dependencies == EVoxelFunctionAxisDependencies::XYZWithoutCache)
			{
				Line += ", " + FVoxelCppIds::GraphOutputsType + "& " + FVoxelCppIds::GraphOutputs;
			}
			Line += ") const";
			Cpp.AddLine(Line);
			Cpp.StartBlock();
			Graph.Compute(Cpp, Dependencies);
			Cpp.EndBlock();
		}

		// Getters
		Cpp.NewLine();
		Cpp.AddLinef(TEXT("inline %s GetBufferX() const { return {}; }"), *FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::X));
		Cpp.AddLinef(TEXT("inline %s GetBufferXY() const { return {}; }"), *FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::XY));
		Cpp.AddLinef(TEXT("inline %s GetOutputs() const { return {}; }"), *FVoxelCppIds::GraphOutputsType);
		Cpp.NewLine();

		Cpp.Private();

		// Constant cache
		Cpp.AddLine(FVoxelCppIds::GetCacheType(EVoxelAxisDependencies::Constant) + " " + FVoxelCppIds::GetCacheName(EVoxelAxisDependencies::Constant) + ";");

		// Private node variables
		{
			TSet<FVoxelComputeNode*> Nodes;
			Graph.GetAllNodes(Nodes);
			TArray<FVoxelVariable> PrivateVariables;
			for (auto* Node : Nodes)
			{
				Node->GetPrivateVariables(PrivateVariables);
			}
			for (auto& Variable : PrivateVariables)
			{
				Cpp.AddLine(Variable.GetDeclaration() + ";");
			}
		}
		Cpp.NewLine();

		// Exposed variables refs
		for (auto& ExposedVariable : Config.GetExposedVariables())
		{
			Cpp.AddLine(ExposedVariable->GetConstRefDeclaration() + ";");
		}

		// Functions
		Cpp.NewLine();
		Cpp.AddLine("///////////////////////////////////////////////////////////////////////");
		Cpp.AddLine("//////////////////////////// Init functions ///////////////////////////");
		Cpp.AddLine("///////////////////////////////////////////////////////////////////////");
		Cpp.NewLine();
		Graph.DeclareInitFunctions(Cpp);
		Cpp.AddLine("///////////////////////////////////////////////////////////////////////");
		Cpp.AddLine("////////////////////////// Compute functions //////////////////////////");
		Cpp.AddLine("///////////////////////////////////////////////////////////////////////");
		Cpp.NewLine();
		Graph.DeclareComputeFunctions(Cpp, GraphOutputs);
	}
	Cpp.EndBlock(true);
	Cpp.ExitNamedScope(StructConfig.StructName);
}

FVoxelCppConstructorManager::FVoxelCppConstructorManager(const FString& ClassName, UVoxelGraphGenerator* VoxelGraphGenerator)
	: ClassName(ClassName)
	, VoxelGraphGenerator(VoxelGraphGenerator)
	, Graphs(MakeUnique<FVoxelCompiledGraphs>())
	, ErrorReporter(MakeUnique<FVoxelGraphErrorReporter>(VoxelGraphGenerator))
{
	check(VoxelGraphGenerator);

	if (!VoxelGraphGenerator->CreateGraphs(*Graphs, false, false, false))
	{
		ErrorReporter->AddError("Compilation error!");
	}
}

FVoxelCppConstructorManager::~FVoxelCppConstructorManager()
{
	// UniquePtr forward decl
}

bool FVoxelCppConstructorManager::Compile(FString& OutHeader, FString& OutCpp)
{
	const bool bResult = CompileInternal(OutHeader, OutCpp);
	ErrorReporter->Apply(true);
	return bResult;
}

#define CHECK_FOR_ERRORS() if (ErrorReporter->HasFatalError()) { return false; }

bool FVoxelCppConstructorManager::CompileInternal(FString& OutHeader, FString& OutCpp)
{
	CHECK_FOR_ERRORS();

	TArray<FVoxelCppStructConfig> AllStructConfigs;
	TMap<FVoxelGraphPermutationArray, FVoxelCppStructConfig> PermutationToStructConfigs;
	TSet<FVoxelComputeNode*> Nodes;
	{
		auto Outputs = VoxelGraphGenerator->GetOutputs();
		// Check outputs names
		{
			TSet<FName> Names;
			for (auto& It : Outputs)
			{
				auto& Name = It.Value.Name;
				if (Name.ToString().IsEmpty())
				{
					ErrorReporter->AddError("Empty Output name!");
				}
				CHECK_FOR_ERRORS();
				if (Names.Contains(Name))
				{
					ErrorReporter->AddError("Multiple Outputs have the same name! (" + Name.ToString() + ")");
				}
				CHECK_FOR_ERRORS();
				Names.Add(Name);
			}
		}
		for (const FVoxelGraphPermutationArray& Permutation : VoxelGraphGenerator->GetPermutations())
		{
			if (Permutation.Num() == 0)
			{
				continue;
			}

			const FString Name = "Local" + FVoxelGraphOutputsUtils::GetPermutationName(Permutation, Outputs);
			TArray<FVoxelGraphOutput> PermutationOutputs;
			for (uint32 Index : Permutation)
			{
				PermutationOutputs.Add(Outputs[Index]);
			}

			const auto& Graph = Graphs->Get(Permutation);
			Graph->GetAllNodes(Nodes);

			FVoxelCppStructConfig StructConfig(Permutation, Name, PermutationOutputs);
			AllStructConfigs.Add(StructConfig);
			PermutationToStructConfigs.Add(Permutation, StructConfig);
		}
	}

	PermutationToStructConfigs.KeySort([](const FVoxelGraphPermutationArray& A, const FVoxelGraphPermutationArray& B)
	{
		if (A.Num() < B.Num())
		{
			return true;
		}
		if (A.Num() > B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); Index++)
		{
			if (A[Index] > B[Index])
			{
				return false;
			}
		}
		return true;
	});

	FVoxelCppConfig Config(*ErrorReporter);
	Config.AddInclude("CoreMinimal.h");
	Config.AddInclude("VoxelGraphGeneratorHelpers.h");
	Config.AddInclude("VoxelNodeFunctions.h");
	for (auto* Node : Nodes)
	{
		Node->CallSetupCpp(Config);
	}
	CHECK_FOR_ERRORS();
	Config.AddInclude(ClassName + ".generated.h");
	Config.BuildExposedVariablesArray();

	const FString InstanceClassName("F" + ClassName + "Instance");
	const FString MainClassName("U" + ClassName);

	FVoxelCppConstructor Header({}, *ErrorReporter);
	FVoxelCppConstructor Cpp({}, *ErrorReporter);

	// Header Intro
	Header.AddLine("// Copyright 2020 Phyronnaz");
	Header.NewLine();
	Header.AddLine("#pragma once");
	Header.NewLine();

	// Includes
	for (auto& Include : Config.GetIncludes())
	{
		Header.AddLine(Include.ToString());
	}
	Header.NewLine();

	// Cpp Intro
	Cpp.AddLine("// Copyright 2020 Phyronnaz");
	Cpp.NewLine();
	Cpp.AddLine("#ifdef __clang__");
	Cpp.AddLine("#pragma clang diagnostic push");
	Cpp.AddLine("#pragma clang diagnostic ignored \"-Wnull-dereference\"");
	Cpp.AddLine("#else");
	Cpp.AddLine("#pragma warning(push)");
	Cpp.AddLine("#pragma warning(disable : 4101 4701)");
	Cpp.AddLine("#endif");
	Cpp.NewLine();
	Cpp.AddLine(FVoxelCppInclude(ClassName + ".h").ToString());
	Cpp.NewLine();
	Cpp.AddLine("using Seed = int32;");
	Cpp.NewLine();

	// Instance
	{
		// For the outputs templates specialization
		FVoxelCppConstructor GlobalScopeCpp({}, *ErrorReporter);

		Cpp.AddLine("class " + InstanceClassName + " : public TVoxelGraphGeneratorInstanceHelper<" + InstanceClassName + ", " + MainClassName + ">");
		Cpp.StartBlock();
		Cpp.Public();
		{
			// Define structs
			for (auto& StructConfig : AllStructConfigs)
			{
				const auto& Graph = *Graphs->Get(StructConfig.Permutation);

				FVoxelCppConstructor LocalCpp(StructConfig.Permutation, *ErrorReporter);
				LocalCpp.EnterNamedScope(InstanceClassName);
				AddCppStruct(LocalCpp, GlobalScopeCpp, Config, Graph, StructConfig);
				LocalCpp.ExitNamedScope(InstanceClassName);
				CHECK_FOR_ERRORS();

				Cpp.AddOtherConstructor(LocalCpp);
			}
			Cpp.NewLine();

			// Constructor
			{
				const auto GetMaps = [&](EVoxelDataPinCategory Category, auto Lambda)
				{
					FString Map;
					for (auto& FlagConfig : AllStructConfigs)
					{
						if (FlagConfig.Outputs.Num() != 1)
						{
							continue;
						}
						auto& Output = FlagConfig.Outputs[0];
						if (Output.Category == Category)
						{
							if (!Map.IsEmpty())
							{
								Map += ",";
							}
							Map += "\n\t\t\t{\"" + Output.Name.ToString() + "\", " + Lambda(Output.Index) + "}";
						}
					}
					return Map;
				};

				// Add the initialization of the structs (passing them refs to exposed variables)
				TArray<FString> Inits;
				for (auto& FlagConfig : AllStructConfigs)
				{
					FString Parameters;
					for (auto& Variable : Config.GetExposedVariables())
					{
						if (!Parameters.IsEmpty())
						{
							Parameters += ", ";
						}
						Parameters += Variable->Name;
					}
					Inits.Add(FlagConfig.Name + "(" + Parameters + ")");
				}
				AddConstructor(
					Cpp,
					InstanceClassName,
					Config.GetExposedVariables(),
					{ "bool bEnableRangeAnalysis" },
					FString("TVoxelGraphGeneratorInstanceHelper(")
					///////////////////////////////////////////////////////////////////////////////
					+ "\n\t\t{"
					+ GetMaps(EVoxelDataPinCategory::Float, [](uint32 Index) { return FString::FromInt(Index); })
					+ "\n\t\t},"
					+ "\n\t\t{"
					+ GetMaps(EVoxelDataPinCategory::Int, [](uint32 Index) { return FString::FromInt(Index); })
					+ "\n\t\t},"
					///////////////////////////////////////////////////////////////////////////////
					+ "\n\t\t{"
					+ GetMaps(EVoxelDataPinCategory::Float, [](uint32 Index) { return FString::Printf(TEXT("NoTransformAccessor<v_flt>::Get<%u, TOutputFunctionPtr<v_flt>>()"), Index); })
					+ "\n\t\t},"
					+ "\n\t\t{"
					+ GetMaps(EVoxelDataPinCategory::Int, [](uint32 Index) { return FString::Printf(TEXT("NoTransformAccessor<int32>::Get<%u, TOutputFunctionPtr<int32>>()"), Index); })
					+ "\n\t\t},"
					+ "\n\t\t{"
					+ GetMaps(EVoxelDataPinCategory::Float, [](uint32 Index) { return FString::Printf(TEXT("NoTransformRangeAccessor<v_flt>::Get<%u, TRangeOutputFunctionPtr<v_flt>>()"), Index); })
					+ "\n\t\t},"
					///////////////////////////////////////////////////////////////////////////////
					+ "\n\t\t{"
					+ GetMaps(EVoxelDataPinCategory::Float, [](uint32 Index) { return FString::Printf(TEXT("WithTransformAccessor<v_flt>::Get<%u, TOutputFunctionPtr_Transform<v_flt>>()"), Index); })
					+ "\n\t\t},"
					+ "\n\t\t{"
					+ GetMaps(EVoxelDataPinCategory::Int, [](uint32 Index) { return FString::Printf(TEXT("WithTransformAccessor<int32>::Get<%u, TOutputFunctionPtr_Transform<int32>>()"), Index); })
					+ "\n\t\t},"
					+ "\n\t\t{"
					+ GetMaps(EVoxelDataPinCategory::Float, [](uint32 Index) { return FString::Printf(TEXT("WithTransformRangeAccessor<v_flt>::Get<%u, TRangeOutputFunctionPtr_Transform<v_flt>>()"), Index); })
					+ "\n\t\t},"
					///////////////////////////////////////////////////////////////////////////////
					+ "\n\t\tbEnableRangeAnalysis)",
					Inits);
			}
			Cpp.NewLine();

			// Init
			Cpp.AddLine("virtual void Init(const FVoxelWorldGeneratorInit& " + FVoxelCppIds::InitStruct + ") override final");
			Cpp.StartBlock();
			{
				for (auto& FlagConfig : AllStructConfigs)
				{
					Cpp.AddLine(FlagConfig.Name + ".Init(" + FVoxelCppIds::InitStruct + ");");
				}
			}
			Cpp.EndBlock();

			const auto PermutationToString = [](FVoxelGraphPermutationArray Permutation)
			{
				Permutation.Sort();
				FString String;
				for (uint32 Index : Permutation)
				{
					if (!String.IsEmpty())
					{
						String += ", ";
					}
					String += FString::FromInt(Index);
				}
				return String;
			};

			for (auto& PermutationIt : PermutationToStructConfigs)
			{
				const FVoxelGraphPermutationArray& Permutation = PermutationIt.Key;
				const FString PermutationString = PermutationToString(Permutation);
				const FString ScopeAccessor = InstanceClassName;

				GlobalScopeCpp.AddLine("template<>");
				if (Permutation.Contains(FVoxelGraphOutputsIndices::RangeAnalysisIndex))
				{
					GlobalScopeCpp.AddLinef(TEXT("inline auto& %s::GetRangeTarget<%s>() const"), *ScopeAccessor, *PermutationString);
				}
				else
				{
					GlobalScopeCpp.AddLinef(TEXT("inline auto& %s::GetTarget<%s>() const"), *ScopeAccessor, *PermutationString);
				}
				GlobalScopeCpp.StartBlock();
				GlobalScopeCpp.AddLinef(TEXT("return %s;"), *PermutationIt.Value.Name);
				GlobalScopeCpp.EndBlock();
			}

			Cpp.NewLine();
			Cpp.AddLine("template<uint32... Permutation>");
			Cpp.AddLine("auto& GetTarget() const;");
			Cpp.NewLine();
			Cpp.AddLine("template<uint32... Permutation>");
			Cpp.AddLine("auto& GetRangeTarget() const;");
			Cpp.NewLine();
			Cpp.AddLine("inline void ReportRangeAnalysisFailure() const {}");
			Cpp.NewLine();
			Cpp.Private();
			for (auto& Variable : Config.GetExposedVariables())
			{
				Cpp.AddLine(Variable->GetConstDeclaration() + ";");
			}
			for (auto& StructConfig : AllStructConfigs)
			{
				Cpp.AddLine(StructConfig.StructName + " " + StructConfig.Name + ";");
			}
			Cpp.NewLine();
		}
		Cpp.EndBlock(true);

		// Add the specializations
		Cpp.NewLine();
		Cpp.AddOtherConstructor(GlobalScopeCpp);
	}

	Cpp.NewLine();
	Cpp.AddLine("////////////////////////////////////////////////////////////");
	Cpp.AddLine("////////////////////////// UCLASS //////////////////////////");
	Cpp.AddLine("////////////////////////////////////////////////////////////");
	Cpp.NewLine();

	// UClass
	{
		Header.AddLine("UCLASS(Blueprintable)");
		Header.AddLine("class " + MainClassName + " : public UVoxelGraphGeneratorHelper");
		Header.StartBlock();
		{
			Header.AddLine("GENERATED_BODY()");
			Header.NewLine();
			Header.Public();

			for (auto& Variable : Config.GetExposedVariables())
			{
				Header.AddLinef(TEXT("// %s"), *Variable->Tooltip);
				FString MetadataString = Variable->GetMetadataString();
				if (!MetadataString.IsEmpty())
				{
					MetadataString = ", meta=(" + MetadataString + ")";
				}
				Header.AddLinef(TEXT("UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=\"%s\"%s)"), *Variable->Category, *MetadataString);
				Header.AddLine(Variable->ExposedType + " " + Variable->Name + (!Variable->DefaultValue.IsEmpty() ? " = " + Variable->DefaultValue + ";" : ";"));
			}

			Header.NewLine();
			Header.AddLine(MainClassName + "();");

			Cpp.AddLine(MainClassName + "::" + MainClassName + "()");
			Cpp.StartBlock();
			Cpp.AddLine("bEnableRangeAnalysis = " + LexToString(VoxelGraphGenerator->bEnableRangeAnalysis) + ";");
			Cpp.EndBlock();

			// GetDefaultSeeds
			Header.AddLine("virtual TMap<FName, int32> GetDefaultSeeds() const override;");
			Cpp.NewLine();
			Cpp.AddLine("TMap<FName, int32> " + MainClassName + "::GetDefaultSeeds() const");
			Cpp.StartBlock();
			{
				Cpp.AddLine("return {");
				Cpp.Indent();
				for (auto& It : VoxelGraphGenerator->GetDefaultSeeds())
				{
					Cpp.AddLinef(TEXT("{ \"%s\", %d },"), *It.Key.ToString(), It.Value);
				}
				Cpp.AddLine("};");
				Cpp.Unindent();
			}
			Cpp.EndBlock();

			// GetWorldGenerator
			Header.AddLine("virtual TVoxelSharedRef<FVoxelTransformableWorldGeneratorInstance> GetTransformableInstance() override;");
			Cpp.NewLine();
			Cpp.AddLine("TVoxelSharedRef<FVoxelTransformableWorldGeneratorInstance> " + MainClassName + "::GetTransformableInstance()");
			Cpp.StartBlock();
			{
				Cpp.AddLine("return MakeVoxelShared<" + InstanceClassName + ">(");
				Cpp.Indent();
				auto& Variables = Config.GetExposedVariables();
				for (int32 Index = 0; Index < Variables.Num(); Index++)
				{
					Cpp.AddLine(Variables[Index]->GetLocalVariableFromExposedOne() + ",");
				}
				Cpp.AddLine("bEnableRangeAnalysis);");
				Cpp.Unindent();
			}
			Cpp.EndBlock();
		}
		Header.EndBlock(true);
	}

	Cpp.NewLine();
	Cpp.AddLine("#ifdef __clang__");
	Cpp.AddLine("#pragma clang diagnostic pop");
	Cpp.AddLine("#else");
	Cpp.AddLine("#pragma warning(pop)");
	Cpp.AddLine("#endif");
	Cpp.NewLine();

	Header.GetCode(OutHeader);
	Cpp.GetCode(OutCpp);

	return !ErrorReporter->HasFatalError();
}