// Copyright 2020 Phyronnaz

#include "Factories/VoxelHeightmapAssetFactory.h"
#include "VoxelEditorDetailsUtilities.h"
#include "VoxelMessages.h"

#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"

#include "Editor.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"

#include "Modules/ModuleManager.h"
#include "Misc/ScopedSlowTask.h"

#include "LandscapeEditorModule.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"

#define LOCTEXT_NAMESPACE "Voxel"

namespace HeightmapHelpers
{
	template<typename TLandscapeMapFileFormat>
	inline const TLandscapeMapFileFormat* GetFormat(const TCHAR* Extension, ILandscapeEditorModule& LandscapeEditorModule);

	template<>
	inline const ILandscapeHeightmapFileFormat* GetFormat(const TCHAR* Extension, ILandscapeEditorModule& LandscapeEditorModule)
	{
		return LandscapeEditorModule.GetHeightmapFormatByExtension(Extension);
	}
	template<>
	inline const ILandscapeWeightmapFileFormat* GetFormat(const TCHAR* Extension, ILandscapeEditorModule& LandscapeEditorModule)
	{
		return LandscapeEditorModule.GetWeightmapFormatByExtension(Extension);
	}

	inline FLandscapeHeightmapInfo Validate(const TCHAR* Filename, const ILandscapeHeightmapFileFormat* Format)
	{
		return Format->Validate(Filename);
	}
	inline FLandscapeWeightmapInfo Validate(const TCHAR* Filename, const ILandscapeWeightmapFileFormat* Format)
	{
		return Format->Validate(Filename, "");
	}

	inline FLandscapeHeightmapImportData Import(const TCHAR* Filename, FLandscapeFileResolution ExpectedResolution, const ILandscapeHeightmapFileFormat* Format)
	{
		return Format->Import(Filename, ExpectedResolution);
	}
	inline FLandscapeWeightmapImportData Import(const TCHAR* Filename, FLandscapeFileResolution ExpectedResolution, const ILandscapeWeightmapFileFormat* Format)
	{
		return Format->Import(Filename, "", ExpectedResolution);
	}

	template<typename TLandscapeMapFileFormat, typename TLandscapeMapImportData>
	bool GetMap(const FString& Filename, int32& OutWidth, int32& OutHeight, TLandscapeMapImportData& OutMapImportData)
	{
		if (Filename.IsEmpty())
		{
			FVoxelEditorUtilities::ShowError(LOCTEXT("EmptyFilename", "Error: Empty filename!"));
			return false;
		}

		ILandscapeEditorModule& LandscapeEditorModule = FModuleManager::GetModuleChecked<ILandscapeEditorModule>("LandscapeEditor");
		FString Extension = FPaths::GetExtension(Filename, true);
		const TLandscapeMapFileFormat* Format = HeightmapHelpers::GetFormat<TLandscapeMapFileFormat>(*Extension, LandscapeEditorModule);

		if (!Format)
		{
			FVoxelEditorUtilities::ShowError(FText::FromString("Error: Unknown extension " + Extension));
			return false;
		}

		auto Info = HeightmapHelpers::Validate(*Filename, Format);
		switch (Info.ResultCode)
		{
		case ELandscapeImportResult::Success:
			break;
		case ELandscapeImportResult::Warning:
		{
			if (!FVoxelEditorUtilities::ShowWarning(Info.ErrorMessage))
			{
				return false;
			}
			break;
		}
		case ELandscapeImportResult::Error:
		{
			FVoxelEditorUtilities::ShowError(Info.ErrorMessage);
			return false;
		}
		default:
			check(false);
		}

		const int32 Index = Info.PossibleResolutions.Num() / 2;
		OutWidth = Info.PossibleResolutions[Index].Width;
		OutHeight = Info.PossibleResolutions[Index].Height;
		OutMapImportData = HeightmapHelpers::Import(*Filename, Info.PossibleResolutions[0], Format);

		switch (OutMapImportData.ResultCode)
		{
		case ELandscapeImportResult::Success:
			return true;
		case ELandscapeImportResult::Warning:
		{
			return FVoxelEditorUtilities::ShowWarning(OutMapImportData.ErrorMessage);
		}
		case ELandscapeImportResult::Error:
		{
			FVoxelEditorUtilities::ShowError(OutMapImportData.ErrorMessage);
			return false;
		}
		default:
			check(false);
			return false;
		}
	}

	bool GetHeightmap(const FString& Filename, int32& OutWidth, int32& OutHeight, FLandscapeHeightmapImportData& OutHeightmapImportData)
	{
		return HeightmapHelpers::GetMap<ILandscapeHeightmapFileFormat>(Filename, OutWidth, OutHeight, OutHeightmapImportData);
	}

	bool GetWeightmap(const FString& Filename, int32& OutWidth, int32& OutHeight, FLandscapeWeightmapImportData& OutWeightmapImportData)
	{
		return HeightmapHelpers::GetMap<ILandscapeWeightmapFileFormat>(Filename, OutWidth, OutHeight, OutWeightmapImportData);
	}
}

FVoxelMaterial FVoxelHeightmapImportersHelpers::ImportMaterialFromWeightmaps(EVoxelMaterialConfig MaterialConfig, const TArray<FWeightmap>& Weightmaps, uint32 Index)
{
	FVoxelMaterial Material(ForceInit);
	if (Weightmaps.Num() == 0)
	{
		return Material;
	}

	if (MaterialConfig == EVoxelMaterialConfig::RGB)
	{
		for (auto& Weightmap : Weightmaps)
		{
			uint8 Value = Weightmap.Data[Index];
			switch (Weightmap.Layer)
			{
			case EVoxelRGBA::R:
				Material.SetR(Value);
				break;
			case EVoxelRGBA::G:
				Material.SetG(Value);
				break;
			case EVoxelRGBA::B:
				Material.SetB(Value);
				break;
			case EVoxelRGBA::A:
				Material.SetA(Value);
				break;
			default:
				check(false);
			}
		}
	}
	else if (MaterialConfig == EVoxelMaterialConfig::SingleIndex)
	{
		uint8 MaxValue = Weightmaps[0].Data[Index];
		uint8 MaxIndex = Weightmaps[0].Index;
		for (int32 WeightmapIndex = 1; WeightmapIndex < Weightmaps.Num(); WeightmapIndex++)
		{
			auto& Weightmap = Weightmaps[WeightmapIndex];
			uint8 Value = Weightmap.Data[Index];
			if (Value > MaxValue)
			{
				MaxValue = Value;
				MaxIndex = Weightmap.Index;
			}
		}
		Material.SetSingleIndex_Index(MaxIndex);
	}
	else if (MaterialConfig == EVoxelMaterialConfig::DoubleIndex)
	{
		uint8 FirstMaxValue = Weightmaps[0].Data[Index];
		uint8 FirstMaxIndex = Weightmaps[0].Index;
		uint8 SecondMaxValue = FirstMaxValue;
		uint8 SecondMaxIndex = FirstMaxIndex;
		for (int32 WeightmapIndex = 1; WeightmapIndex < Weightmaps.Num(); WeightmapIndex++)
		{
			auto& Weightmap = Weightmaps[WeightmapIndex];
			uint8 Value = Weightmap.Data[Index];
			if (Value > FirstMaxValue)
			{
				SecondMaxValue = FirstMaxValue;
				SecondMaxIndex = FirstMaxIndex;
				FirstMaxValue = Value;
				FirstMaxIndex = Weightmap.Index;
			}
			else if (Value >= SecondMaxValue)
			{
				SecondMaxValue = Value;
				SecondMaxIndex = Weightmap.Index;
			}
		}
		Material.SetDoubleIndex_IndexA(FirstMaxIndex);
		Material.SetDoubleIndex_IndexB(SecondMaxIndex);
		const int32 Strength = ((255 - FirstMaxValue) + SecondMaxValue) / 2;
		checkVoxelSlow(0 <= Strength && Strength < 256);
		Material.SetDoubleIndex_Blend(Strength);
	}
	else
	{
		check(false);
	}

	return Material;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelHeightmapAssetFloatFactory::UVoxelHeightmapAssetFloatFactory()
{
	bCreateNew = false;
	bEditAfterNew = true;
	bEditorImport = true;
	SupportedClass = UVoxelHeightmapAssetFloat::StaticClass();
}

UObject* UVoxelHeightmapAssetFloatFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	auto* Asset = NewObject<UVoxelHeightmapAssetFloat>(InParent, Class, Name, Flags | RF_Transactional);

	int32 Width = 0;
	int32 Height = 0;
	for (auto& Component : Components)
	{
		Width = FMath::Max(Width, Component->SectionBaseX + Component->ComponentSizeQuads);
		Height = FMath::Max(Height, Component->SectionBaseY + Component->ComponentSizeQuads);
	}

	auto& Data = Asset->GetData();
	Data.SetSize(Width, Height, LayerInfos.Num() > 0);

	for (auto& Component : Components)
	{
		FLandscapeComponentDataInterface DataInterface(Component);

		if (Data.HasMaterials())
		{
			TArray<FVoxelHeightmapImportersHelpers::FWeightmap> Weightmaps;
			Weightmaps.SetNum(LayerInfos.Num());

			for (int32 Index = 0; Index < Weightmaps.Num(); Index++)
			{
				auto& Weightmap = Weightmaps[Index];
				auto& WeightmapInfo = LayerInfos[Index];
				DataInterface.GetWeightmapTextureData(WeightmapInfo.LayerInfo, Weightmap.Data);
				Weightmap.Layer = WeightmapInfo.Layer;
				Weightmap.Index = WeightmapInfo.Index;
			}
			Weightmaps.RemoveAll([&](auto& Weightmap) { return Weightmap.Data.Num() == 0; });

			const int32 WeightmapSize = (Component->SubsectionSizeQuads + 1) * Component->NumSubsections;

			for (int32 X = 0; X < Component->ComponentSizeQuads; X++)
			{
				for (int32 Y = 0; Y < Component->ComponentSizeQuads; Y++)
				{
					const int32 Index = (Component->SectionBaseX + X) + Width * (Component->SectionBaseY + Y);
					const int32 LocalIndex = X + WeightmapSize * Y;

					Data.SetMaterial(Index, FVoxelHeightmapImportersHelpers::ImportMaterialFromWeightmaps(MaterialConfig, Weightmaps, LocalIndex));
				}
			}
		}

		for (int32 X = 0; X < Component->ComponentSizeQuads; X++)
		{
			for (int32 Y = 0; Y < Component->ComponentSizeQuads; Y++)
			{
				const FVector Vertex = DataInterface.GetWorldVertex(X, Y);
				const FVector LocalVertex = (Vertex - ActorLocation) / Component->GetComponentTransform().GetScale3D();
				if (ensure(Data.IsValidIndex(LocalVertex.X, LocalVertex.Y)))
				{
					Data.SetHeight(LocalVertex.X, LocalVertex.Y, Vertex.Z);
				}
			}
		}
	}

	Asset->Save();

	return Asset;
}

FString UVoxelHeightmapAssetFloatFactory::GetDefaultNewAssetName() const
{
	return AssetName;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelHeightmapAssetUINT16Factory::UVoxelHeightmapAssetUINT16Factory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = true;
	SupportedClass = UVoxelHeightmapAssetUINT16::StaticClass();
}

bool UVoxelHeightmapAssetUINT16Factory::ConfigureProperties()
{
	// Load from default
	Heightmap = GetDefault<UVoxelHeightmapAssetUINT16Factory>()->Heightmap;
	MaterialConfig = GetDefault<UVoxelHeightmapAssetUINT16Factory>()->MaterialConfig;
	WeightmapsInfos = GetDefault<UVoxelHeightmapAssetUINT16Factory>()->WeightmapsInfos;

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("ImportHeightmap", "Import Heightmap"))
		.SizingRule(ESizingRule::Autosized);

	bool bSuccess = false;

	auto OnOkClicked = FOnClicked::CreateLambda([&]()
	{
		if (TryLoad())
		{
			bSuccess = true;
			PickerWindow->RequestDestroyWindow();
		}
		return FReply::Handled();
	});
	auto OnCancelClicked = FOnClicked::CreateLambda([&]()
	{
		bSuccess = false;
		PickerWindow->RequestDestroyWindow();
		return FReply::Handled();
	});

	class FVoxelHeightmapFactoryDetails : public IDetailCustomization
	{
	public:
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShared<FVoxelHeightmapFactoryDetails>();
		}

		FVoxelHeightmapFactoryDetails() = default;

	private:
		virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override
		{
			FSimpleDelegate RefreshDelegate = FSimpleDelegate::CreateLambda([&DetailLayout]()
			{
				DetailLayout.ForceRefreshDetails();
			});
			DetailLayout.GetProperty(GET_MEMBER_NAME_STATIC(UVoxelHeightmapAssetUINT16Factory, MaterialConfig))->SetOnPropertyValueChanged(RefreshDelegate);
		}
	};

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs(false, false, false, FDetailsViewArgs::HideNameArea);

	auto DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	FOnGetDetailCustomizationInstance LayoutDelegateDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FVoxelHeightmapFactoryDetails::MakeInstance);
	DetailsPanel->RegisterInstancedCustomPropertyLayout(UVoxelHeightmapAssetUINT16Factory::StaticClass(), LayoutDelegateDetails);
	DetailsPanel->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([&](const FPropertyAndParent& Property)
	{
		FName Name = Property.Property.GetFName();
		if (Name == GET_MEMBER_NAME_STATIC(FVoxelHeightmapImporterWeightmapInfos, Layer))
		{
			return MaterialConfig == EVoxelMaterialConfig::RGB;
		}
		else if (Name == GET_MEMBER_NAME_STATIC(FVoxelHeightmapImporterWeightmapInfos, Index))
		{
			return MaterialConfig == EVoxelMaterialConfig::SingleIndex || MaterialConfig == EVoxelMaterialConfig::DoubleIndex;
		}
		else
		{
			return true;
		}
	}));
	DetailsPanel->SetObject(this);

	auto Widget =
		SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			SNew(SBox)
			.Visibility(EVisibility::Visible)
		.WidthOverride(520.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(500)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
		[
			DetailsPanel
		]
		]
	+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Bottom)
		.Padding(8)
		[
			SNew(SUniformGridPanel)
			.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
		+ SUniformGridPanel::Slot(0, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("Create", "Create"))
		.HAlign(HAlign_Center)
		.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([&]()
	{
		if (Heightmap.FilePath.IsEmpty())
		{
			return EVisibility::Hidden;
		}
		for (auto& Weightmap : WeightmapsInfos)
		{
			if (Weightmap.File.FilePath.IsEmpty())
			{
				return EVisibility::Hidden;
			}
		}
		return EVisibility::Visible;
	})))
		.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
		.OnClicked(OnOkClicked)
		.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
		.TextStyle(FEditorStyle::Get(), "FlatButton.DefaultTextStyle")
		]
	+ SUniformGridPanel::Slot(1, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("Cancel", "Cancel"))
		.HAlign(HAlign_Center)
		.ContentPadding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
		.OnClicked(OnCancelClicked)
		.ButtonStyle(FEditorStyle::Get(), "FlatButton.Default")
		.TextStyle(FEditorStyle::Get(), "FlatButton.DefaultTextStyle")
		]
		]
		]
		];

	PickerWindow->SetContent(Widget);

	GEditor->EditorAddModalWindow(PickerWindow);

	// Save to default
	GetMutableDefault<UVoxelHeightmapAssetUINT16Factory>()->Heightmap = Heightmap;
	GetMutableDefault<UVoxelHeightmapAssetUINT16Factory>()->MaterialConfig = MaterialConfig;
	GetMutableDefault<UVoxelHeightmapAssetUINT16Factory>()->WeightmapsInfos = WeightmapsInfos;

	return bSuccess;
}

UObject* UVoxelHeightmapAssetUINT16Factory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	auto* Asset = NewObject<UVoxelHeightmapAssetUINT16>(InParent, Class, Name, Flags | RF_Transactional);
	if (DoImport(Asset))
	{
		return Asset;
	}
	else
	{
		return nullptr;
	}
}

FString UVoxelHeightmapAssetUINT16Factory::GetDefaultNewAssetName() const
{
	return FPaths::GetBaseFilename(Heightmap.FilePath);
}

bool UVoxelHeightmapAssetUINT16Factory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (auto* Asset = Cast<UVoxelHeightmapAssetUINT16>(Obj))
	{
		OutFilenames.Add(Asset->Heightmap);
		for (auto& Weightmap : Asset->WeightmapsInfos)
		{
			OutFilenames.Add(Weightmap.File.FilePath);
		}
		return true;
	}
	else
	{
		return false;
	}
}

void UVoxelHeightmapAssetUINT16Factory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (auto* Asset = Cast<UVoxelHeightmapAssetUINT16>(Obj))
	{
		for (int32 Index = 0; Index < NewReimportPaths.Num(); Index++)
		{
			if (Index == 0)
			{
				Asset->Heightmap = NewReimportPaths[0];
			}
			else if (ensure(Index - 1 < Asset->WeightmapsInfos.Num()))
			{
				Asset->WeightmapsInfos[Index - 1].File.FilePath = NewReimportPaths[Index];
			}
		}
	}
}

EReimportResult::Type UVoxelHeightmapAssetUINT16Factory::Reimport(UObject* Obj)
{
	if (auto* Asset = Cast<UVoxelHeightmapAssetUINT16>(Obj))
	{
		Heightmap.FilePath = Asset->Heightmap;
		MaterialConfig = Asset->MaterialConfig;
		WeightmapsInfos = Asset->WeightmapsInfos;
		if (!TryLoad())
		{
			return EReimportResult::Failed;
		}
		return DoImport(Asset) ? EReimportResult::Succeeded : EReimportResult::Cancelled;
	}
	else
	{
		return EReimportResult::Failed;
	}
}

int32 UVoxelHeightmapAssetUINT16Factory::GetPriority() const
{
	return ImportPriority;
}

bool UVoxelHeightmapAssetUINT16Factory::TryLoad()
{
	FScopedSlowTask Progress(1 + WeightmapsInfos.Num(), LOCTEXT("CreatingAsset", "Creating heightmap asset..."));
	Progress.MakeDialog();

	Progress.EnterProgressFrame(1, LOCTEXT("ProcessingHeightmap", "Processing heightmap"));
	if (!HeightmapHelpers::GetHeightmap(Heightmap.FilePath, Width, Height, HeightmapImportData))
	{
		return false;
	}

	Weightmaps.SetNum(WeightmapsInfos.Num());
	for (int32 Index = 0; Index < Weightmaps.Num(); Index++)
	{
		Progress.EnterProgressFrame(1, LOCTEXT("ProcessingWeightmaps", "Processing Weightmaps"));

		auto& Weightmap = Weightmaps[Index];
		auto& WeightmapInfo = WeightmapsInfos[Index];

		int32 WeightmapWidth;
		int32 WeightmapHeight;
		FLandscapeWeightmapImportData Result;
		if (!HeightmapHelpers::GetWeightmap(WeightmapInfo.File.FilePath, WeightmapWidth, WeightmapHeight, Result))
		{
			return false;
		}
		if (WeightmapWidth != Width || WeightmapHeight != Height)
		{
			FVoxelEditorUtilities::ShowError(FText::Format(LOCTEXT("InvalidResolution", "Weightmap resolution is not the same as Heightmap ({0})"), FText::FromString(WeightmapInfo.File.FilePath)));
			return false;
		}
		Weightmap.Data = MoveTemp(Result.Data);
		Weightmap.Layer = WeightmapInfo.Layer;
		Weightmap.Index = WeightmapInfo.Index;
	}

	return true;
}

bool UVoxelHeightmapAssetUINT16Factory::DoImport(UVoxelHeightmapAssetUINT16* Asset)
{
#define RETURN_IF_CANCEL() { if (Progress.ShouldCancel()) { FVoxelEditorUtilities::ShowError(LOCTEXT("Canceled", "Canceled!")); return false; } }

	FScopedSlowTask Progress(3.f, LOCTEXT("CreatingAsset", "Creating heightmap asset..."));
	Progress.MakeDialog(true, true);

	RETURN_IF_CANCEL();

	auto& Data = Asset->GetData();
	Data.SetSize(Width, Height, Weightmaps.Num() > 0);

	Progress.EnterProgressFrame(1.f, LOCTEXT("CopyingHeightmap", "Copying heightmap"));
	{
		const uint32 Total = Width * Height;
		FScopedSlowTask HeightmapProgress(Total);
		for (uint32 Index = 0; Index < Total; Index++)
		{
			if ((Index & 0x0000FFFF) == 0)
			{
				HeightmapProgress.EnterProgressFrame(FMath::Min<int32>(0x0000FFFF, Total - Index));
				RETURN_IF_CANCEL();
			}
			Data.SetHeight(Index, HeightmapImportData.Data[Index]);
		}
	}

	RETURN_IF_CANCEL();

	Progress.EnterProgressFrame(1.f, LOCTEXT("CopyingWeightmaps", "Copying weightmaps"));
	if (Data.HasMaterials())
	{
		FScopedSlowTask WeightmapProgress(Width * Height);
		for (uint32 Index = 0; Index < uint32(Width * Height); Index++)
		{
			if ((Index & 0x0000FFFF) == 0)
			{
				WeightmapProgress.EnterProgressFrame(0x0000FFFF);
				RETURN_IF_CANCEL();
			}
			Data.SetMaterial(Index, FVoxelHeightmapImportersHelpers::ImportMaterialFromWeightmaps(MaterialConfig, Weightmaps, Index));
		}
	}

	RETURN_IF_CANCEL();

	// Copy config
	Asset->Heightmap = Heightmap.FilePath;
	Asset->MaterialConfig = MaterialConfig;
	Asset->WeightmapsInfos = WeightmapsInfos;
	Asset->Weightmaps.Reset();
	for (auto& Weightmap : WeightmapsInfos)
	{
		FString Path;
		if (MaterialConfig == EVoxelMaterialConfig::RGB)
		{
			switch (Weightmap.Layer)
			{
			case EVoxelRGBA::R:
				Path += "Channel = R";
				break;
			case EVoxelRGBA::G:
				Path += "Channel = G";
				break;
			case EVoxelRGBA::B:
				Path += "Channel = B";
				break;
			case EVoxelRGBA::A:
				Path += "Channel = A";
				break;
			default:
				check(false);
				break;
			}
		}
		else
		{
			Path += "Index = " + FString::FromInt(Weightmap.Index);
		}
		Path += "; Path = " + Weightmap.File.FilePath;
		Asset->Weightmaps.Add(Path);
	}

	Progress.EnterProgressFrame(1.f, LOCTEXT("Compressing", "Compressing"));

	Asset->Save();

	return true;
#undef RETURN_IF_CANCEL
}

#undef LOCTEXT_NAMESPACE