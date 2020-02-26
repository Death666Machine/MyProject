// Copyright 2020 Phyronnaz

#pragma once

#include "CoreMinimal.h"

namespace FVoxelCppUtils
{
	template<typename T>
	struct TIsStringType
	{
		static constexpr bool Value = false;
	};
	template<>
	struct TIsStringType<FName>
	{
		static constexpr bool Value = true;
	};
	template<>
	struct TIsStringType<FString>
	{
		static constexpr bool Value = true;
	};

	template<typename T>
	FString ArrayToString(const TArray<T>& Array)
	{
		FString Line = "{";
		for (auto& Name : Array)
		{
			Line += " ";
			if (TIsStringType<T>::Value) Line += "\"";
			Line += LexToString(Name);
			if (TIsStringType<T>::Value) Line += "\"";
			Line += ",";
		}
		Line += " }";
		return Line;
	}

	template<typename T>
	void CreateMapString(T& Constructor, const FString& MapName, const TArray<FName>& Keys, const TArray<FString>& Values, int32 ValuesOffset)
	{
		Constructor.StartBlock();
		for (int32 I = 0; I < Keys.Num(); I++)
		{
			Constructor.AddLinef(TEXT("static FName StaticName%d = \"%s\";"), I, *Keys[I].ToString());
			Constructor.AddLinef(TEXT("%s.Add(StaticName%d, %s);"), *MapName, I, *Values[ValuesOffset + I]);
		}
		Constructor.EndBlock();
	}

	template<typename T>
	void DeclareStaticName(T& Constructor, FName Name, const FString& StaticName = "StaticName")
	{
		Constructor.AddLinef(TEXT("static FName %s = \"%s\";"), *StaticName, *Name.ToString());
	}

	template<typename T>
	inline FString ClassString()
	{
		return FString::Printf(TEXT("%s%s"), T::StaticClass()->GetPrefixCPP(), *T::StaticClass()->GetName());
	}

	template<typename T>
	inline FString SoftObjectPtrString()
	{
		return FString::Printf(TEXT("TSoftObjectPtr<%s>"), *ClassString<T>());
	}
	template<typename T>
	inline FString SoftClassPtrString()
	{
		return FString::Printf(TEXT("TSoftClassPtr<%s>"), *ClassString<T>());
	}

	template<typename T>
	inline FString ObjectDefaultString(T* Object)
	{
		if (!Object)
		{
			return "";
		}
		return FString::Printf(TEXT("%s(FSoftObjectPath(\"%s\"))"), *SoftObjectPtrString<T>(), *Object->GetPathName());
	}
	template<typename T>
	inline FString ClassDefaultString(UClass* Class)
	{
		if (!Class)
		{
			return "";
		}
		return FString::Printf(TEXT("%s(FSoftObjectPath(\"%s\"))"), *SoftClassPtrString<T>(), *Class->GetPathName());
	}

	template<typename T>
	inline FString PickerDefaultString(const T& Picker)
	{
		const FString Struct = T::StaticStruct()->GetStructCPPName();
		if (Picker.IsClass())
		{
			return Struct + "(" + ClassDefaultString<typename T::WorldGeneratorType>(Picker.WorldGeneratorClass.LoadSynchronous()) + ")";
		}
		else
		{
			return Struct + "(" + ObjectDefaultString(Picker.WorldGeneratorObject.LoadSynchronous()) + ")";
		}
	}

	inline FString LoadObjectString(const FString& Name)
	{
		return Name + ".LoadSynchronous()";
	}
}
