# Repository Guidelines

## Project Structure & Module Organization
101st Floor is a Windows architectural design application using Unreal Engine 5.8. `Source/` contains Unreal modules with `Public/` headers and `Private/` implementations. `Floor101CadCore`, `Floor101Geometry`, `Floor101SceneModel`, `Floor101Drawing`, `Floor101Assets`, and `Floor101Exporters` contain engine-independent logic; `Floor101`, `Floor101UnrealAdapter`, and `Floor101Editor` handle runtime interaction, rendering, and editor tooling.

Pure C++ tests live in `Tests/Unit/`; Unreal tests live in module `Private/Tests/` directories. Fixtures are in `Tests/Data/`. `Config/` stores Unreal settings, `Content/` generated Unreal assets, and `Library/` furniture and textures. Read `docs/architecture.md` and relevant `docs/adr/` decisions before substantial changes.

## Build, Test, and Development Commands
Run from the repository root in PowerShell 7 with Windows 11, UE 5.8.x, and Visual Studio 2026/MSVC 14.50 installed:

- `.\Scripts\Setup.ps1`: verify the development environment.
- `.\Scripts\Build.ps1`: build Floor101Editor for Win64 Development; close the editor first.
- `.\Scripts\Test.ps1`: check module boundaries and build/run standalone unit tests.
- `.\Scripts\Test.ps1 -Integration`: additionally run headless Unreal Automation tests after a current editor build.
- `.\Scripts\Run.ps1`: launch the application; add `-Demo:$false` for an empty document.
- `.\Scripts\CheckRendering.ps1`: verify the required DX12/SM6 and ray-tracing configuration.
- `.\Scripts\CheckHdr.ps1`: inspect monitor HDR; see `docs/hdr.md` for the runtime toggle check and EXR captures.
- `.\Scripts\GenerateAssets.ps1`: regenerate finish and glass materials.

## Coding Style & Architecture
Follow existing C++20 code: tabs for indentation, braces on separate lines, PascalCase types, and lowerCamelCase pure-module functions and fields. Preserve Unreal naming prefixes in engine-facing code. No repository-wide formatter or linter configuration is provided; unit builds enforce strict MSVC warnings as errors.

Keep pure modules free of Unreal types and product logic out of Blueprints. Persist dimensions as `int64_t` millimeters; quantize temporary floating-point calculations explicitly. The CAD model is authoritative; rendering and exports derive from it. Record significant architecture changes in an ADR. Keep asset generation reproducible through scripts.

## Testing Guidelines
Use doctest in `Tests/Unit/<Feature>Tests.cpp` with descriptive behavior-based test cases. Add regression coverage for changed behavior; use Unreal Automation for integration boundaries. No numeric coverage threshold is defined. Never disable tests or suppress warnings to obtain a passing result.

## Commit & Pull Request Guidelines
History uses concise descriptive subjects such as `Fix startup Python traceback...` and `MVP 13: columns and building services`; no mandatory Conventional Commits format is evident. Keep commits focused. PRs should describe behavior changes, list validation results, link relevant issues/ADRs, and include screenshots for visual changes. Use Git LFS for library binaries; exclude generated build outputs.
