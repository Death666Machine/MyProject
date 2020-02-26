// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"

class UVoxelGraphGenerator;
class SWidget;

namespace FVoxelGraphCompileToCpp
{
	void Compile(const TSharedRef<SWidget>& Widget, UVoxelGraphGenerator* WorldGenerator, bool bIsAutomaticCompile);
}
