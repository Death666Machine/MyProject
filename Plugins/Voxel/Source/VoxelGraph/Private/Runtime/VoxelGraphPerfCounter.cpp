// Copyright 2020 Phyronnaz

#include "Runtime/VoxelGraphPerfCounter.h"

FCriticalSection FVoxelGraphPerfCounter::Section;
FVoxelGraphPerfCounter::FNodePerfTree FVoxelGraphPerfCounter::SingletonTree;

FCriticalSection FVoxelGraphRangeFailuresReporter::Section;
FVoxelGraphRangeFailuresReporter::FNodeErrorMap FVoxelGraphRangeFailuresReporter::SingletonNodes;