#include "PackageCheck.h"
#include "Floor101.h"
#include "Floor101DocumentSubsystem.h"
#include "Floor101EditingPawn.h"
#include "Floor101SceneNodeActor.h"
#include "EngineUtils.h"
#include "Floor101FinishLibrary.h"
#include "Floor101GameViewportClient.h"
#include "Floor101PathTracing.h"
#include "Floor101Preferences.h"
#include "F101/Assets/Library.h"
#include "F101/Cad/Document.h"
#include "F101/Cad/EditController.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "InputCoreTypes.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "UnrealClient.h"

namespace Floor101::PackageCheck
{
	// Runs only with an explicit output directory. Does not touch the user's project or preferences.
	void Start(UFloor101GameViewportClient* Client)
	{
		FString Directory;
		if (!Client || !FParse::Param(FCommandLine::Get(), TEXT("F101PackageCheck")) ||
			!FParse::Value(FCommandLine::Get(), TEXT("F101PackageOutput="), Directory)) { return; }
		Directory = FPaths::ConvertRelativePathToFull(Directory);
		IFileManager::Get().MakeDirectory(*Directory, true);
		const TWeakObjectPtr<UFloor101GameViewportClient> WeakClient = Client;
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakClient, Directory, Elapsed = 0.0f, Phase = 0, Passed = true](float Delta) mutable
		{
			Elapsed += Delta;
			auto* Viewport = WeakClient.Get();
			if (!Viewport || Elapsed > 150.0f) { FPlatformMisc::RequestExitWithStatus(false, 1); return false; }
			auto Check = [&Passed](bool Condition, const TCHAR* Name)
			{
				Passed = Passed && Condition;
				F101_LOG(LogFloor101, Display, TEXT("F101PackageCheck: %s=%s"), Name, Condition ? TEXT("PASS") : TEXT("FAIL"));
				return Condition;
			};
			auto* Document = Viewport->GetWorld()->GetSubsystem<UFloor101DocumentSubsystem>();
			if (!Document || PathTracing::GetPendingShaders() > 0 || Elapsed < 2.0f) { return true; }
			if (Phase == 0)
			{
				Check(!GIsEditor, TEXT("standalone_game"));
				const auto& Library = UFloor101DocumentSubsystem::GetLibrary();
				Check(Library.furniture.size() >= 50 && Library.textures.size() >= 14, TEXT("shipped_library"));
				auto* Finishes = UFloor101FinishLibrary::Get();
				if (!Check(Finishes && Finishes->GetBaseMaterial() && Finishes->GetGlassMaterial(), TEXT("cooked_materials")))
				{ FPlatformMisc::RequestExitWithStatus(false, 1); return false; }
				Check(Document->NewProject().ok(), TEXT("new_project"));
				Check(Document->SaveProject(FPaths::Combine(Directory, TEXT("project.json"))).ok(), TEXT("save_empty"));
				for (const auto& Ends : {std::pair<f101::cad::Point2D, f101::cad::Point2D>{{0,0},{4000,0}}, {{4000,0},{4000,4000}}, {{4000,4000},{0,4000}}, {{0,4000},{0,0}}})
				{ Check(Document->GetMutableDocument().addWall(Ends.first, Ends.second, 120, 2800).ok(), TEXT("create_wall")); }
				Check(Document->GetRooms().Num() == 1, TEXT("room_detection"));
				Check(Document->GetMutableDocument().undo().ok() && Document->GetDocument().project().walls.size() == 3, TEXT("undo"));
				Check(Document->GetMutableDocument().redo().ok() && Document->GetDocument().project().walls.size() == 4, TEXT("redo"));
				if (!Document->GetRooms().IsEmpty())
				{
					const std::string RoomKey = TCHAR_TO_UTF8(*Document->GetRooms()[0].Key);
					Check(Document->Edit([RoomKey](f101::cad::EditController& E) { return E.selectRoom(RoomKey); }).ok(), TEXT("room_select"));
					Check(Document->Edit([](f101::cad::EditController& E) { return E.setSelectedRoomName("Cocina"); }).ok(), TEXT("room_rename"));
					Check(Document->GetRooms()[0].Name == TEXT("Cocina"), TEXT("room_name_view"));
					Check(Document->Edit([](f101::cad::EditController& E) { return E.setSelectedRoomBaseboard(true); }).ok(), TEXT("baseboard_enable"));
					Check(Document->Edit([](f101::cad::EditController& E) { return E.setSelectedBaseboardHeight("15 cm"); }).ok(), TEXT("baseboard_height"));
					Check(Document->Edit([](f101::cad::EditController& E) { return E.setSelectedBaseboardFinish("wood-walnut"); }).ok(), TEXT("baseboard_finish"));
					const FString NodeId = Document->GetRooms()[0].Key + TEXT("/baseboard");
					bool Rendered = false;
					for (TActorIterator<AFloor101SceneNodeActor> It(Viewport->GetWorld()); It; ++It)
					{
						if (It->GetSceneNodeId() == NodeId) Rendered = It->GetDisplayMaterial() == Finishes->GetFinishMaterial(TEXT("wood-walnut"));
					}
					Check(Rendered, TEXT("baseboard_render_material"));
				}
				const auto Chair = Document->AddFromLibrary(TEXT("chair-windsor"));
				if (Check(Chair.ok(), TEXT("import_furniture")))
				{
					const std::string Ref = f101::cad::assetRef(Chair.value());
					Check(Document->Edit([&Ref](f101::cad::EditController& E) { E.setMode(f101::cad::EditMode::Furniture); return E.setFurnitureType(Ref); }).ok(), TEXT("furniture_tool"));
					Check(Document->Edit([](f101::cad::EditController& E) { return E.pointerPressed({2000,2000,10000,0,0,-1}); }).ok(), TEXT("place_furniture"));
					Check(Document->GetAssetProblem(Chair.value()).IsEmpty(), TEXT("furniture_render_asset"));
				}
				const auto Laminate = Document->AddFromLibrary(TEXT("photo-laminate-oak"));
				if (Check(Laminate.ok(), TEXT("import_texture")))
				{
					const std::string Ref = f101::cad::assetRef(Laminate.value());
					Check(Document->Edit([&Ref](f101::cad::EditController& E) { E.setMode(f101::cad::EditMode::Paint); return E.setPaintFinish(Ref); }).ok(), TEXT("paint_tool"));
					Check(Document->Edit([](f101::cad::EditController& E) { return E.pointerPressed({1000,3000,10000,0,0,-1}); }).ok(), TEXT("paint_floor"));
					Check(Finishes->GetAssetFinishImage(UTF8_TO_TCHAR(Ref.c_str())) != nullptr, TEXT("texture_decode"));
				}
				Check(Document->GetMutableDocument().addWall({1000,-1000},{1000,5000},120,2800).ok() && Document->GetDocument().project().walls.size() > 5, TEXT("automatic_wall_split"));
				const auto Walls = Document->GetDocument().project().walls.size();
				Check(Document->SaveProject(FPaths::Combine(Directory, TEXT("project.json"))).ok(), TEXT("save_project"));
				Check(Document->NewProject().ok() && Document->OpenProject(FPaths::Combine(Directory, TEXT("project.json"))).ok() &&
					Document->GetDocument().project().walls.size() == Walls && Document->GetDocument().project().furniture.size() == 1, TEXT("reopen_project"));
				Check(!Document->GetRooms().IsEmpty() && Document->GetRooms().ContainsByPredicate([](const FFloor101RoomView& Room) { return Room.Name == TEXT("Cocina"); }), TEXT("room_name_reopen"));
				bool BaseboardsPersisted = !Document->GetRooms().IsEmpty();
				for (const auto& Room : Document->GetRooms())
				{
					const auto* B = Document->GetDocument().project().findRoomBaseboard(TCHAR_TO_UTF8(*Room.Key));
					BaseboardsPersisted = BaseboardsPersisted && B && B->enabled && B->heightMm == 150 && B->finish == "wood-walnut";
				}
				Check(BaseboardsPersisted, TEXT("baseboard_division_reopen"));
				Check(Document->ExportPlan(FPaths::Combine(Directory, TEXT("plan.svg")), {}).ok(), TEXT("export_svg"));
				FString NamedSvg;
				Check(FFileHelper::LoadFileToString(NamedSvg, *FPaths::Combine(Directory, TEXT("plan.svg"))) && NamedSvg.Contains(TEXT("Cocina")), TEXT("room_name_svg"));
				Check(Document->ExportPlan(FPaths::Combine(Directory, TEXT("plan.png")), {}).ok(), TEXT("export_png"));
				Check(Document->ExportGltf(FPaths::Combine(Directory, TEXT("scene.glb"))).ok(), TEXT("export_glb"));
				Viewport->FrameProject();
				(void)Document->Edit([](f101::cad::EditController& E) { E.setMode(f101::cad::EditMode::Select); return f101::Status::success(); });
				FSlateApplication::Get().SetAllUserFocusToGameViewport();
				const FString OriginalIni = GGameUserSettingsIni;
				const bool OriginalDebug = Diagnostics::IsVerbose();
				const bool OriginalWindow = Diagnostics::IsWindowVisible();
				GGameUserSettingsIni = FPaths::Combine(Directory, TEXT("test-preferences.ini"));
				FFileHelper::SaveStringToFile(TEXT("[SectionsToSave]\nbCanSaveAllSections=true\n"), *GGameUserSettingsIni);
				GConfig->LoadFile(GGameUserSettingsIni);
				ON_SCOPE_EXIT { GConfig->UnloadFile(GGameUserSettingsIni); GGameUserSettingsIni = OriginalIni; Diagnostics::SetVerbose(OriginalDebug); Diagnostics::SetWindowVisible(OriginalWindow); };
				Viewport->InputKey(FInputKeyEventArgs(Viewport->Viewport, FInputDeviceId::CreateFromInternalId(0), EKeys::Zero, IE_Pressed, 0ULL));
				Check(Viewport->GetNavigationSpeedLevel() == 10, TEXT("top_row_speed_zero"));
				Viewport->InputKey(FInputKeyEventArgs(Viewport->Viewport, FInputDeviceId::CreateFromInternalId(0), EKeys::Three, IE_Pressed, 0ULL));
				Check(Viewport->GetNavigationSpeedLevel() == 3, TEXT("top_row_speed_three"));
				Check(UFloor101GameViewportClient::MapKey(EKeys::W, false, false, false, false, false) == EFloor101KeyCommand::NavigationMove, TEXT("wasd_without_n"));
				if (auto* Pawn = Cast<AFloor101EditingPawn>(Viewport->GetWorld()->GetFirstPlayerController()->GetPawn()))
				{
					const FVector Before = Pawn->GetFocus(); Pawn->Move(FVector(1,0,0), 1.0f, false);
					Check(!Pawn->GetFocus().Equals(Before), TEXT("camera_motion"));
				}
				Preferences::SetDiagnosticMode(true);
				Check(Diagnostics::IsVerbose() && Preferences::DiagnosticMode(), TEXT("debug_toggle_on"));
				Preferences::SetShowLogWindow(true);
				Check(Diagnostics::IsWindowVisible(), TEXT("logs_window_on"));
				Preferences::SetShowLogWindow(false);
				Check(!Diagnostics::IsWindowVisible(), TEXT("logs_window_off"));
				Preferences::SetDiagnosticMode(false);
				Check(!Diagnostics::IsVerbose() && !Preferences::DiagnosticMode(), TEXT("debug_toggle_off"));
				Viewport->SetNavigationMode(true);
				Check(!Document->AreConstructionAidsVisible(), TEXT("walk_hides_guides"));
				Viewport->SetNavigationMode(false);
				Viewport->FrameProject();
				Phase = 1;
			}
			if (Phase == 1 && Elapsed > 8.0f)
			{
				FScreenshotRequest::RequestScreenshot(FPaths::Combine(Directory, TEXT("design.png")), true, false);
				Viewport->SetPhotoSamples(8);
				Phase = 2;
			}
			if (Phase == 2 && Elapsed > 10.0f) { Viewport->SetPhotoMode(true); Phase = 3; }
			if (Phase == 3 && PathTracing::IsComplete())
			{
				Check(!Document->AreConstructionAidsVisible(), TEXT("photo_hides_guides"));
				Check(Document->SavePhoto(FPaths::Combine(Directory, TEXT("photo.png"))).ok(), TEXT("photo_request"));
				Phase = 4;
			}
			if (Phase == 4 && FPaths::FileExists(FPaths::Combine(Directory, TEXT("photo.png"))))
			{
				FImage Photo;
				Check(FImageUtils::LoadImage(*FPaths::Combine(Directory, TEXT("photo.png")), Photo) && Photo.SizeX > 0 && Photo.SizeY > 0, TEXT("photo_image"));
				Viewport->SetPhotoMode(false);
				F101_LOG(LogFloor101, Display, TEXT("F101PackageCheck: result=%s"), Passed ? TEXT("PASS") : TEXT("FAIL"));
				FPlatformMisc::RequestExitWithStatus(false, Passed ? 0 : 1);
				return false;
			}
			return true;
		}));
	}
}
