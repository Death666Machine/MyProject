// Copyright 2020 Phyronnaz

#include "VoxelDataAssetEditorToolkit.h"
#include "VoxelEditorToolsPanel.h"
#include "DataAssetEditor/VoxelDataAssetEditorCommands.h"
#include "DataAssetEditor/VoxelDataAssetEditorManager.h"
#include "DataAssetEditor/SVoxelDataAssetEditorViewport.h"
#include "Details/VoxelWorldDetails.h"

#include "VoxelWorld.h"
#include "VoxelAssets/VoxelDataAsset.h"
#include "VoxelTools/VoxelAssetTools.h"

#include "IDetailsView.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "AdvancedPreviewSceneModule.h"
#include "AdvancedPreviewScene.h"
#include "PreviewScene.h"
#include "EngineUtils.h"
#include "EditorStyleSet.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Docking/SDockTab.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "Voxel"

const FName FVoxelDataAssetEditorToolkit::EditToolsTabId(TEXT("VoxelDataAssetEditor_EditTools"));
const FName FVoxelDataAssetEditorToolkit::PreviewSettingsTabId(TEXT("VoxelDataAssetEditor_PreviewSettings"));
const FName FVoxelDataAssetEditorToolkit::DetailsTabId(TEXT("VoxelDataAssetEditor_Details"));
const FName FVoxelDataAssetEditorToolkit::AdvancedPreviewSettingsTabId(TEXT("VoxelDataAssetEditor_AdvancedPreviewSettings"));
const FName FVoxelDataAssetEditorToolkit::PreviewTabId(TEXT("VoxelDataAssetEditor_Preview"));

class FVoxelAdvancedPreviewScene : public FAdvancedPreviewScene
{
public:
	FVoxelAdvancedPreviewScene()
		: FAdvancedPreviewScene(ConstructionValues())
	{
		if (SkyComponent)
		{
			SkyComponent->SetWorldScale3D(FVector(1000000));
		}
	}
};

FVoxelDataAssetEditorToolkit::FVoxelDataAssetEditorToolkit()
{
	PreviewScene = MakeShared<FVoxelAdvancedPreviewScene>();
	PreviewScene->SetFloorVisibility(false);
	PreviewScene->SetSkyCubemap(GUnrealEd->GetThumbnailManager()->AmbientCubemap);

	UWorld* PreviewWorld = PreviewScene->GetWorld();
	for (FActorIterator It(PreviewWorld); It; ++It)
	{
		It->DispatchBeginPlay();
	}
	PreviewWorld->bBegunPlay = true;
}

FVoxelDataAssetEditorToolkit::~FVoxelDataAssetEditorToolkit()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDataAssetEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_VoxelEditor", "Voxel Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(EditToolsTabId, FOnSpawnTab::CreateSP(this, &FVoxelDataAssetEditorToolkit::SpawnTab_EditTools))
		.SetDisplayName(LOCTEXT("EditToolsTab", "Edit Tools"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(PreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FVoxelDataAssetEditorToolkit::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSettingsTab", "Preview Settings"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FVoxelDataAssetEditorToolkit::SpawnTab_Details))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(AdvancedPreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FVoxelDataAssetEditorToolkit::SpawnTab_AdvancedPreviewSettings))
		.SetDisplayName(LOCTEXT("AdvancedPreviewSettingsTab", "Advanced Preview Settings"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(PreviewTabId, FOnSpawnTab::CreateSP(this, &FVoxelDataAssetEditorToolkit::SpawnTab_Preview))
		.SetDisplayName(LOCTEXT("PreviewTab", "Preview"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));
}

void FVoxelDataAssetEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(EditToolsTabId);
	InTabManager->UnregisterTabSpawner(PreviewSettingsTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(AdvancedPreviewSettingsTabId);
	InTabManager->UnregisterTabSpawner(PreviewTabId);
}

void FVoxelDataAssetEditorToolkit::InitVoxelEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit)
{
	DataAsset = CastChecked<UVoxelDataAsset>(ObjectToEdit);

	// Support undo/redo
	DataAsset->SetFlags(RF_Transactional);

	Manager = MakeUnique<FVoxelDataAssetEditorManager>(DataAsset, *PreviewScene);

	ToolsPanel = MakeShared<FVoxelEditorToolsPanel>();
	ToolsPanel->Init();

	FVoxelDataAssetEditorCommands::Register();

	BindCommands();
	CreateInternalWidgets();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_VoxelDataAssetEditor_Layout_v3")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->SetHideTabWell(true)
				->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)->SetSizeCoefficient(0.9f)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)->SetSizeCoefficient(0.2f)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(EditToolsTabId, ETabState::OpenedTab)
						->AddTab(PreviewSettingsTabId, ETabState::OpenedTab)
						->AddTab(AdvancedPreviewSettingsTabId, ETabState::ClosedTab)
					)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(DetailsTabId, ETabState::OpenedTab)
					)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.80f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.8f)
						->SetHideTabWell(true)
						->AddTab(PreviewTabId, ETabState::OpenedTab)
					)

				)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, TEXT("VoxelDataAssetEditorApp"), StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectToEdit, false);

	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDataAssetEditorToolkit::CreateInternalWidgets()
{
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	{
		FDetailsViewArgs Args;
		Args.bHideSelectionTip = true;
		Args.NotifyHook = nullptr;
		Args.bShowOptions = false;
		Args.bShowActorLabel = false;

		PreviewSettings = PropertyModule.CreateDetailView(Args);
		PreviewSettings->RegisterInstancedCustomPropertyLayout(
			AVoxelWorld::StaticClass(),
			FOnGetDetailCustomizationInstance::CreateStatic(&FVoxelWorldDetails::MakeDataAssetEditorInstance));
		PreviewSettings->SetObject(&Manager->GetVoxelWorld());
	}

	{
		FDetailsViewArgs Args;
		Args.bHideSelectionTip = true;
		Args.NotifyHook = this;
		Args.bShowOptions = false;
		Args.bShowActorLabel = false;
		Details = PropertyModule.CreateDetailView(Args);
		Details->SetObject(DataAsset);
	}

	FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>("AdvancedPreviewScene");
	AdvancedPreviewSettingsWidget = AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(PreviewScene.ToSharedRef());

	Preview = SNew(SVoxelDataAssetEditorViewport).Editor(this);
}

void FVoxelDataAssetEditorToolkit::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShared<FExtender>();

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateRaw(this, &FVoxelDataAssetEditorToolkit::FillToolbar)
	);

	AddToolbarExtender(ToolbarExtender);
}

void FVoxelDataAssetEditorToolkit::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	auto& Commands = FVoxelDataAssetEditorCommands::Get();

	ToolbarBuilder.BeginSection("Toolbar");
	ToolbarBuilder.AddToolBarButton(Commands.InvertDataAsset);
	ToolbarBuilder.EndSection();
}

void FVoxelDataAssetEditorToolkit::BindCommands()
{
	auto& Commands = FVoxelDataAssetEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.InvertDataAsset,
		FExecuteAction::CreateSP(this, &FVoxelDataAssetEditorToolkit::InvertDataAsset));
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDataAssetEditorToolkit::SaveAsset_Execute()
{
	FScopedSlowTask Progress(2, LOCTEXT("SavingAsset", "Saving asset"));
	Progress.MakeDialog(false, true);

	Progress.EnterProgressFrame();
	Manager->Save(true);

	Progress.EnterProgressFrame();
	FAssetEditorToolkit::SaveAsset_Execute();
}

bool FVoxelDataAssetEditorToolkit::OnRequestClose()
{
	if (Manager->IsDirty())
	{
		const auto Result = FMessageDialog::Open(
			EAppMsgType::YesNoCancel,
			EAppReturnType::Cancel,
			FText::Format(LOCTEXT("NeedToSave", "Voxel Data Asset {0}: \nSave your changes?"),
				FText::FromString(DataAsset->GetName())));
		if (Result == EAppReturnType::Yes)
		{
			SaveAsset_Execute();
			return true;
		}
		else if (Result == EAppReturnType::No)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		return true;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDataAssetEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(DataAsset);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void FVoxelDataAssetEditorToolkit::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive ||
		!ensure(PropertyChangedEvent.GetNumObjectsBeingEdited() == 1) ||
		!ensure(PropertyChangedEvent.MemberProperty) ||
		!ensure(PropertyChangedEvent.GetObjectBeingEdited(0) == DataAsset))
	{
		return;
	}

	const FName Name = PropertyChangedEvent.MemberProperty->GetFName();
	if (Name == GET_MEMBER_NAME_STATIC(UVoxelDataAsset, bSubtractiveAsset))
	{
		if (Manager->IsDirty())
		{
			const bool bNewSubtractiveAsset = DataAsset->bSubtractiveAsset;
			const bool bOldSubtractiveAsset = !bNewSubtractiveAsset;
			DataAsset->bSubtractiveAsset = bOldSubtractiveAsset;

			{
				FScopedSlowTask Progress(1, LOCTEXT("SavingAsset", "Saving asset"));
				Progress.MakeDialog(false, true);
				Progress.EnterProgressFrame();
				Manager->Save(false);
			}

			DataAsset->bSubtractiveAsset = bNewSubtractiveAsset;
		}
		Manager->RecreateWorld();
	}
	else if (Name == GET_MEMBER_NAME_STATIC(UVoxelDataAsset, PositionOffset))
	{
		// Save position offset, as it will be changed by Save
		const FIntVector PositionOffset = DataAsset->PositionOffset;

		if (Manager->IsDirty())
		{
			FScopedSlowTask Progress(1, LOCTEXT("SavingAsset", "Saving asset"));
			Progress.MakeDialog(false, true);
			Progress.EnterProgressFrame();
			Manager->Save(false);
		}

		DataAsset->PositionOffset = PositionOffset;
		Manager->RecreateWorld();
	}
	else
	{
		ensure(false);
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

FAdvancedPreviewScene& FVoxelDataAssetEditorToolkit::GetPreviewScene() const
{
	return *PreviewScene;
}

AVoxelWorld& FVoxelDataAssetEditorToolkit::GetVoxelWorld() const
{
	return Manager->GetVoxelWorld();
}

UVoxelDataAsset& FVoxelDataAssetEditorToolkit::GetDataAsset() const
{
	return *DataAsset;
}

FVoxelEditorToolsPanel& FVoxelDataAssetEditorToolkit::GetPanel() const
{
	return *ToolsPanel;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TSharedRef<SDockTab> FVoxelDataAssetEditorToolkit::SpawnTab_EditTools(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == EditToolsTabId);

	auto Tab =
		SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(LOCTEXT("VoxelEditToolsTitle", "Edit Tools"))
		[
			ToolsPanel->GetWidget().ToSharedRef()
		];
	return Tab;
}

TSharedRef<SDockTab> FVoxelDataAssetEditorToolkit::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewSettingsTabId);

	auto Tab =
		SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(LOCTEXT("VoxelPreviewSettingsTitle", "Preview Settings"))
		[
			PreviewSettings.ToSharedRef()
		];
	return Tab;
}

TSharedRef<SDockTab> FVoxelDataAssetEditorToolkit::SpawnTab_Details(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == DetailsTabId);

	auto Tab =
		SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(LOCTEXT("VoxelDetailsTitle", "Details"))
		[
			Details.ToSharedRef()
		];
	return Tab;
}

TSharedRef<SDockTab> FVoxelDataAssetEditorToolkit::SpawnTab_AdvancedPreviewSettings(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == AdvancedPreviewSettingsTabId);

	auto Tab =
		SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(LOCTEXT("VoxelAdvancedPreviewSettingsTitle", "Advanced Preview Settings"))
		[
			AdvancedPreviewSettingsWidget.ToSharedRef()
		];
	return Tab;
}

TSharedRef<SDockTab> FVoxelDataAssetEditorToolkit::SpawnTab_Preview(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewTabId);

	auto Tab =
		SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Viewports"))
		.Label(LOCTEXT("VoxelPreviewTitle", "Preview"))
		[
			Preview.ToSharedRef()
		];
	return Tab;
}

void FVoxelDataAssetEditorToolkit::InvertDataAsset()
{
	if (Manager->IsDirty())
	{
		FScopedSlowTask Progress(1, LOCTEXT("SavingAsset", "Saving asset"));
		Progress.MakeDialog(false, true);
		Progress.EnterProgressFrame();
		Manager->Save(false);
	}

	const auto NewData = DataAsset->MakeData();
	UVoxelAssetTools::InvertDataAssetImpl(*DataAsset->GetData(), *NewData);
	DataAsset->SetData(NewData);

	Manager->RecreateWorld();
}
#undef LOCTEXT_NAMESPACE