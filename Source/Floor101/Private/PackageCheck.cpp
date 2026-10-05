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
#include "F101/Drawing/PlanDrawing.h"
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
				{
					auto Legacy = Document->GetDocument().project();
					uint64 Id = 310;
					for (const auto& Ends : {std::pair<f101::cad::Point2D, f101::cad::Point2D>{{0,0},{4300,0}},
						{{4300,0},{4300,2550}}, {{4300,2490},{4300,2590}}, {{4300,2590},{0,2590}},
						{{0,2590},{0,0}}, {{2384,0},{2384,2590}}})
					{
						f101::cad::Wall Wall{f101::cad::WallId{f101::cad::Uuid{0, Id++}}, Ends.first, Ends.second, 120, 2800};
						Wall.level = Legacy.levels.front().id;
						Legacy.walls.push_back(Wall);
					}
					Check(Document->GetMutableDocument().replaceProject(Legacy).ok() && Document->GetRooms().Num() == 2, TEXT("overlap_room_detection"));
					Viewport->SetPlanView(true);
					Check(Document->Edit([](f101::cad::EditController& E) { return E.pointerPressed({3300,1300,10000,0,0,-1}); }).ok(), TEXT("overlap_room_pick"));
					Check(Document->Edit([](f101::cad::EditController& E) { return E.setSelectedRoomName("Baño"); }).ok(), TEXT("overlap_room_rename"));
					Check(std::any_of(Document->GetPlanDrawing().labels.begin(), Document->GetPlanDrawing().labels.end(),
						[](const auto& Label) { return Label.text == "Baño"; }), TEXT("overlap_room_label"));
					Viewport->SetPlanView(false);
					Check(Document->NewProject().ok(), TEXT("clear_overlap_fixture"));
				}
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
				Viewport->SetPlanView(true);
				Check(Document->Edit([](f101::cad::EditController& E) { E.setMode(f101::cad::EditMode::Dimension); return f101::Status::success(); }).ok(), TEXT("dimension_tool"));
				for (const auto Point : {f101::cad::Point2D{1511,1013}, f101::cad::Point2D{3487,1013}, f101::cad::Point2D{2500,1600}})
					Check(Document->Edit([Point](f101::cad::EditController& E) { return E.pointerPressed({static_cast<double>(Point.x_mm),static_cast<double>(Point.y_mm),10000,0,0,-1}); }).ok(), TEXT("dimension_click"));
				Check(Document->GetDocument().project().dimensions.size() == 1, TEXT("dimension_manual"));
				const auto Manual = Document->GetDocument().project().dimensions.front();
				Check(Manual.start == f101::cad::Point2D{1511,1013} && Manual.end == f101::cad::Point2D{3487,1013}, TEXT("dimension_exact_points"));
				Check(Document->Edit([Manual](f101::cad::EditController& E) { return E.pointerPressed({static_cast<double>(Manual.end.x_mm),static_cast<double>(Manual.end.y_mm),10000,0,0,-1}); }).ok(), TEXT("dimension_grab_endpoint"));
				Check(Document->Edit([](f101::cad::EditController& E) { E.pointerMoved({3504,1021,10000,0,0,-1}); return f101::Status::success(); }).ok() &&
					Document->Edit([](f101::cad::EditController& E) { return E.pointerReleased({3504,1021,10000,0,0,-1}); }).ok(), TEXT("dimension_resize_endpoint"));
				Check(Document->GetEditController().selectedDimension()->end == f101::cad::Point2D{3504,1021}, TEXT("dimension_resized_exact_points"));
				Check(Document->Edit([](f101::cad::EditController& E) { return E.setDimensionText("Paso libre"); }).ok(), TEXT("dimension_label"));
				const auto Automatic = Document->GetPlanDrawing().dimensions.front();
				Check(Document->Edit([Automatic](f101::cad::EditController& E) { return E.selectDimension(Automatic.id); }).ok(), TEXT("dimension_select_automatic"));
				std::array<f101::cad::PlanPoint, 4> Handles;
				f101::cad::dimensionHandles(Automatic, Handles);
				const auto Corner = Handles[3];
				Check(Document->Edit([Corner](f101::cad::EditController& E) { return E.pointerPressed({Corner.x_mm,Corner.y_mm,10000,0,0,-1}); }).ok(), TEXT("dimension_grab_automatic_corner"));
				Check(Document->Edit([Corner](f101::cad::EditController& E) { E.pointerMoved({Corner.x_mm + 125,Corner.y_mm,10000,0,0,-1}); return f101::Status::success(); }).ok() &&
					Document->Edit([Corner](f101::cad::EditController& E) { return E.pointerReleased({Corner.x_mm + 125,Corner.y_mm,10000,0,0,-1}); }).ok(), TEXT("dimension_resize_automatic_corner"));
				Check(Document->GetEditController().selectedDimension() && !Document->GetEditController().selectedDimension()->automatic, TEXT("dimension_converted_manual"));
				Check(Document->Edit([](f101::cad::EditController& E) { return E.undo(); }).ok() && Document->GetDocument().project().dimensions.size() == 1, TEXT("dimension_conversion_undo"));
				Check(Document->Edit([Automatic](f101::cad::EditController& E) { return E.selectDimension(Automatic.id); }).ok(), TEXT("dimension_select_restored_automatic"));
				Check(Document->Edit([](f101::cad::EditController& E) { return E.setDimensionOffset("-150 cm"); }).ok(), TEXT("dimension_edit_automatic"));
				Check(Document->Edit([](f101::cad::EditController& E) { return E.deleteSelection(); }).ok(), TEXT("dimension_delete_automatic"));
				Check(Document->Edit([](f101::cad::EditController& E) { return E.undo(); }).ok(), TEXT("dimension_undo"));
				Check(Document->Edit([](f101::cad::EditController& E) { return E.redo(); }).ok(), TEXT("dimension_redo"));
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
				const auto* HiddenDimension = Document->GetDocument().project().findDimension(Automatic.id);
				Check(HiddenDimension && HiddenDimension->automatic && HiddenDimension->hidden && HiddenDimension->offsetMm == -1500 &&
					Document->GetDocument().project().dimensions.size() == 2, TEXT("dimensions_reopen"));
				Check(Document->ExportPlan(FPaths::Combine(Directory, TEXT("plan.svg")), {}).ok(), TEXT("export_svg"));
				FString NamedSvg;
				Check(FFileHelper::LoadFileToString(NamedSvg, *FPaths::Combine(Directory, TEXT("plan.svg"))) && NamedSvg.Contains(TEXT("Cocina")), TEXT("room_name_svg"));
				Check(NamedSvg.Contains(TEXT("Paso libre")), TEXT("dimension_svg"));
				Check(Document->ExportPlan(FPaths::Combine(Directory, TEXT("plan.png")), {}).ok(), TEXT("export_png"));
				Check(Document->ExportGltf(FPaths::Combine(Directory, TEXT("scene.glb"))).ok(), TEXT("export_glb"));
				Viewport->SetPlanView(false);
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
