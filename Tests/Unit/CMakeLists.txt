# Unit tests of the pure C++ modules, built without Unreal (ADR-0002). Run through Scripts/Test.ps1.
cmake_minimum_required(VERSION 3.28)
project(Floor101UnitTests LANGUAGES CXX)

if(NOT MSVC)
	message(FATAL_ERROR "The unit tests mirror UnrealBuildTool's MSVC settings; configure them with MSVC.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(F101_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Same code generation as UnrealBuildTool for modules without exceptions or RTTI (VCToolChain.cs):
# no /EHsc, _HAS_EXCEPTIONS=0, /GR-. Stricter warnings than Unreal so problems surface here first.
string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "/GR( |$)" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
add_compile_options(/W4 /WX /permissive- /utf-8 /GR-)
add_compile_definitions(_HAS_EXCEPTIONS=0)

# A pure module = Source/<Name>/Private/*.cpp minus the Unreal registration file (*Module.cpp).
function(f101_pure_module name)
	file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${F101_ROOT}/Source/${name}/Private/*.cpp")
	list(FILTER sources EXCLUDE REGEX "Module\\.cpp$")
	add_library(${name} STATIC ${sources})
	target_include_directories(${name} PUBLIC "${F101_ROOT}/Source/${name}/Public")
endfunction()

f101_pure_module(Floor101CadCore)
target_include_directories(Floor101CadCore SYSTEM PRIVATE "${F101_ROOT}/ThirdParty") # nlohmann/json
f101_pure_module(Floor101Geometry)
target_link_libraries(Floor101Geometry PUBLIC Floor101CadCore)
f101_pure_module(Floor101SceneModel)
target_link_libraries(Floor101SceneModel PUBLIC Floor101Geometry)
f101_pure_module(Floor101Drawing)
target_link_libraries(Floor101Drawing PUBLIC Floor101Geometry)
f101_pure_module(Floor101Assets)
target_link_libraries(Floor101Assets PUBLIC Floor101Geometry)
target_include_directories(Floor101Assets SYSTEM PRIVATE "${F101_ROOT}/ThirdParty") # nlohmann/json

add_executable(Floor101UnitTests
	Main.cpp
	UnitsTests.cpp
	IdTests.cpp
	Geometry2DTests.cpp
	WallTests.cpp
	WallGeometryTests.cpp
	SceneTests.cpp
	CommandTests.cpp
	ProjectFileTests.cpp
	SnappingTests.cpp
	WallToolTests.cpp
	PickingTests.cpp
	EditControllerTests.cpp
	WallJoinTests.cpp
	PolygonGeometryTests.cpp
	RoomTests.cpp
	LevelTests.cpp
	FinishTests.cpp
	CarpentryTests.cpp
	OpeningTypeTests.cpp
	PlanDrawingTests.cpp
	FurnitureTests.cpp
	AssetTests.cpp
	AssetImportTests.cpp
)
target_include_directories(Floor101UnitTests SYSTEM PRIVATE "${F101_ROOT}/ThirdParty/doctest")
# No exceptions, like Unreal (a failed REQUIRE aborts the run). Real std headers instead of doctest's forward
# declarations: MSVC's string_view operator<< needs the complete std::ostream.
target_compile_definitions(Floor101UnitTests PRIVATE
	DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
	DOCTEST_CONFIG_USE_STD_HEADERS
)
target_link_libraries(Floor101UnitTests PRIVATE Floor101SceneModel Floor101Drawing Floor101Assets)
# Sample assets exported by Blender (Tests/Data/Assets/make_samples.py).
target_compile_definitions(Floor101UnitTests PRIVATE F101_TEST_DATA_DIR="${F101_ROOT}/Tests/Data")

enable_testing()
add_test(NAME Floor101UnitTests COMMAND Floor101UnitTests)
