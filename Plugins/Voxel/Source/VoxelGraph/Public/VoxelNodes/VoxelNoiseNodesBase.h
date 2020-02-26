// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "FastNoise.h"
#include "VoxelRange.h"
#include "VoxelNode.h"
#include "VoxelNodeHelper.h"
#include "VoxelNodeHelperMacros.h"
#include "VoxelNoiseNodesEnums.h"
#include "CppTranslation/VoxelVariables.h"
#include "Runtime/VoxelComputeNode.h"
#include "VoxelNoiseNodesBase.generated.h"

class FVoxelCppConstructor;
struct FVoxelWorldGeneratorInit;

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_NoiseNode : public UVoxelNodeWithContext // For LOD for octaves
{
	GENERATED_BODY()

public:
	// Do not use here, exposed as pin now
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Noise settings", meta = (DisplayName = "Old Frequency"))
		float Frequency = 0.02;

	UPROPERTY(EditAnywhere, Category = "Noise settings")
		EInterp Interpolation = EInterp::Quintic;

	// To find the output range, NumberOfSamples random samples are computed on start.
	// Increase this if the output range is too irregular, and if you start to see holes in your terrain
	// Increasing it will add a significant (async) cost in graphs
	// Free when compiled to C++
	UPROPERTY(EditAnywhere, Category = "Range analysis settings")
		uint32 NumberOfSamples = 100000;

	UPROPERTY(VisibleAnywhere, Category = "Range analysis settings")
		TArray<FVoxelRange> OutputRanges;

	//~ Begin UVoxelNode Interface
	virtual FName GetInputPinName(int32 PinIndex) const override;
	virtual FName GetOutputPinName(int32 PinIndex) const override;
	virtual FString GetInputPinToolTip(int32 PinIndex) const override;
	virtual FString GetOutputPinToolTip(int32 PinIndex) const override;
	virtual int32 GetMinInputPins() const override { return GetDimension() + 2; }
	virtual int32 GetMaxInputPins() const override { return GetDimension() + 2; }
	virtual int32 GetOutputPinsCount() const override { return IsDerivative() ? GetDimension() + 1 : 1; }
	virtual EVoxelPinCategory GetInputPinCategory(int32 PinIndex) const override { return PinIndex == GetDimension() + 1 ? EVoxelPinCategory::Seed : EVoxelPinCategory::Float; }
	virtual EVoxelPinCategory GetOutputPinCategory(int32 PinIndex) const override { return EVoxelPinCategory::Float; }
	virtual FString GetInputPinDefaultValue(int32 PinIndex) const override;
	//~ End UVoxelNode Interface

	//~ Begin UVoxelNode_NoiseNode Interface
	virtual uint32 GetDimension() const { unimplemented(); return 0; }
	virtual bool IsDerivative() const { return false; }
	virtual bool IsFractal() const { return false; }
	virtual bool NeedRangeAnalysis() const { return true; }
	//~ End UVoxelNode_NoiseNode Interface

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	//~ End UObject Interface
};

class FVoxelNoiseComputeNode : public FVoxelDataComputeNode
{
public:
	const uint32 Dimension;
	const bool bIsDerivative;
	const bool bIsFractal;

	FVoxelNoiseComputeNode(const UVoxelNode_NoiseNode& Node, const FVoxelCompilationNode& CompilationNode);

	void Init(Seed Inputs[], const FVoxelWorldGeneratorInit& InitStruct) override;
	void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override;

	void ComputeRange(const FVoxelNodeRangeType Inputs[], FVoxelNodeRangeType Outputs[], const FVoxelContextRange& Context) const override;
	void ComputeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override;
	void ComputeRangeCpp(const TArray<FString>& Inputs, const TArray<FString>& Outputs, FVoxelCppConstructor& Constructor) const override;

	void SetupCpp(FVoxelCppConfig& Config) const override;
	void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override;

	//~ Begin FVoxelNoiseComputeNode Interface
	virtual FString GetFunctionName() const { return ""; }
	virtual int32 GetSeedInputIndex() const { return Dimension + 1; }

	virtual void InitNoise(FastNoise& Noise) const;
	virtual void InitNoiseCpp(const FString& NoiseName, FVoxelCppConstructor& Constructor) const;
	//~ End FVoxelNoiseComputeNode Interface

protected:
	inline const FastNoise& GetNoise() const { return PrivateNoise; }
	inline const FString& GetNoiseName() const { return NoiseVariable.Name; }

private:
	const EInterp Interpolation;
	const TArray<TVoxelRange<v_flt>, TFixedAllocator<4>> OutputRanges;
	FVoxelVariable const NoiseVariable;
	FastNoise PrivateNoise;

public:
	static TArray<FVoxelRange> ComputeOutputRanges(UVoxelNode* Node, uint32 NumberOfSamples);
};

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_NoiseNodeFractal : public UVoxelNode_NoiseNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Fractal Noise settings", meta = (UIMin = 1, ClampMin = 1))
		int32 FractalOctaves = 3;

	// A multiplier that determines how quickly the frequency increases for each successive octave
	// The frequency of each successive octave is equal to the product of the previous octave's frequency and the lacunarity value.
	UPROPERTY(EditAnywhere, Category = "Fractal Noise settings")
		float FractalLacunarity = 2;

	// A multiplier that determines how quickly the amplitudes diminish for each successive octave
	// The amplitude of each successive octave is equal to the product of the previous octave's amplitude and the gain value. Increasing the gain produces "rougher" Perlin noise.
	UPROPERTY(EditAnywhere, Category = "Fractal Noise settings")
		float FractalGain = 0.5;

	UPROPERTY(EditAnywhere, Category = "Fractal Noise settings")
		EFractalType FractalType = EFractalType::FBM;

	// To use lower quality noise for far LODs
	UPROPERTY(EditAnywhere, Category = "LOD settings", meta = (DisplayName = "LOD to Octaves map"))
		TMap<FString, uint8> LODToOctavesMap;

	//~ Begin UVoxelNode Interface
	virtual FLinearColor GetNodeBodyColor() const override;
	virtual FLinearColor GetColor() const override;
	//~ End UVoxelNode Interface

	//~ Begin UVoxelNode_NoiseNode Interface
	virtual bool IsFractal() const override final { return true; }
	//~ End UVoxelNode_NoiseNode Interface

protected:
	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	//~ End UObject Interface
};

class FVoxelNoiseFractalComputeNode : public FVoxelNoiseComputeNode
{
public:
	FVoxelNoiseFractalComputeNode(const UVoxelNode_NoiseNodeFractal& Node, const FVoxelCompilationNode& CompilationNode);

	virtual void InitNoise(FastNoise& Noise) const override;
	virtual void InitNoiseCpp(const FString& NoiseName, FVoxelCppConstructor& Constructor) const override;

	virtual void InitCpp(const TArray<FString>& Inputs, FVoxelCppConstructor& Constructor) const override;
	virtual void GetPrivateVariables(TArray<FVoxelVariable>& PrivateVariables) const override;
	virtual void SetupCpp(FVoxelCppConfig& Config) const override;

	const uint8 FractalOctaves;
	const float FractalLacunarity;
	const float FractalGain;
	const EFractalType FractalType;
	const TStaticArray<uint8, 32> LODToOctaves;
	const FVoxelVariable LODToOctavesVariable;
};

#define GET_OCTAVES LODToOctaves[FMath::Clamp(Context.LOD, 0, 31)]
#define GET_OCTAVES_CPP static_cast<const FVoxelNoiseFractalComputeNode&>(*this).LODToOctavesVariable.Name + "[FMath::Clamp(" + FVoxelCppIds::Context + ".LOD, 0, 31)]"

//////////////////////////////////////////////////////////////////////////////////////

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_NoiseNodeWithDerivative : public UVoxelNode_NoiseNode
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Noise settings")
		bool bComputeDerivative = false;

	virtual bool IsDerivative() const override { return bComputeDerivative; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_NoiseNodeWithDerivativeFractal : public UVoxelNode_NoiseNodeFractal
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Noise settings")
		bool bComputeDerivative = false;

	virtual bool IsDerivative() const override { return bComputeDerivative; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};

//////////////////////////////////////////////////////////////////////////////////////

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_IQNoiseBase : public UVoxelNode_NoiseNodeWithDerivativeFractal
{
	GENERATED_BODY()

public:
	UVoxelNode_IQNoiseBase();

	//~ Begin UObject Interface
#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
#endif
	//~ End UObject Interface
};

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_2DIQNoiseBase : public UVoxelNode_IQNoiseBase
{
	GENERATED_BODY()

public:
	// Rotation (in degrees) applied to the position between each octave
	UPROPERTY(EditAnywhere, Category = "IQ Noise settings")
		float Rotation = 40;
};

class FVoxel2DIQNoiseComputeNode : public FVoxelNoiseFractalComputeNode
{
public:
	FVoxel2DIQNoiseComputeNode(const UVoxelNode_2DIQNoiseBase& Node, const FVoxelCompilationNode& CompilationNode);

	void InitNoise(FastNoise& Noise) const override;
	void InitNoiseCpp(const FString& NoiseName, FVoxelCppConstructor& Constructor) const override;

private:
	const float Rotation;
	const FMatrix2x2 Matrix;
};

UCLASS(Abstract)
class VOXELGRAPH_API UVoxelNode_3DIQNoiseBase : public UVoxelNode_IQNoiseBase
{
	GENERATED_BODY()

public:
	// Rotation (in degrees) applied to the position between each octave
	UPROPERTY(EditAnywhere, Category = "IQ Noise settings")
		FRotator Rotation = { 40, 45, 50 };
};

class FVoxel3DIQNoiseComputeNode : public FVoxelNoiseFractalComputeNode
{
public:
	FVoxel3DIQNoiseComputeNode(const UVoxelNode_3DIQNoiseBase& Node, const FVoxelCompilationNode& CompilationNode);

	void InitNoise(FastNoise& Noise) const override;
	void InitNoiseCpp(const FString& NoiseName, FVoxelCppConstructor& Constructor) const override;

private:
	const FRotator Rotation;
	const FMatrix Matrix;
};
