// Copyright 2020 Phyronnaz

using System.IO;
using UnrealBuildTool;

public class Embree3 : ModuleRules
{
    private string EmbreeDir
    {
        get { return Path.GetFullPath(Path.Combine(ModuleDirectory, "embree-3.5.2")) + "/"; }
    }

    public Embree3(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string SDKDir = EmbreeDir + "Win64/";
            string IncludeDir = SDKDir + "include/";
#if false
            string LibDir = SDKDir + "lib-debug/";
            if (!Directory.Exists(LibDir))
            {
                LibDir = SDKDir + "lib/";
            }
#else
            string LibDir = SDKDir + "lib/";
#endif

            PublicIncludePaths.Add(IncludeDir);

            RuntimeDependencies.Add(LibDir + "embree3.dll");
            RuntimeDependencies.Add(LibDir + "tbb.dll");
            RuntimeDependencies.Add(LibDir + "tbbmalloc.dll");
            PublicDelayLoadDLLs.Add("embree3.dll");
            PublicDelayLoadDLLs.Add("tbb.dll");
            PublicDelayLoadDLLs.Add("tbbmalloc.dll");

            PublicAdditionalLibraries.Add(LibDir + "embree3.lib");
            PublicAdditionalLibraries.Add(LibDir + "tbb.lib");
            PublicAdditionalLibraries.Add(LibDir + "tbbmalloc.lib");

            PublicDefinitions.Add("USE_EMBREE_VOXEL=1");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string SDKDir = EmbreeDir + "MacOSX/";
            string IncludeDir = SDKDir + "include/";
            string LibDir = SDKDir + "lib/";

            PublicIncludePaths.Add(IncludeDir);

            PublicAdditionalLibraries.Add(LibDir + "libembree3.3.dylib");
            PublicAdditionalLibraries.Add(LibDir + "libtbb.dylib");
            PublicAdditionalLibraries.Add(LibDir + "libtbbmalloc.dylib");

            PublicDefinitions.Add("USE_EMBREE_VOXEL=1");
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string SDKDir = EmbreeDir + "Linux/";
            string IncludeDir = SDKDir + "include/";
            string LibDir = SDKDir + "lib/";

            PublicIncludePaths.Add(IncludeDir);
#if UE_4_24_OR_LATER
            // TODO
            throw new System.ArgumentException("Embree Linux Error: Need to fix the code below for 4.24. Some tricky stuff was happening with UE removing the .3 at the end, so can't directly fix it");
#else
            /////////////////////////////////////////
            // The following are needed for linking:

            PublicLibraryPaths.Add(LibDir);

            PublicAdditionalLibraries.Add(":libembree3.so.3");
            PublicAdditionalLibraries.Add(":libtbb.so.2");
            PublicAdditionalLibraries.Add(":libtbbmalloc.so.2");

            //////////////////////////////////////////////////////
            // The following are needed for runtime dependencies:

            // Adds the library path to LD_LIBRARY_PATH
            PublicRuntimeLibraryPaths.Add(LibDir);

            // Tells UBT to copy the .so to the packaged game directory
            RuntimeDependencies.Add(LibDir + "libembree3.so.3");
            RuntimeDependencies.Add(LibDir + "libtbb.so.2");
            RuntimeDependencies.Add(LibDir + "libtbbmalloc.so.2");

            PublicDefinitions.Add("USE_EMBREE_VOXEL=1");
#endif
        }
        else
        {
            PublicDefinitions.Add("USE_EMBREE_VOXEL=0");
        }
    }
}