// Copyright 2020 Phyronnaz

#include "VoxelNodes/VoxelNoiseNodes.h"
#include "CppTranslation/VoxelCppIds.h"
#include "CppTranslation/VoxelCppConfig.h"
#include "CppTranslation/VoxelCppConstructor.h"
#include "VoxelGraphGenerator.h"

#define NOISE_SAMPLE_RANGE 10

FName UVoxelNode_NoiseNode::GetInputPinName(int32 PinIndex) const
{
	if (GetDimension() == 2)
	{
		if (PinIndex == 0)
		{
			return "X";
		}
		else if (PinIndex == 1)
		{
			return "Y";
		}
		else if (PinIndex == 2)
		{
			return "Frequency";
		}
		else
		{
			return "Seed";
		}
	}
	else
	{
		if (PinIndex == 0)
		{
			return "X";
		}
		else if (PinIndex == 1)
		{
			return "Y";
		}
		else if (PinIndex == 2)
		{
			return "Z";
		}
		else if (PinIndex == 3)
		{
			return "Frequency";
		}
		else
		{
			return "Seed";
		}
	}
}

FName UVoxelNode_NoiseNode::GetOutputPinName(int32 PinIndex) const
{
	if (PinIndex == 0)
	{
		return IsDerivative() ? "Value" : "";
	}
	else if (PinIndex == 1)
	{
		return "DX";
	}
	else if (PinIndex == 2)
	{
		return "DY";
	}
	else
	{
		return "DZ";
	}
}

FString UVoxelNode_NoiseNode::GetInputPinToolTip(int32 PinIndex) const
{
	if (GetDimension() == 2)
	{
		if (PinIndex == 0)
		{
			return "X";
		}
		else if (PinIndex == 1)
		{
			return "Y";
		}
		else if (PinIndex == 2)
		{
			return "The frequency of the noise";
		}
		else
		{
			return "The seed to use";
		}
	}
	else
	{
		if (PinIndex == 0)
		{
			return "X";
		}
		else if (PinIndex == 1)
		{
			return "Y";
		}
		else if (PinIndex == 2)
		{
			return "Z";
		}
		else if (PinIndex == 3)
		{
			return "The frequency of the noise";
		}
		else
		{
			return "The seed to use";
		}
	}
}

FString UVoxelNode_NoiseNode::GetOutputPinToolTip(int32 PinIndex) const
{
	if (PinIndex == 0)
	{
		return "The noise value";
	}
	else if (PinIndex == 1)
	{
		return "The derivative along the X axis. Can be used to compute the slope of the noise using GetSlopeFromDerivatives.";
	}
	else if (PinIndex == 2)
	{
		return "The derivative along the Y axis. Can be used to compute the slope of the noise using GetSlopeFromDerivatives.";
	}
	else
	{
		return "The derivative along the Z axis. Can be used to compute the slope of the noise using GetSlopeFromDerivatives.";
	}
}

FString UVoxelNode_NoiseNode::GetInputPinDefaultValue(int32 PinIndex) const
{
	if (PinIndex == GetDimension())
	{
		return FString::SanitizeFloat(Frequency);
	}
	else
	{
		return "";
	}
}

///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void UVoxelNode_NoiseNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.MemberProperty &&
		PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		OutputRanges = FVoxelNoiseComputeNode::ComputeOutputRanges(this, NumberOfSamples);
	}
}

bool UVoxelNode_NoiseNode::CanEditChange(const UProperty* InProperty) const
{
	return
		Super::CanEditChange(InProperty) &&
		(NeedRangeAnalysis() || InProperty->GetFName() != GET_MEMBER_NAME_STATIC(UVoxelNode_NoiseNode, NumberOfSamples));
}
#endif

void UVoxelNode_NoiseNode::PostLoad()
{
	Super::PostLoad();
	if (OutputRanges.Num() == 0)
	{
		OutputRanges = FVoxelNoiseComputeNode::ComputeOutputRanges(this, NumberOfSamples);
	}
}

void UVoxelNode_NoiseNode::PostInitProperties()
{
	Super::PostInitProperties();
	if (OutputRanges.Num() == 0 && !HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		OutputRanges = FVoxelNoiseComputeNode::ComputeOutputRanges(this, NumberOfSamples);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelNoiseComputeNode::FVoxelNoiseComputeNode(const UVoxelNode_NoiseNode& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelDataComputeNode(Node, CompilationNode)
	, Dimension(Node.GetDimension())
	, bIsDerivative(Node.IsDerivative())
	, bIsFractal(Node.IsFractal())
	, Interpolation(Node.Interpolation)
	, OutputRanges(Node.OutputRanges)
	, NoiseVariable(FVoxelVariable("FastNoise", UniqueName.ToString() + "_Noise"))
{
}

void FVoxelNoiseComputeNode::Init(Seed Inputs[], const FVoxelWorldGeneratorInit& InitStruct)
{
	PrivateNoise.SetSeed(Inputs[GetSeedInputIndex()]);
	InitNoise(PrivateNoise);
}

void FVoxelNoiseComputeNode::InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const
{
	Constructor.AddLine(NoiseVariable.Name + ".SetSeed(" + Inputs[GetSeedInputIndex()] + ");");
	InitNoiseCpp(NoiseVariable.Name, Constructor);
}

void FVoxelNoiseComputeNode::ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const
{
	for (int32 Index = 0; Index < OutputCount; Index++)
	{
		Outputs[Index].Get<v_flt>() = OutputRanges[Index];
	}
}

void FVoxelNoiseComputeNode::ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const
{
	const FString& X = Inputs[0];
	const FString& Y = Inputs[1];
	const FString& Z = Inputs[2];
	const FString& Freq = Dimension == 2 ? Inputs[2] : Inputs[3];

	FString Line;
	Line += Outputs[0] + " = ";
	Line += NoiseVariable.Name + "." + GetFunctionName();
	Line += "(" + X + ", " + Y;
	if (Dimension == 3)
	{
		Line += ", " + Z;
	}
	Line += ", " + Freq;
	if (bIsFractal)
	{
		Line += ", " + GET_OCTAVES_CPP;
	}
	if (bIsDerivative)
	{
		for (uint32 Index = 0; Index < Dimension; Index++)
		{
			Line += "," + Outputs[Index + 1];
		}
	}
	Line += ");";
	Constructor.AddLine(Line);
}

void FVoxelNoiseComputeNode::ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const
{
	for (int32 Index = 0; Index < OutputCount; Index++)
	{
		Constructor.AddLinef(TEXT("%s = { %ff, %ff };"), *Outputs[Index], OutputRanges[Index].Min, OutputRanges[Index].Max);
	}
}

void FVoxelNoiseComputeNode::SetupCpp(FVoxelCppConfig& Config) const
{
	Config.AddInclude("FastNoise.h");
}

void FVoxelNoiseComputeNode::GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const
{
	PrivateVariables.Add(NoiseVariable);
}

void FVoxelNoiseComputeNode::InitNoise(FastNoise& Noise) const
{
	Noise.SetInterp(FVoxelNoiseNodesEnums::GetFastNoiseEnum(Interpolation));
}

void FVoxelNoiseComputeNode::InitNoiseCpp(const FString& NoiseName, FVoxelCppConstructor& Constructor) const
{
	Constructor.AddLine(NoiseName + ".SetInterp(" + FVoxelNoiseNodesEnums::GetFastNoiseName(Interpolation) + ");");
}

TArray<FVoxelRange> FVoxelNoiseComputeNode::ComputeOutputRanges(UVoxelNode* Node, uint32 NumberOfSamples)
{
	const auto CompilationNode = Node->GetCompilationNode();
	auto ComputeNode = StaticCastSharedPtr<FVoxelNoiseComputeNode>(Node->GetComputeNode(*CompilationNode));
	const int32 Dimension = ComputeNode->Dimension;

	TArray<FVoxelRange> OutputRanges;

	ComputeNode->InitNoise(ComputeNode->PrivateNoise);

	v_flt Min[4];
	v_flt Max[4];
	FVoxelNodeType Inputs[4];
	FVoxelNodeType Outputs[4];
	for (int32 InputIndex = 0; InputIndex < Dimension; InputIndex++)
	{
		Inputs[InputIndex].Get<v_flt>() = 0;
	}
	Inputs[Dimension].Get<v_flt>() = 1;
	ComputeNode->Compute(Inputs, Outputs, FVoxelContext::EmptyContext);
	for (int32 OutputIndex = 0; OutputIndex < 4; OutputIndex++)
	{
		Min[OutputIndex] = Outputs[OutputIndex].Get<v_flt>();
		Max[OutputIndex] = Outputs[OutputIndex].Get<v_flt>();
	}

	for (uint32 Index = 0; Index < NumberOfSamples; Index++)
	{
		for (int32 InputIndex = 0; InputIndex < Dimension; InputIndex++)
		{
			Inputs[InputIndex].Get<v_flt>() = FMath::FRandRange(-NOISE_SAMPLE_RANGE, NOISE_SAMPLE_RANGE);
		}
		ComputeNode->Compute(Inputs, Outputs, FVoxelContext::EmptyContext);
		for (int32 OutputIndex = 0; OutputIndex < 4; OutputIndex++)
		{
			Min[OutputIndex] = FMath::Min(Min[OutputIndex], Outputs[OutputIndex].Get<v_flt>());
			Max[OutputIndex] = FMath::Max(Max[OutputIndex], Outputs[OutputIndex].Get<v_flt>());
		}
	}

	for (int32 OutputIndex = 0; OutputIndex < ComputeNode->OutputCount; OutputIndex++)
	{
		OutputRanges.Emplace(Min[OutputIndex], Max[OutputIndex]);
	}
	return OutputRanges;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FLinearColor UVoxelNode_NoiseNodeFractal::GetNodeBodyColor() const
{
	return FMath::Lerp<FLinearColor>(FColorList::White, FColorList::Orange, FMath::Clamp<float>((FractalOctaves - 1.f) / 10, 0, 1));
}

FLinearColor UVoxelNode_NoiseNodeFractal::GetColor() const
{
	return FMath::Lerp<FLinearColor>(FColorList::Black, FColorList::Orange, FMath::Clamp<float>((FractalOctaves - 1.f) / 10, 0, 1));
}

///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void UVoxelNode_NoiseNodeFractal::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		LODToOctavesMap.Add("0", FractalOctaves);
		int32 MaxInt = 0;
		for (auto& It : LODToOctavesMap)
		{
			if (!It.Key.IsEmpty())
			{
				const int32 Int = FMath::Clamp(TCString<TCHAR>::Atoi(*It.Key), 0, MAX_WORLD_DEPTH);
				MaxInt = FMath::Max(MaxInt, Int);
				It.Key = FString::FromInt(Int);
			}
		}
		for (auto& It : LODToOctavesMap)
		{
			if (It.Key.IsEmpty())
			{
				if (uint8 * LOD = LODToOctavesMap.Find(FString::FromInt(MaxInt)))
				{
					It.Value = *LOD - 1;
				}
				It.Key = FString::FromInt(++MaxInt);
			}
		}
		LODToOctavesMap.KeySort([](const FString& A, const FString& B) { return TCString<TCHAR>::Atoi(*A) < TCString<TCHAR>::Atoi(*B); });
	}
}
#endif

void UVoxelNode_NoiseNodeFractal::PostLoad()
{
	LODToOctavesMap.Add("0", FractalOctaves);
	Super::PostLoad(); // Make sure to call ComputeRange after
}

void UVoxelNode_NoiseNodeFractal::PostInitProperties()
{
	LODToOctavesMap.Add("0", FractalOctaves);
	Super::PostInitProperties(); // Make sure to call ComputeRange after
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelNoiseFractalComputeNode::FVoxelNoiseFractalComputeNode(const UVoxelNode_NoiseNodeFractal& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelNoiseComputeNode(Node, CompilationNode)
	, FractalOctaves(Node.FractalOctaves)
	, FractalLacunarity(Node.FractalLacunarity)
	, FractalGain(Node.FractalGain)
	, FractalType(Node.FractalType)
	, LODToOctavesVariable("TStaticArray<uint8, 32>", UniqueName.ToString() + "_LODToOctaves")
{
	TMap<uint8, uint8> Map;
	for (auto& It : Node.LODToOctavesMap)
	{
		if (!It.Key.IsEmpty())
		{
			Map.Add(TCString<TCHAR>::Atoi(*It.Key), It.Value);
		}
	}
	Map.Add(0, Node.FractalOctaves);

	for (int32 LODIt = 0; LODIt < 32; LODIt++)
	{
		int32 LOD = LODIt;
		while (!Map.Contains(LOD))
		{
			LOD--;
			check(LOD >= 0);
		}
		const_cast<TStaticArray<uint8, 32>&>(LODToOctaves)[LODIt] = Map[LOD];
	}
}

void FVoxelNoiseFractalComputeNode::InitNoise(FastNoise& Noise) const
{
	FVoxelNoiseComputeNode::InitNoise(Noise);

	Noise.SetFractalOctavesAndGain(FractalOctaves, FractalGain);
	Noise.SetFractalLacunarity(FractalLacunarity);
	Noise.SetFractalType(FVoxelNoiseNodesEnums::GetFastNoiseEnum(FractalType));
}

void FVoxelNoiseFractalComputeNode::InitNoiseCpp(const FString& NoiseName, FVoxelCppConstructor& Constructor) const
{
	FVoxelNoiseComputeNode::InitNoiseCpp(NoiseName, Constructor);

	Constructor.AddLine(NoiseName + ".SetFractalOctavesAndGain(" + FString::FromInt(FractalOctaves) + ", " + FString::SanitizeFloat(FractalGain) + ");");
	Constructor.AddLine(NoiseName + ".SetFractalLacunarity(" + FString::SanitizeFloat(FractalLacunarity) + ");");
	Constructor.AddLine(NoiseName + ".SetFractalType(" + FVoxelNoiseNodesEnums::GetFastNoiseName(FractalType) + ");");
}

void FVoxelNoiseFractalComputeNode::InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const
{
	FVoxelNoiseComputeNode::InitCpp(Inputs, Constructor);

	for (int32 LOD = 0; LOD < 32; LOD++)
	{
		Constructor.AddLinef(TEXT("%s[%d] = %u;"), *LODToOctavesVariable.Name, LOD, LODToOctaves[LOD]);
	}
}

void FVoxelNoiseFractalComputeNode::GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const
{
	FVoxelNoiseComputeNode::GetPrivateVariables(PrivateVariables);

	PrivateVariables.Add(LODToOctavesVariable);
}

void FVoxelNoiseFractalComputeNode::SetupCpp(FVoxelCppConfig& Config) const
{
	FVoxelNoiseComputeNode::SetupCpp(Config);

	Config.AddInclude("Containers/StaticArray.h");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelNode_IQNoiseBase::UVoxelNode_IQNoiseBase()
{
	bComputeDerivative = true;
	FractalType = EFractalType::FBM;
	FractalOctaves = 15;
	Frequency = 0.001;
	NumberOfSamples = 1000000;
}

#if WITH_EDITOR
bool UVoxelNode_IQNoiseBase::CanEditChange(const UProperty* InProperty) const
{
	return
		Super::CanEditChange(InProperty) &&
		InProperty->GetFName() != GET_MEMBER_NAME_STATIC(UVoxelNode_IQNoiseBase, bComputeDerivative) &&
		InProperty->GetFName() != GET_MEMBER_NAME_STATIC(UVoxelNode_IQNoiseBase, FractalType);
}
#endif

FVoxel2DIQNoiseComputeNode::FVoxel2DIQNoiseComputeNode(const UVoxelNode_2DIQNoiseBase& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelNoiseFractalComputeNode(Node, CompilationNode)
	, Rotation(Node.Rotation)
	, Matrix(FMatrix2x2(FQuat2D(FMath::DegreesToRadians(Node.Rotation))))
{
}

void FVoxel2DIQNoiseComputeNode::InitNoise(FastNoise& Noise) const
{
	FVoxelNoiseFractalComputeNode::InitNoise(Noise);
	Noise.SetMatrix(Matrix);
}

void FVoxel2DIQNoiseComputeNode::InitNoiseCpp(const FString& NoiseName, FVoxelCppConstructor& Constructor) const
{
	FVoxelNoiseFractalComputeNode::InitNoiseCpp(NoiseName, Constructor);
	Constructor.AddLinef(TEXT("%s.SetMatrix(FMatrix2x2(FQuat2D(FMath::DegreesToRadians(%f))));"), *NoiseName, Rotation);
}

FVoxel3DIQNoiseComputeNode::FVoxel3DIQNoiseComputeNode(const UVoxelNode_3DIQNoiseBase& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelNoiseFractalComputeNode(Node, CompilationNode)
	, Rotation(Node.Rotation)
	, Matrix(ToMatrix(Node.Rotation))
{
}

void FVoxel3DIQNoiseComputeNode::InitNoise(FastNoise& Noise) const
{
	FVoxelNoiseFractalComputeNode::InitNoise(Noise);
	Noise.SetMatrix(Matrix);
}

void FVoxel3DIQNoiseComputeNode::InitNoiseCpp(const FString& NoiseName, FVoxelCppConstructor& Constructor) const
{
	FVoxelNoiseFractalComputeNode::InitNoiseCpp(NoiseName, Constructor);
	Constructor.AddLinef(TEXT("%s.SetMatrix(ToMatrix(FRotator(%f, %f, %f)));"), *NoiseName, Rotation.Pitch, Rotation.Yaw, Rotation.Roll);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FVoxelCellularNoiseComputeNode::FVoxelCellularNoiseComputeNode(const UVoxelNode_CellularNoise& Node, const FVoxelCompilationNode& CompilationNode)
	: FVoxelNoiseComputeNode(Node, CompilationNode)
	, DistanceFunction(Node.DistanceFunction)
	, ReturnType(Node.ReturnType)
	, Jitter(Node.Jitter)
{
}

void FVoxelCellularNoiseComputeNode::InitNoise(FastNoise& Noise) const
{
	FVoxelNoiseComputeNode::InitNoise(Noise);

	Noise.SetCellularJitter(Jitter);
	Noise.SetCellularDistanceFunction(FVoxelNoiseNodesEnums::GetFastNoiseEnum(DistanceFunction));
	Noise.SetCellularReturnType(FVoxelNoiseNodesEnums::GetFastNoiseEnum(ReturnType));
}

void FVoxelCellularNoiseComputeNode::InitNoiseCpp(const FString& NoiseName, FVoxelCppConstructor& Constructor) const
{
	FVoxelNoiseComputeNode::InitNoiseCpp(NoiseName, Constructor);

	Constructor.AddLine(NoiseName + ".SetCellularJitter(" + FString::SanitizeFloat(Jitter) + ");");
	Constructor.AddLine(NoiseName + ".SetCellularDistanceFunction(" + FVoxelNoiseNodesEnums::GetFastNoiseName(DistanceFunction) + ");");
	Constructor.AddLine(NoiseName + ".SetCellularReturnType(" + FVoxelNoiseNodesEnums::GetFastNoiseName(ReturnType) + ");");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#if WITH_EDITOR
void UVoxelNode_NoiseNodeWithDerivative::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_STATIC(UVoxelNode_NoiseNodeWithDerivative, bComputeDerivative) &&
		PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive &&
		Graph &&
		GraphNode)
	{
		GraphNode->ReconstructNode();
		Graph->CompileVoxelNodesFromGraphNodes();
	}

	// Reconstruct first to have the right output count
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UVoxelNode_NoiseNodeWithDerivativeFractal::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_STATIC(UVoxelNode_NoiseNodeWithDerivative, bComputeDerivative) &&
		PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive &&
		Graph &&
		GraphNode)
	{
		GraphNode->ReconstructNode();
		Graph->CompileVoxelNodesFromGraphNodes();
	}

	// Reconstruct first to have the right output count
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif