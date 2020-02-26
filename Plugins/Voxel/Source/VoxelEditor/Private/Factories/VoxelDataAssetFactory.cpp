// Copyright 2020 Phyronnaz

#include "Factories/VoxelDataAssetFactory.h"
#include "VoxelAssets/VoxelDataAsset.h"
#include "VoxelImporters/VoxelMeshImporter.h"
#include "Importers/MagicaVox.h"
#include "Importers/RawVox.h"
#include "VoxelMessages.h"

#include "Editor.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/StaticMesh.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Misc/ScopedSlowTask.h"

#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "Voxel"

UVoxelDataAssetFactory::UVoxelDataAssetFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = true;
	SupportedClass = UVoxelDataAsset::StaticClass();
}

UObject* UVoxelDataAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	auto* NewDataAsset = NewObject<UVoxelDataAsset>(InParent, Class, Name, Flags | RF_Transactional);

	auto Data = NewDataAsset->MakeData();
	Data->SetSize(FIntVector(1, 1, 3), false);
	Data->SetValue(0, 0, 0, FVoxelValue::Full());
	Data->SetValue(0, 0, 1, FVoxelValue::Empty());
	Data->SetValue(0, 0, 2, FVoxelValue::Full());
	NewDataAsset->SetData(Data);

	return NewDataAsset;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelDataAssetFromMeshImporterFactory::UVoxelDataAssetFromMeshImporterFactory()
{
	bEditAfterNew = true;
	bEditorImport = true;
	SupportedClass = UVoxelDataAsset::StaticClass();
}

UObject* UVoxelDataAssetFromMeshImporterFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	FScopedSlowTask Progress(2, LOCTEXT("ImportingFromMesh", "Importing from mesh"));
	Progress.MakeDialog(true);

	auto* NewDataAsset = NewObject<UVoxelDataAsset>(InParent, Class, Name, Flags | RF_Transactional);
	int32 NumLeaks = 0;
	Progress.EnterProgressFrame();
	auto Data = NewDataAsset->MakeData();

	FVoxelMeshImporterInputData InputData;
	UVoxelMeshImporterLibrary::CreateMeshDataFromStaticMesh(MeshImporter->StaticMesh, InputData);
	FTransform Transform = MeshImporter->GetTransform();
	Transform.SetTranslation(FVector::ZeroVector);
	FVoxelMeshImporterRenderTargetCache Cache;
	if (UVoxelMeshImporterLibrary::ConvertMeshToVoxels(MeshImporter, InputData, Transform, MeshImporter->Settings, Cache, *Data, NewDataAsset->PositionOffset, NumLeaks))
	{
		Progress.EnterProgressFrame();
		NewDataAsset->Source = EVoxelDataAssetImportSource::Mesh;
		NewDataAsset->SetData(Data);
		if (NumLeaks > 0)
		{
			FNotificationInfo Info = FNotificationInfo(FText::Format(
				NSLOCTEXT("Voxel", "ImportDone", "{0} leaks in the mesh (or bug in the voxelizer)"),
				FText::AsNumber(NumLeaks)));
			Info.ExpireDuration = 10;
			FSlateNotificationManager::Get().AddNotification(Info);
		}
		return NewDataAsset;
	}
	else
	{
		Progress.EnterProgressFrame();
		return nullptr;
	}
}

FString UVoxelDataAssetFromMeshImporterFactory::GetDefaultNewAssetName() const
{
	return MeshImporter->StaticMesh->GetName();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelDataAssetFromMagicaVoxFactory::UVoxelDataAssetFromMagicaVoxFactory()
{
	bEditorImport = true;
	SupportedClass = UVoxelDataAsset::StaticClass();
	Formats.Add(TEXT("vox;Magica Voxel Asset"));
}

bool UVoxelDataAssetFromMagicaVoxFactory::ConfigureProperties()
{
	// Load from default
	bUsePalette = GetDefault<UVoxelDataAssetFromMagicaVoxFactory>()->bUsePalette;
	Palette = GetDefault<UVoxelDataAssetFromMagicaVoxFactory>()->Palette;

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("ImportMagicaVox", "Import Magica Vox"))
		.SizingRule(ESizingRule::Autosized);

	bool bSuccess = false;

	auto OnOkClicked = FOnClicked::CreateLambda([&]()
	{
		bSuccess = true;
		PickerWindow->RequestDestroyWindow();
		return FReply::Handled();
	});
	auto OnCancelClicked = FOnClicked::CreateLambda([&]()
	{
		bSuccess = false;
		PickerWindow->RequestDestroyWindow();
		return FReply::Handled();
	});

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs(false, false, false, FDetailsViewArgs::HideNameArea);

	auto DetailsPanel = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
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
		[
			DetailsPanel
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
		if (bUsePalette && Palette.FilePath.IsEmpty())
		{
			return EVisibility::Hidden;
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
	GetMutableDefault<UVoxelDataAssetFromMagicaVoxFactory>()->bUsePalette = bUsePalette;
	GetMutableDefault<UVoxelDataAssetFromMagicaVoxFactory>()->Palette = Palette;

	return bSuccess;
}

bool UVoxelDataAssetFromMagicaVoxFactory::FactoryCanImport(const FString& Filename)
{
	return FPaths::GetExtension(Filename) == TEXT("vox");
}

UObject* UVoxelDataAssetFromMagicaVoxFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	auto NewDataAsset = NewObject<UVoxelDataAsset>(InParent, InName, Flags | RF_Transactional);
	auto Data = NewDataAsset->MakeData();
	if (MagicaVox::ImportToAsset(Filename, Palette.FilePath, bUsePalette, *Data))
	{
		NewDataAsset->Source = EVoxelDataAssetImportSource::MagicaVox;
		NewDataAsset->Paths = { Filename, bUsePalette ? Palette.FilePath : "" };
		NewDataAsset->SetData(Data);
		return NewDataAsset;
	}
	else
	{
		return nullptr;
	}
}

bool UVoxelDataAssetFromMagicaVoxFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (auto* Asset = Cast<UVoxelDataAsset>(Obj))
	{
		if (Asset->Source == EVoxelDataAssetImportSource::MagicaVox)
		{
			OutFilenames = Asset->Paths;
			// Else it will ask the user to choose a palette file
			OutFilenames.RemoveAll([](auto& S) { return S.IsEmpty(); });
			return true;
		}
	}
	return false;
}

void UVoxelDataAssetFromMagicaVoxFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (auto* Asset = Cast<UVoxelDataAsset>(Obj))
	{
		Asset->Paths = NewReimportPaths;
		if (NewReimportPaths.Num() == 1)
		{
			Asset->Paths.Add("");
		}
	}
}

EReimportResult::Type UVoxelDataAssetFromMagicaVoxFactory::Reimport(UObject* Obj)
{
	if (auto* Asset = Cast<UVoxelDataAsset>(Obj))
	{
		auto Data = Asset->MakeData();
		if (MagicaVox::ImportToAsset(Asset->Paths[0], Asset->Paths[1], !Asset->Paths[1].IsEmpty(), *Data))
		{
			Asset->SetData(Data);
			return EReimportResult::Succeeded;
		}
	}
	return EReimportResult::Failed;
}

int32 UVoxelDataAssetFromMagicaVoxFactory::GetPriority() const
{
	return ImportPriority;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

UVoxelDataAssetFromRawVoxFactory::UVoxelDataAssetFromRawVoxFactory()
{
	bEditorImport = true;
	SupportedClass = UVoxelDataAsset::StaticClass();
	Formats.Add(TEXT("rawvox;3D Coat RawVox"));
}

bool UVoxelDataAssetFromRawVoxFactory::FactoryCanImport(const FString& Filename)
{
	return FPaths::GetExtension(Filename) == TEXT("rawvox");
}

UObject* UVoxelDataAssetFromRawVoxFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
	auto NewDataAsset = NewObject<UVoxelDataAsset>(InParent, InName, Flags | RF_Transactional);
	auto Data = NewDataAsset->MakeData();
	if (RawVox::ImportToAsset(Filename, *Data))
	{
		NewDataAsset->Source = EVoxelDataAssetImportSource::RawVox;
		NewDataAsset->Paths = { Filename };
		NewDataAsset->SetData(Data);
		return NewDataAsset;
	}
	else
	{
		return nullptr;
	}
}

bool UVoxelDataAssetFromRawVoxFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	if (auto* Asset = Cast<UVoxelDataAsset>(Obj))
	{
		if (Asset->Source == EVoxelDataAssetImportSource::RawVox)
		{
			OutFilenames = Asset->Paths;
			return true;
		}
	}
	return false;
}

void UVoxelDataAssetFromRawVoxFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	if (auto* Asset = Cast<UVoxelDataAsset>(Obj))
	{
		Asset->Paths = NewReimportPaths;
	}
}

EReimportResult::Type UVoxelDataAssetFromRawVoxFactory::Reimport(UObject* Obj)
{
	if (auto* Asset = Cast<UVoxelDataAsset>(Obj))
	{
		auto Data = Asset->MakeData();
		if (RawVox::ImportToAsset(Asset->Paths[0], *Data))
		{
			Asset->SetData(Data);
			return EReimportResult::Succeeded;
		}
	}
	return EReimportResult::Failed;
}

int32 UVoxelDataAssetFromRawVoxFactory::GetPriority() const
{
	return ImportPriority;
}

#undef LOCTEXT_NAMESPACE