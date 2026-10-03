#include "Floor101DocumentSubsystem.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "F101/Assets/AssetImport.h"
#include "F101/Assets/Library.h"
#include "F101/Cad/Furniture.h"
#include "F101/Exporters/Gltf.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "F101/Cad/ProjectFile.h"
#include "F101/Cad/Units.h"
#include "F101/Drawing/PlanDrawing.h"
#include "F101/Drawing/SvgWriter.h"
#include "F101/Geometry/Rooms.h"
#include "F101/Geometry/WallJoins.h"
#include "F101/Scene/SceneCompiler.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Floor101.h"
#include "Floor101FinishLibrary.h"
#include "Floor101Names.h"
#include "Floor101PathTracing.h"
#include "Floor101SceneNodeActor.h"
#include "Floor101SceneReconciler.h"
#include "Floor101SceneSpace.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "RHI.h"
#include "SFloor101PlanView.h"
#include "Slate/WidgetRenderer.h"
#include "TextureResource.h"

#include <filesystem>
#include <string_view>

#define LOCTEXT_NAMESPACE "Floor101Document"

namespace
{
	constexpr int64 ReferenceLengthMm = 3550;
	constexpr int64 ReferenceThicknessMm = 120;
	constexpr int64 ReferenceHeightMm = 2400;

	UFloor101DocumentSubsystem* FindSubsystem(UWorld* World)
	{
		UFloor101DocumentSubsystem* Subsystem = World ? World->GetSubsystem<UFloor101DocumentSubsystem>() : nullptr;
		if (!Subsystem)
		{
			UE_LOG(LogFloor101, Warning, TEXT("No 101st Floor document in this world"));
		}
		return Subsystem;
	}

	// UTF-8 in both directions, so non-ASCII paths (e.g. a user name with accents) survive on Windows.
	std::filesystem::path ToFilesystemPath(const FString& Path)
	{
		const FTCHARToUTF8 Utf8(*Path);
		return std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(Utf8.Get()), Utf8.Length()));
	}

	FText ToText(const std::string& Utf8)
	{
		return FText::FromString(UTF8_TO_TCHAR(Utf8.c_str()));
	}

	// The plan's words and numbers in the UI's language (ADR-0015).
	f101::drawing::DrawingTexts PlanTexts()
	{
		f101::drawing::DrawingTexts Texts;
		Texts.roomName = TCHAR_TO_UTF8(*LOCTEXT("PlanRoomName", "Habitación {n}").ToString());
		Texts.areaUnit = TCHAR_TO_UTF8(*LOCTEXT("PlanAreaUnit", " m²").ToString());
		Texts.decimalSeparator = ',';
		return Texts;
	}

	std::string ToUtf8(const FText& Text)
	{
		return std::string(TCHAR_TO_UTF8(*Text.ToString()));
	}

	// The names Twinmotion shows (ADR-0020), in the UI's language.
	f101::exporters::GltfTexts ExportTexts()
	{
		f101::exporters::GltfTexts Texts;
		Texts.groundFloor = ToUtf8(LOCTEXT("GltfGroundFloor", "Planta baja"));
		Texts.levelNumber = ToUtf8(LOCTEXT("GltfLevelNumber", "Planta {n}"));
		Texts.walls = ToUtf8(LOCTEXT("GltfWalls", "Muros"));
		Texts.floors = ToUtf8(LOCTEXT("GltfFloors", "Suelos"));
		Texts.slabs = ToUtf8(LOCTEXT("GltfSlabs", "Forjados"));
		Texts.openings = ToUtf8(LOCTEXT("GltfOpenings", "Puertas y ventanas"));
		Texts.furniture = ToUtf8(LOCTEXT("GltfFurniture", "Muebles"));
		Texts.wall = ToUtf8(LOCTEXT("GltfWall", "Muro"));
		Texts.joint = ToUtf8(LOCTEXT("GltfJoint", "Unión"));
		Texts.floor = ToUtf8(LOCTEXT("GltfFloor", "Suelo"));
		Texts.slab = ToUtf8(LOCTEXT("GltfSlab", "Forjado"));
		Texts.door = ToUtf8(LOCTEXT("GltfDoor", "Puerta"));
		Texts.window = ToUtf8(LOCTEXT("GltfWindow", "Ventana"));
		Texts.columns = ToUtf8(LOCTEXT("GltfColumns", "Pilares"));
		Texts.services = ToUtf8(LOCTEXT("GltfServices", "Instalaciones"));
		Texts.column = ToUtf8(LOCTEXT("GltfColumn", "Pilar"));
		Texts.electrical = ToUtf8(Floor101::ServiceKindName(f101::cad::ServiceKind::Electrical));
		Texts.water = ToUtf8(Floor101::ServiceKindName(f101::cad::ServiceKind::Water));
		Texts.sewage = ToUtf8(Floor101::ServiceKindName(f101::cad::ServiceKind::Sewage));
		Texts.gas = ToUtf8(Floor101::ServiceKindName(f101::cad::ServiceKind::Gas));
		for (const f101::cad::FinishInfo& Finish : f101::cad::finishCatalogue())
		{
			Texts.finishNames.emplace(std::string(Finish.id), ToUtf8(Floor101::FinishName(Finish.id)));
		}
		for (const f101::cad::FurnitureType& Type : f101::cad::furnitureCatalogue())
		{
			Texts.furnitureNames.emplace(std::string(Type.id), ToUtf8(Floor101::FurnitureName(Type.id)));
		}
		return Texts;
	}

	constexpr double PlanMarginMm = 1000.0;
	constexpr double PlanPngPixelsPerMm = 0.2; // 5 mm per pixel: 1:100 at about 127 dpi
	constexpr double PlanPngMaxPixels = 4096.0;

	// The file named on the command line, else the current one, else the default one.
	FString ResolveProjectPath(const TArray<FString>& Args, const UFloor101DocumentSubsystem& Subsystem)
	{
		if (!Args.IsEmpty())
		{
			return FPaths::ConvertRelativePathToFull(FString::Join(Args, TEXT(" ")));
		}
		return Subsystem.GetProjectPath().IsEmpty() ? UFloor101DocumentSubsystem::GetDefaultProjectPath() : Subsystem.GetProjectPath();
	}

	// Development commands; they act on the world of the console that runs them (editor, PIE or game).
	FAutoConsoleCommandWithWorld LoadCommand(
		TEXT("f101.Demo.Load"),
		TEXT("Creates the MVP 0 reference wall: (0,0)->(3550,0) mm, 120 mm thick, 2400 mm high."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->LoadReferenceWall();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SetLengthCommand(
		TEXT("f101.Demo.SetLength"),
		TEXT("Sets the reference wall length. Accepts 101st Floor input: 420, 420 cm, 4.2 m, 4,2 m, 4200 mm (no unit = cm)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World);
			if (!Subsystem)
			{
				return;
			}
			const FString Input = FString::Join(Args, TEXT(" "));
			const f101::Result<std::int64_t> Length = f101::cad::parseLength(TCHAR_TO_UTF8(*Input));
			if (!Length)
			{
				UE_LOG(LogFloor101, Warning, TEXT("f101.Demo.SetLength '%s': %s"), *Input, UTF8_TO_TCHAR(Length.error().message.c_str()));
				return;
			}
			Subsystem->SetReferenceWallLength(Length.value());
		}));

	FAutoConsoleCommandWithWorld ClearCommand(
		TEXT("f101.Demo.Clear"),
		TEXT("Removes the reference wall."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->ClearReferenceWall();
			}
		}));

	FAutoConsoleCommandWithWorld StatusCommand(
		TEXT("f101.Demo.Status"),
		TEXT("Logs the walls of the document and the Unreal actors that represent them."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (const UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				Subsystem->LogStatus();
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs SaveCommand(
		TEXT("f101.Project.Save"),
		TEXT("Saves the document as project.json: to the given path, else the current file, else Documents/101st Floor/project.json."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				(void)Subsystem->SaveProject(ResolveProjectPath(Args, *Subsystem));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs OpenCommand(
		TEXT("f101.Project.Open"),
		TEXT("Opens a project.json: the given path, else the current file, else Documents/101st Floor/project.json."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				(void)Subsystem->OpenProject(ResolveProjectPath(Args, *Subsystem));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ImportAssetCommand(
		TEXT("f101.Asset.Import"),
		TEXT("Imports a texture (.png, .jpg) or a model (.glb, .gltf, .obj) into the project's folder and its asset library (ADR-0017)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World);
			if (Subsystem && !Args.IsEmpty())
			{
				(void)Subsystem->ImportAsset(FString::Join(Args, TEXT(" ")).TrimQuotes());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ExportPlanCommand(
		TEXT("f101.Plan.Export"),
		TEXT("Exports the edited level's plan (ADR-0015) as .svg or .png: to the given path, else next to the project file as <name>-<level>.svg."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				const FString Path = Args.IsEmpty() ? Subsystem->GetDefaultPlanPath(TEXT("svg"))
					: FPaths::ConvertRelativePathToFull(FString::Join(Args, TEXT(" ")));
				(void)Subsystem->ExportPlan(Path, f101::cad::LevelId());
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs ExportGltfCommand(
		TEXT("f101.Export.Gltf"),
		TEXT("Exports the whole project for Twinmotion (ADR-0020) as .glb: to the given path, else next to the project file as <name>.glb."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				const FString Path = Args.IsEmpty() ? Subsystem->GetDefaultGltfPath()
					: FPaths::ConvertRelativePathToFull(FString::Join(Args, TEXT(" ")).TrimQuotes());
				(void)Subsystem->ExportGltf(Path);
			}
		}));

	FAutoConsoleCommandWithWorld UndoCommand(
		TEXT("f101.Undo"),
		TEXT("Undoes the last change of the document."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				(void)Subsystem->Edit([](f101::cad::EditController& Editor) { return Editor.undo(); });
			}
		}));

	FAutoConsoleCommandWithWorld RedoCommand(
		TEXT("f101.Redo"),
		TEXT("Redoes the last undone change of the document."),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UFloor101DocumentSubsystem* Subsystem = FindSubsystem(World))
			{
				(void)Subsystem->Edit([](f101::cad::EditController& Editor) { return Editor.redo(); });
			}
		}));
}

UFloor101DocumentSubsystem::UFloor101DocumentSubsystem() = default;
UFloor101DocumentSubsystem::~UFloor101DocumentSubsystem() = default;

bool UFloor101DocumentSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Editor);
}

void UFloor101DocumentSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Document = MakePimpl<f101::cad::Document>(f101::cad::makeRandomUuidSource());
	Editor = MakePimpl<f101::cad::EditController>(*Document, f101::cad::EditSettings{});
	Editor->setRoomLocator(f101::geometry::makeRoomLocator()); // rooms are picked by their floor (ADR-0012)
	Document->setWallObstructions(f101::geometry::makeWallObstructions()); // openings clear of joined walls (ADR-0013)
	Reconciler = MakePimpl<FFloor101SceneReconciler>(GetWorld());
	PlanDrawing = MakePimpl<f101::drawing::PlanDrawing>();
	Models = MakePimpl<f101::assets::ModelCache>();
	ListenerHandle = Document->subscribe([this](const f101::cad::ChangeSet&)
	{
		++ChangeCount;
		if (!bInEdit)
		{
			Resync(true); // changed outside an edit (console, MCP toolset, file): show it now
		}
	});
}

void UFloor101DocumentSubsystem::Deinitialize()
{
	if (Document)
	{
		Document->unsubscribe(ListenerHandle);
	}
	// Actors die with the world; the reconciler only drops its weak references.
	Reconciler.Reset();
	Editor.Reset();
	Document.Reset();
	Super::Deinitialize();
}

void UFloor101DocumentSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// The map's own meshes (the template's ground plane and sky sphere) cast no shadow: their shadows fall where nothing
	// is seen, and as huge non-Nanite meshes they cover hundreds of virtual shadow map pages each (r.Shadow.Virtual.
	// NonNanite.NumPageAreaDiagSlots), which overflows VSM's marking queue ("[VSM] Non-Nanite Marking Job Queue
	// overflow"). The image does not change. The project's own actors are not static mesh actors.
	for (TActorIterator<AStaticMeshActor> It(&InWorld); It; ++It)
	{
		if (UStaticMeshComponent* Component = It->GetStaticMeshComponent())
		{
			Component->SetCastShadow(false);
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("F101Demo")))
	{
		LoadReferenceWall();
	}
}

f101::Status UFloor101DocumentSubsystem::Edit(TFunctionRef<f101::Status(f101::cad::EditController&)> Action)
{
	// One resync once the action is over: while it runs, the controller's view state is not final (e.g. the wall
	// just created is still the tool's preview), so a resync from the document listener would show both.
	const uint64 ChangesBefore = ChangeCount;
	bInEdit = true;
	const f101::Status Result = Action(*Editor);
	bInEdit = false;
	if (Result)
	{
		SetStatus(FText::GetEmpty(), false);
	}
	else
	{
		SetStatus(ToText(Result.error().message), true);
	}
	Resync(ChangeCount != ChangesBefore);
	return Result;
}

void UFloor101DocumentSubsystem::UpdatePointer(const f101::cad::Ray3& Ray)
{
	Editor->pointerMoved(Ray);
	Resync(false);
}

void UFloor101DocumentSubsystem::SetStatus(const FText& Message, bool bIsError)
{
	StatusMessage = Message;
	bStatusIsError = bIsError;
	if (bIsError)
	{
		UE_LOG(LogFloor101, Warning, TEXT("F101: %s"), *Message.ToString());
	}
	else if (!Message.IsEmpty())
	{
		UE_LOG(LogFloor101, Display, TEXT("F101: %s"), *Message.ToString());
	}
}

FString UFloor101DocumentSubsystem::GetDefaultProjectPath()
{
	return FPaths::Combine(FPlatformProcess::UserDir(), TEXT("101st Floor"), TEXT("project.json"));
}

f101::Status UFloor101DocumentSubsystem::SaveProject(const FString& Path)
{
	const FString Directory = FPaths::GetPath(Path);
	if (!Directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*Directory, true))
	{
		const f101::Error Failed{f101::ErrorCode::NotFound, "Cannot create the folder " + std::string(TCHAR_TO_UTF8(*Directory))};
		SetStatus(ToText(Failed.message), true);
		return Failed;
	}
	// Imported assets go with the project (ADR-0017): saved somewhere else, their folders are copied there first.
	if (const f101::Status Copied = f101::assets::copyAssetFiles(Document->project(), ToFilesystemPath(GetProjectFolder()),
			ToFilesystemPath(Directory.IsEmpty() ? FString(TEXT(".")) : Directory));
		!Copied)
	{
		SetStatus(ToText(Copied.error().message), true);
		return Copied;
	}
	const f101::Status Saved = f101::cad::saveProjectFile(ToFilesystemPath(Path), Document->project());
	if (!Saved)
	{
		SetStatus(ToText(Saved.error().message), true);
		return Saved;
	}
	ProjectPath = Path;
	SavedChangeCount = ChangeCount;
	SetStatus(FText::Format(LOCTEXT("Saved", "Guardado: {0}"), FText::FromString(Path)), false);
	return Saved;
}

f101::Status UFloor101DocumentSubsystem::OpenProject(const FString& Path)
{
	f101::Result<f101::cad::Project> Loaded = f101::cad::loadProjectFile(ToFilesystemPath(Path));
	if (!Loaded)
	{
		SetStatus(ToText(Loaded.error().message), true);
		return Loaded.error();
	}
	Editor->reset();
	// Replacing the project rebuilds the scene, which reads the assets' files: from the folder of the one opened.
	const FString PreviousPath = ProjectPath;
	ProjectPath = Path;
	const f101::Status Replaced = Document->replaceProject(std::move(Loaded).value());
	if (!Replaced)
	{
		ProjectPath = PreviousPath;
		SetStatus(ToText(Replaced.error().message), true);
		return Replaced;
	}
	SavedChangeCount = ChangeCount;
	SetStatus(FText::Format(LOCTEXT("Opened", "Abierto: {0}"), FText::FromString(Path)), false);
	return Replaced;
}

FString UFloor101DocumentSubsystem::GetProjectFolder() const
{
	return FPaths::GetPath(ProjectPath.IsEmpty() ? GetDefaultProjectPath() : ProjectPath);
}

FString UFloor101DocumentSubsystem::GetAssetFilePath(const f101::cad::Asset& Asset) const
{
	return FPaths::Combine(GetProjectFolder(), UTF8_TO_TCHAR(Asset.file.c_str()));
}

f101::Result<f101::cad::AssetId> UFloor101DocumentSubsystem::ImportAsset(const FString& SourcePath)
{
	const f101::cad::AssetId Id{f101::cad::makeRandomUuidSource()()};
	const f101::Result<f101::assets::ImportedAsset> Imported =
		f101::assets::importAsset(ToFilesystemPath(SourcePath), ToFilesystemPath(GetProjectFolder()), Id);
	if (!Imported)
	{
		SetStatus(ToText(Imported.error().message), true);
		return Imported.error();
	}
	const f101::Result<f101::cad::Transaction> Added = Document->execute(f101::cad::AddAssetCommand{Imported.value().asset});
	if (!Added)
	{
		SetStatus(ToText(Added.error().message), true);
		return Added.error();
	}
	const FText Name = ToText(Imported.value().asset.name);
	const std::vector<std::string>& Warnings = Imported.value().warnings;
	SetStatus(Warnings.empty() ? FText::Format(LOCTEXT("Imported", "Importado: {0}"), Name)
		: FText::Format(LOCTEXT("ImportedWarning", "Importado: {0} · aviso: {1}"), Name, ToText(Warnings.front())), false);
	for (const std::string& Warning : Warnings)
	{
		UE_LOG(LogFloor101, Warning, TEXT("F101: %s: %s"), *Name.ToString(), UTF8_TO_TCHAR(Warning.c_str()));
	}
	return Id;
}

f101::Status UFloor101DocumentSubsystem::ChangeAsset(const f101::cad::Asset& Asset)
{
	f101::cad::Asset Changed = Asset;
	const f101::cad::Asset* Current = Document->project().findAsset(Asset.id);
	if (Current && Asset.kind == f101::cad::AssetKind::Model &&
		(Asset.model.unit != Current->model.unit || Asset.model.up != Current->model.up ||
			Asset.model.frontQuarterTurns != Current->model.frontQuarterTurns))
	{
		const f101::Result<f101::cad::ModelAsset> Measured =
			f101::assets::measureModel(ToFilesystemPath(GetProjectFolder()), *Current, Asset.model);
		if (!Measured)
		{
			SetStatus(ToText(Measured.error().message), true);
			return Measured.error();
		}
		Changed.model = Measured.value();
	}
	const f101::Result<f101::cad::Transaction> Applied = Document->execute(f101::cad::ChangeAssetCommand{Changed});
	if (!Applied)
	{
		SetStatus(ToText(Applied.error().message), true);
		return Applied.error();
	}
	return f101::Status::success();
}

f101::Status UFloor101DocumentSubsystem::RemoveAsset(const f101::cad::AssetId& Id)
{
	const f101::Result<f101::cad::Transaction> Removed = Document->execute(f101::cad::DeleteAssetCommand{Id});
	if (!Removed)
	{
		SetStatus(ToText(Removed.error().message), true);
		return Removed.error();
	}
	return f101::Status::success();
}

FString UFloor101DocumentSubsystem::GetAssetProblem(const f101::cad::AssetId& Id) const
{
	return UTF8_TO_TCHAR(Models->error(Id).c_str());
}

FString UFloor101DocumentSubsystem::GetLibraryFolder()
{
	return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Library")));
}

const f101::assets::Library& UFloor101DocumentSubsystem::GetLibrary()
{
	static const f101::assets::Library Library = []()
	{
		f101::Result<f101::assets::Library> Loaded = f101::assets::loadLibrary(ToFilesystemPath(GetLibraryFolder()));
		if (!Loaded)
		{
			UE_LOG(LogFloor101, Error, TEXT("F101: the library in %s cannot be read: %s"), *GetLibraryFolder(),
				UTF8_TO_TCHAR(Loaded.error().message.c_str()));
			return f101::assets::Library{};
		}
		f101::assets::Library Read = std::move(Loaded).value();
		UE_LOG(LogFloor101, Log, TEXT("F101: library: %d pieces of furniture and %d textures"), static_cast<int32>(Read.furniture.size()),
			static_cast<int32>(Read.textures.size()));
		return Read;
	}();
	return Library;
}

f101::Result<f101::cad::AssetId> UFloor101DocumentSubsystem::AddFromLibrary(const FString& ItemId)
{
	const std::string Id = TCHAR_TO_UTF8(*ItemId);
	if (const f101::cad::Asset* Added = f101::assets::findLibraryAsset(Document->project(), Id))
	{
		return Added->id;
	}
	const f101::assets::Library& Library = GetLibrary();
	const f101::assets::LibraryFurniture* Furniture = Library.findFurniture(Id);
	const f101::assets::LibraryTexture* Texture = Furniture ? nullptr : Library.findTexture(Id);
	if (!Furniture && !Texture)
	{
		const f101::Error Missing{f101::ErrorCode::NotFound, "The library has no item '" + Id + "'"};
		SetStatus(ToText(Missing.message), true);
		return Missing;
	}
	const f101::cad::AssetId AssetId{f101::cad::makeRandomUuidSource()()};
	const std::filesystem::path LibraryFolder = ToFilesystemPath(GetLibraryFolder());
	const std::filesystem::path ProjectFolder = ToFilesystemPath(GetProjectFolder());
	const f101::Result<f101::assets::ImportedAsset> Imported = Furniture
		? f101::assets::importLibraryFurniture(*Furniture, LibraryFolder, ProjectFolder, AssetId)
		: f101::assets::importLibraryTexture(*Texture, LibraryFolder, ProjectFolder, AssetId);
	if (!Imported)
	{
		SetStatus(ToText(Imported.error().message), true);
		return Imported.error();
	}
	const f101::Result<f101::cad::Transaction> AddedAsset = Document->execute(f101::cad::AddAssetCommand{Imported.value().asset});
	if (!AddedAsset)
	{
		SetStatus(ToText(AddedAsset.error().message), true);
		return AddedAsset.error();
	}
	SetStatus(FText::Format(LOCTEXT("AddedFromLibrary", "Añadido de la biblioteca: {0}"), ToText(Imported.value().asset.name)), false);
	return AssetId;
}

void UFloor101DocumentSubsystem::RegisterAssetFinishes()
{
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	if (!Library)
	{
		return;
	}
	const std::filesystem::path Folder = ToFilesystemPath(GetProjectFolder());
	for (const f101::cad::Asset& Asset : Document->project().assets)
	{
		const FString Ref = UTF8_TO_TCHAR(f101::cad::assetRef(Asset.id).c_str());
		if (Asset.kind == f101::cad::AssetKind::Texture)
		{
			FFloor101AssetFinish Finish;
			Finish.ImagePath = GetAssetFilePath(Asset);
			Finish.Roughness = static_cast<float>(Asset.texture.roughnessPercent) / 100.0f;
			Finish.RepeatU = static_cast<float>(Asset.texture.widthMm) / 1000.0f;
			Finish.RepeatV = static_cast<float>(Asset.texture.heightMm) / 1000.0f;
			Library->SetAssetFinish(Ref, Finish);
			continue;
		}
		const f101::assets::LoadedModel* Model = Models->get(Folder, Asset);
		if (!Model)
		{
			continue;
		}
		const FString ModelFolder = FPaths::GetPath(GetAssetFilePath(Asset));
		for (std::size_t Index = 0; Index < Model->materials.size(); ++Index)
		{
			const f101::assets::ModelMaterial& Material = Model->materials[Index];
			FFloor101AssetFinish Finish;
			Finish.ImagePath = Material.texture.empty() ? FString() : FPaths::Combine(ModelFolder, UTF8_TO_TCHAR(Material.texture.c_str()));
			Finish.bFlipImage = false; // model UVs already address the image
			Finish.Tint = FLinearColor::FromSRGBColor(FColor(Material.color.r, Material.color.g, Material.color.b));
			Finish.Roughness = static_cast<float>(Material.roughness);
			Library->SetAssetFinish(UTF8_TO_TCHAR(f101::cad::modelMaterialFinish(Asset.id, Index).c_str()), Finish);
		}
	}
}

AActor* UFloor101DocumentSubsystem::FindWallActor(const f101::cad::WallId& Id) const
{
	return Reconciler->FindActor(f101::scene::wallBodyNodeId(Id));
}

AActor* UFloor101DocumentSubsystem::FindRoomFloorActor(const FString& RoomKey) const
{
	return Reconciler->FindActor(f101::scene::SceneNodeId{std::string(TCHAR_TO_UTF8(*RoomKey)) + "/floor"});
}

AActor* UFloor101DocumentSubsystem::FindRoomSlabActor(const FString& RoomKey) const
{
	return Reconciler->FindActor(f101::scene::SceneNodeId{std::string(TCHAR_TO_UTF8(*RoomKey)) + "/slab"});
}

AActor* UFloor101DocumentSubsystem::FindFurnitureActor(const f101::cad::FurnitureId& Id) const
{
	return Reconciler->FindActor(f101::scene::furnitureNodeId(Id));
}

AActor* UFloor101DocumentSubsystem::FindColumnActor(const f101::cad::ColumnId& Id) const
{
	return Reconciler->FindActor(f101::scene::columnNodeId(Id));
}

AActor* UFloor101DocumentSubsystem::FindServiceRunActor(const f101::cad::ServiceRunId& Id) const
{
	return Reconciler->FindActor(f101::scene::serviceRunNodeId(Id));
}

void UFloor101DocumentSubsystem::SetPlanView(bool bShown)
{
	bPlanView = bShown;
	if (!bShown)
	{
		*PlanDrawing = f101::drawing::PlanDrawing{};
	}
	Resync(false);
}

const f101::drawing::PlanDrawing& UFloor101DocumentSubsystem::GetPlanDrawing() const
{
	return *PlanDrawing;
}

FString UFloor101DocumentSubsystem::GetDefaultPlanPath(const FString& Extension) const
{
	const FString Project = ProjectPath.IsEmpty() ? GetDefaultProjectPath() : ProjectPath;
	const f101::cad::Project& Model = Document->project();
	const f101::cad::LevelId Level = Editor->activeLevel();
	const f101::cad::Level* Found = Model.findLevel(Level);
	const std::size_t Index = Model.levelIndex(Level).value_or(0);
	FString Name = Found && !Found->name.empty() ? FString(UTF8_TO_TCHAR(Found->name.c_str()))
		: Index == 0 ? FString(TEXT("planta baja")) : FString::Printf(TEXT("planta %d"), static_cast<int32>(Index));
	// A file name: lower case, words joined by hyphens, no characters Windows forbids.
	Name = Name.ToLower().TrimStartAndEnd();
	FString Slug;
	for (const TCHAR Character : Name)
	{
		Slug.AppendChar(FChar::IsWhitespace(Character) || FCString::Strchr(TEXT("\\/:*?\"<>|"), Character) ? TEXT('-') : Character);
	}
	return FPaths::Combine(FPaths::GetPath(Project), FPaths::GetBaseFilename(Project) + TEXT("-") + Slug + TEXT(".") + Extension);
}

void UFloor101DocumentSubsystem::SetPhotoMode(bool bPhoto)
{
	if (bPhotoMode == bPhoto)
	{
		return;
	}
	bPhotoMode = bPhoto;
	SetStatus(FText::GetEmpty(), false);
	Resync(false);
}

FString UFloor101DocumentSubsystem::GetDefaultPhotoPath() const
{
	const FString Project = ProjectPath.IsEmpty() ? GetDefaultProjectPath() : ProjectPath;
	return FPaths::Combine(FPaths::GetPath(Project),
		FPaths::GetBaseFilename(Project) + TEXT("-foto-") + FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT(".png"));
}

FString UFloor101DocumentSubsystem::GetDefaultGltfPath() const
{
	const FString Project = ProjectPath.IsEmpty() ? GetDefaultProjectPath() : ProjectPath;
	return FPaths::Combine(FPaths::GetPath(Project), FPaths::GetBaseFilename(Project) + TEXT(".glb"));
}

f101::Result<FFloor101GltfSummary> UFloor101DocumentSubsystem::ExportGltf(const FString& Path)
{
	if (!Path.EndsWith(TEXT(".glb"), ESearchCase::IgnoreCase))
	{
		const f101::Error Wrong{f101::ErrorCode::InvalidArgument, "Export the model as .glb (binary glTF)"};
		SetStatus(ToText(Wrong.message), true);
		return Wrong;
	}
	const f101::Result<f101::exporters::GltfExport> Exported = f101::exporters::exportProjectGlb(Document->project(),
		ToFilesystemPath(GetProjectFolder()), *Models, ExportTexts());
	if (!Exported)
	{
		SetStatus(ToText(Exported.error().message), true);
		return Exported.error();
	}
	const f101::Status Saved = f101::exporters::saveGlbFile(ToFilesystemPath(Path), Exported.value().bytes);
	if (!Saved)
	{
		SetStatus(ToText(Saved.error().message), true);
		return Saved.error();
	}
	FFloor101GltfSummary Summary;
	Summary.Objects = static_cast<int32>(Exported.value().objects);
	Summary.Materials = static_cast<int32>(Exported.value().materials);
	Summary.Images = static_cast<int32>(Exported.value().images);
	Summary.Triangles = static_cast<int64>(Exported.value().triangles);
	Summary.Bytes = static_cast<int64>(Exported.value().bytes.size());
	for (const std::string& Warning : Exported.value().warnings)
	{
		Summary.Warnings.Add(UTF8_TO_TCHAR(Warning.c_str()));
		UE_LOG(LogFloor101, Warning, TEXT("F101: export %s: %s"), *Path, UTF8_TO_TCHAR(Warning.c_str()));
	}
	FNumberFormattingOptions OneDecimal;
	OneDecimal.SetMinimumFractionalDigits(1).SetMaximumFractionalDigits(1);
	const FText Done = FText::Format(LOCTEXT("GltfExported", "Exportado para Twinmotion: {0} ({1} objetos, {2} materiales, {3} MB)"),
		FText::FromString(Path), Summary.Objects, Summary.Materials, FText::AsNumber(static_cast<double>(Summary.Bytes) / (1024.0 * 1024.0), &OneDecimal));
	SetStatus(Summary.Warnings.IsEmpty() ? Done
		: FText::Format(LOCTEXT("GltfExportedWarning", "{0} · aviso: {1}"), Done, FText::FromString(Summary.Warnings[0])), false);
	return Summary;
}

f101::Status UFloor101DocumentSubsystem::SavePhoto(const FString& Path)
{
	auto Fail = [this](const std::string& Message) -> f101::Status
	{
		SetStatus(ToText(Message), true);
		return f101::Error{f101::ErrorCode::InvalidArgument, Message};
	};
	if (!FPaths::GetExtension(Path).Equals(TEXT("png"), ESearchCase::IgnoreCase))
	{
		return Fail("Save the photo as .png");
	}
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true))
	{
		return Fail("Cannot create the folder " + std::string(TCHAR_TO_UTF8(*FPaths::GetPath(Path))));
	}
	Floor101::PathTracing::RequestImage(Path);
	SetStatus(FText::Format(LOCTEXT("PhotoSaved", "Foto guardada: {0}"), FText::FromString(Path)), false);
	return f101::Status::success();
}

f101::Status UFloor101DocumentSubsystem::ExportPlan(const FString& Path, const f101::cad::LevelId& Level)
{
	const f101::cad::Project& Model = Document->project();
	const f101::cad::LevelId Target = Level.isNil() ? Editor->activeLevel() : Level;
	auto Fail = [this](const std::string& Message) -> f101::Status
	{
		const f101::Error Failed{f101::ErrorCode::InvalidArgument, Message};
		SetStatus(ToText(Message), true);
		return Failed;
	};
	if (!Model.levels.empty() && !Model.findLevel(Target))
	{
		return Fail("Level not found: " + f101::cad::toString(Target));
	}
	const FString Extension = FPaths::GetExtension(Path).ToLower();
	if (Extension != TEXT("svg") && Extension != TEXT("png"))
	{
		return Fail("Export the plan as .svg or .png");
	}
	const FString Directory = FPaths::GetPath(Path);
	if (!Directory.IsEmpty() && !IFileManager::Get().MakeDirectory(*Directory, true))
	{
		return Fail("Cannot create the folder " + std::string(TCHAR_TO_UTF8(*Directory)));
	}
	// No editing marks; the runs of the building services when their layer is shown, of the networks it shows (ADR-0021).
	f101::drawing::PlanOverlay Layers;
	Layers.services = Editor->showsServices();
	Layers.serviceKinds = Editor->shownServiceKinds();
	const f101::drawing::PlanDrawing Drawing = f101::drawing::buildPlanDrawing(Model, Target, Layers, PlanTexts());
	if (Extension == TEXT("svg"))
	{
		const std::string Svg = f101::drawing::writeSvg(Drawing, f101::drawing::SvgOptions{100.0, PlanMarginMm});
		if (!FFileHelper::SaveArrayToFile(TArrayView<const uint8>(reinterpret_cast<const uint8*>(Svg.data()), static_cast<int32>(Svg.size())), *Path))
		{
			return Fail("Cannot write " + std::string(TCHAR_TO_UTF8(*Path)));
		}
	}
	else if (const f101::Status Saved = SavePlanPng(Drawing, Path); !Saved)
	{
		return Fail(Saved.error().message);
	}
	SetStatus(FText::Format(LOCTEXT("PlanExported", "Plano exportado: {0}"), FText::FromString(Path)), false);
	return f101::Status::success();
}

f101::Status UFloor101DocumentSubsystem::SavePlanPng(const f101::drawing::PlanDrawing& Drawing, const FString& Path) const
{
	if (GUsingNullRHI || !FApp::CanEverRender() || !FSlateApplication::IsInitialized())
	{
		return f101::Error{f101::ErrorCode::InvalidArgument, "Exporting a PNG needs a renderer (not available with -nullrhi)"};
	}
	const double WidthMm = (Drawing.empty() ? 0.0 : Drawing.max.x_mm - Drawing.min.x_mm) + 2.0 * PlanMarginMm;
	const double HeightMm = (Drawing.empty() ? 0.0 : Drawing.max.y_mm - Drawing.min.y_mm) + 2.0 * PlanMarginMm;
	const double Scale = FMath::Min(PlanPngPixelsPerMm, PlanPngMaxPixels / FMath::Max(WidthMm, HeightMm));
	const FIntPoint Pixels(FMath::Max(1, FMath::CeilToInt32(WidthMm * Scale)), FMath::Max(1, FMath::CeilToInt32(HeightMm * Scale)));
	const TSharedRef<SFloor101PlanView> Sheet = SNew(SFloor101PlanView)
		.Drawing([&Drawing]() { return &Drawing; })
		.MarginMm(PlanMarginMm);
	// Slate blends in linear space and its gamma correction writes sRGB, so the target must not encode again (an sRGB
	// target makes everything paler; no correction makes it darker): the pixels come out as the inks' sRGB colours.
	FWidgetRenderer Renderer(true, true);
	UTextureRenderTarget2D* Target = FWidgetRenderer::CreateTargetFor(FVector2D(Pixels), TF_Bilinear, false);
	if (!Target)
	{
		return f101::Error{f101::ErrorCode::InvalidArgument, "Cannot create the image to render the plan into"};
	}
	Renderer.DrawWidget(Target, Sheet, FVector2D(Pixels), 0.0f);
	FlushRenderingCommands();
	TArray<FColor> Colors;
	FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
	if (!Resource || !Resource->ReadPixels(Colors) || Colors.Num() != Pixels.X * Pixels.Y)
	{
		return f101::Error{f101::ErrorCode::InvalidArgument, "Cannot read the rendered plan"};
	}
	for (FColor& Color : Colors)
	{
		Color.A = 255; // paper is opaque
	}
	if (!FImageUtils::SaveImageByExtension(*Path, FImageView(Colors.GetData(), Pixels.X, Pixels.Y)))
	{
		return f101::Error{f101::ErrorCode::InvalidArgument, "Cannot write " + std::string(TCHAR_TO_UTF8(*Path))};
	}
	return f101::Status::success();
}

void UFloor101DocumentSubsystem::LoadReferenceWall()
{
	if (!ReferenceWallId.isNil() && Document->project().findWall(ReferenceWallId))
	{
		UE_LOG(LogFloor101, Display, TEXT("F101: reference wall already loaded"));
		return;
	}
	const f101::Result<f101::cad::WallId> Added = Document->addWall(
		{0, 0}, {ReferenceLengthMm, 0}, ReferenceThicknessMm, ReferenceHeightMm);
	check(Added.ok());
	ReferenceWallId = Added.value();
}

bool UFloor101DocumentSubsystem::SetReferenceWallLength(int64 LengthMm)
{
	if (ReferenceWallId.isNil())
	{
		UE_LOG(LogFloor101, Warning, TEXT("F101: no reference wall (run f101.Demo.Load first)"));
		return false;
	}
	const f101::Status Changed = Document->setWallEnd(ReferenceWallId, {LengthMm, 0});
	if (!Changed)
	{
		UE_LOG(LogFloor101, Warning, TEXT("F101: length %lld mm rejected: %s"), LengthMm, UTF8_TO_TCHAR(Changed.error().message.c_str()));
		return false;
	}
	return true;
}

void UFloor101DocumentSubsystem::ClearReferenceWall()
{
	if (!ReferenceWallId.isNil())
	{
		const f101::Status Removed = Document->removeWall(ReferenceWallId);
		check(Removed.ok());
		ReferenceWallId = f101::cad::WallId();
	}
}

void UFloor101DocumentSubsystem::Resync(bool bDocumentChanged)
{
	const double Start = FPlatformTime::Seconds();

	const f101::cad::EditView View = Editor->view();
	f101::scene::ViewOverlay Overlay;
	Overlay.selectedWall = View.selectedWall;
	Overlay.previewWalls = View.previewWalls;
	Overlay.markers = View.handles;
	Overlay.markerSizeMm = Editor->settings().handleSizeMm;
	Overlay.markerHeightMm = Editor->settings().handleHeightMm;
	Overlay.activeLevel = View.activeLevel;
	Overlay.showAllLevels = View.showAllLevels;
	Overlay.selectedRoom = View.selectedRoom;
	Overlay.selectedFurniture = View.selectedFurniture;
	Overlay.previewFurniture = View.previewFurniture;
	Overlay.services = View.showServices;
	Overlay.serviceKinds = View.serviceKinds;
	Overlay.selectedColumn = View.selectedColumn;
	Overlay.previewColumns = View.previewColumns;
	Overlay.selectedServiceRun = View.selectedServiceRun;
	Overlay.previewServiceRuns = View.previewServiceRuns;
	if (bPhotoMode)
	{
		// A photo shows the project as it is (ADR-0018): no selection, gesture, handles or door swings. The services
		// layer stays as chosen, so the installation can be photographed (ADR-0021).
		const f101::scene::ViewOverlay Levels = Overlay;
		Overlay = f101::scene::ViewOverlay{};
		Overlay.activeLevel = Levels.activeLevel;
		Overlay.showAllLevels = Levels.showAllLevels;
		Overlay.services = Levels.services;
		Overlay.serviceKinds = Levels.serviceKinds;
		Overlay.symbols = false;
	}

	// Imported assets (ADR-0017): their looks registered first, models read from the project's folder (once per file).
	RegisterAssetFinishes();
	const std::filesystem::path AssetFolder = ToFilesystemPath(GetProjectFolder());
	const f101::scene::ModelProvider ModelMeshes = [this, &AssetFolder](const f101::cad::Asset& Asset) -> const f101::geometry::ModelMesh*
	{
		const f101::assets::LoadedModel* Model = Models->get(AssetFolder, Asset);
		return Model ? &Model->mesh : nullptr;
	};
	const f101::scene::Scene Scene = f101::scene::compileScene(Document->project(), Overlay, ModelMeshes);
	const FFloor101ReconcileStats Stats = Reconciler->Reconcile(Scene);
	SetGroundGhosted(Overlay.services);
	if (bPlanView)
	{
		f101::drawing::PlanOverlay Marks;
		Marks.selectedWall = View.selectedWall;
		Marks.previewWalls = View.previewWalls;
		Marks.selectedFurniture = View.selectedFurniture;
		Marks.previewFurniture = View.previewFurniture;
		Marks.services = View.showServices;
		Marks.serviceKinds = View.serviceKinds;
		Marks.selectedColumn = View.selectedColumn;
		Marks.previewColumns = View.previewColumns;
		Marks.selectedServiceRun = View.selectedServiceRun;
		Marks.previewServiceRuns = View.previewServiceRuns;
		Marks.handles = View.handles;
		Marks.handleSizeMm = static_cast<double>(Editor->settings().handleSizeMm);
		*PlanDrawing = f101::drawing::buildPlanDrawing(Document->project(), View.activeLevel, Marks, PlanTexts());
	}

	// Labels sit on the floor of each room of the edited level, or of every level when all are shown (preview included,
	// so they follow a gesture live).
	Rooms.Reset(static_cast<int32>(Scene.rooms.size()));
	for (const f101::scene::SceneRoom& Entry : Scene.rooms)
	{
		if (!View.showAllLevels && Entry.level != View.activeLevel)
		{
			continue;
		}
		const f101::geometry::Room& Room = Entry.room;
		const FVector Label = FVector(Floor101::Conversion::ToUnrealPosition({Room.labelPoint.x_mm, Room.labelPoint.y_mm,
			static_cast<double>(Entry.elevationMm) + f101::geometry::FloorThicknessMm}));
		Rooms.Add(FFloor101RoomView{UTF8_TO_TCHAR(Room.key.c_str()), Label, Room.areaMm2 / 1.0e6});
	}
	if (bDocumentChanged)
	{
		UE_LOG(LogFloor101, Display, TEXT("F101: scene resync in %.3f ms (created %d, updated %d, unchanged %d, removed %d)"),
			(FPlatformTime::Seconds() - Start) * 1000.0, Stats.Created, Stats.Updated, Stats.Unchanged, Stats.Removed);
		LogStatus();
	}
	else
	{
		UE_LOG(LogFloor101, Verbose, TEXT("F101: view refresh in %.3f ms (created %d, updated %d, unchanged %d, removed %d)"),
			(FPlatformTime::Seconds() - Start) * 1000.0, Stats.Created, Stats.Updated, Stats.Unchanged, Stats.Removed);
	}
}

void UFloor101DocumentSubsystem::SetGroundGhosted(bool bGhosted)
{
	if (bGhosted == bGroundGhosted)
	{
		return;
	}
	bGroundGhosted = bGhosted;
	if (!bGhosted)
	{
		for (int32 Slot = 0; Slot < GhostedSlots.Num(); ++Slot)
		{
			if (UMeshComponent* Component = GhostedSlots[Slot].Component.Get())
			{
				Component->SetMaterial(GhostedSlots[Slot].Index, GroundMaterials[Slot]);
			}
		}
		GhostedSlots.Reset();
		GroundMaterials.Reset();
		return;
	}
	UFloor101FinishLibrary* Library = UFloor101FinishLibrary::Get();
	UMaterialInterface* Ghost = Library ? Library->GetGhostMaterial() : nullptr;
	UWorld* World = GetWorld();
	if (!Ghost || !World)
	{
		return;
	}
	// The map's static meshes: the project's own actors are AFloor101SceneNodeActor, never static mesh actors.
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		UStaticMeshComponent* Component = It->GetStaticMeshComponent();
		if (!Component || !Component->IsVisible())
		{
			continue;
		}
		for (int32 Index = 0; Index < Component->GetNumMaterials(); ++Index)
		{
			GhostedSlots.Add(FGhostedSlot{Component, Index});
			GroundMaterials.Add(Component->GetMaterial(Index));
			Component->SetMaterial(Index, Ghost);
		}
	}
}

void UFloor101DocumentSubsystem::LogStatus() const
{
	for (const f101::cad::Wall& Wall : Document->project().walls)
	{
		const AFloor101SceneNodeActor* Actor = Reconciler->FindActor(f101::scene::wallBodyNodeId(Wall.id));
		const FVector Size = Actor ? Actor->GetMeshBounds().GetSize() : FVector::ZeroVector;
		UE_LOG(LogFloor101, Display,
			TEXT("F101: wall %s length=%.0f mm thickness=%lld mm height=%lld mm -> actor %s size=(%.1f, %.1f, %.1f) cm"),
			UTF8_TO_TCHAR(f101::cad::toString(Wall.id).c_str()), Wall.lengthMm(), Wall.thicknessMm, Wall.heightMm,
			Actor ? *Actor->GetName() : TEXT("<none>"), Size.X, Size.Y, Size.Z);
	}
	if (Document->project().walls.empty())
	{
		UE_LOG(LogFloor101, Display, TEXT("F101: document has no walls"));
	}
	for (const FFloor101RoomView& Room : Rooms)
	{
		const AActor* Floor = FindRoomFloorActor(Room.Key);
		UE_LOG(LogFloor101, Display, TEXT("F101: room %s area=%.4f m2 -> floor %s"), *Room.Key, Room.AreaM2,
			Floor ? *Floor->GetName() : TEXT("<none>"));
	}
}

#undef LOCTEXT_NAMESPACE
