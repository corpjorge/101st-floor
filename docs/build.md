# Build

## Toolchain verificado (2026-09-28)

| Componente | Versión | Notas |
|---|---|---|
| Windows | 11 Pro 25H2 x64 (26200) | |
| Unreal Engine | 5.8.3 (CL 58210709), instalación del Epic Games Launcher | `EngineAssociation: "5.8"` |
| Visual Studio | 2026 Community 18.9.3 | + Build Tools 2026 (no requerido) |
| MSVC | **14.50.35738** (seleccionado por UBT) + 14.51.36257 | ver "Selección de MSVC" |
| Windows SDK | 10.0.26100.0 | Turnkey `VerifySdk`: Valid (UE 5.8 admite 10.0.19041–10.9.x) |
| .NET | incluido en el motor (10.0) | no hace falta instalar SDK aparte |

Componentes de VS recomendados por UE 5.8 (`Engine/Config/Windows/Windows_SDK.json`) e instalados:
`VC.14.50.18.0.x86.x64`, `VC.14.50.18.0.ATL`, `Workload.NativeGame`, `Component.Unreal.Ide`,
`Component.Unreal.Debugger`, `Net.Component.4.6.2.TargetingPack`. Clang no es necesario.

### Selección de MSVC
UE 5.8.3 declara como preferidas las familias 14.50 (≥ 14.50.35723) y 14.44, y veta 14.50.0–14.50.35722.
UnrealBuildTool (`MicrosoftPlatformSDK.cs`) veta por la versión real de `cl.exe` y ordena por familia preferida,
por lo que con 14.50 instalado lo elige automáticamente, sin fijar versión en el proyecto. Con solo 14.51 compila,
pero UBT lo marca como "no probado a fondo". La carpeta del toolset se llama `14.50.35717` aunque `cl.exe` es
14.50.35738: es normal y no está vetado.

Cada build imprime la línea de toolchain elegida, p. ej.:
`Using Visual Studio 14.50.35738 toolchain (...\MSVC\14.50.35717) and Windows 10.0.26100.0 SDK`.

## Localización del motor
`Scripts/Common.ps1` → `Find-UnrealEngine`, sin rutas fijas:
1. `$env:UE_ROOT` si está definido (override manual).
2. Si `EngineAssociation` es un GUID (build desde código fuente): `HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds`.
3. Si es una versión (`5.8`): `%ProgramData%\Epic\UnrealEngineLauncher\LauncherInstalled.dat` (`AppName = UE_5.8`).

Valida `Engine/Build/Build.version` contra la asociación del proyecto.

## Comandos
```powershell
.\Scripts\Setup.ps1                                   # comprueba el entorno (no instala nada)
.\Scripts\Build.ps1                                   # Floor101Editor Win64 Development (UBT)
.\Scripts\Test.ps1                                    # arquitectura + unitarios (CMake, sin Unreal)
.\Scripts\Test.ps1 -Integration                       # + Unreal Automation en editor headless
.\Scripts\Run.ps1                                     # app (-game) con modo/resolución guardados y pared de referencia
.\Scripts\CheckRendering.ps1 [-Demo]                  # validación de render (ver rendering-rtx.md)
.\Scripts\RunEditorMcp.ps1 ; .\Scripts\CheckMcp.ps1   # editor + MCP (ver unreal-mcp.md)
.\Scripts\GenerateAssets.ps1                          # regenera Content/Floor101 (materiales de acabados y vidrio) desde código
.\Scripts\GenerateLibrary.ps1 [-Only id,…]            # regenera los muebles de Library/furniture con Blender (opcional)
.\Scripts\CheckGltf.ps1 -Path <archivo.glb>           # segunda opinión de una exportación para Twinmotion, con Blender (opcional)
.\Scripts\Clean.ps1 [-IncludeSaved]                   # borra Binaries, Intermediate y DerivedDataCache
```
`Package.ps1` compila y cocina Windows Shipping, incluye `Library/`, splash y prerrequisitos.
`CheckPackage.ps1` valida el ejecutable archivado; `Installer.ps1` genera el instalador. Ver [distribución](distribution.md) (ADR-0028).

### Biblioteca integrada (`Library/`)
Muebles (`.glb` + imagen `.png`) y texturas fotográficas (`.jpg`) con su `manifest.json` (ADR-0019), versionados en Git
LFS. Los muebles salen de `Library/Source/make_furniture.py`: `.\Scripts\GenerateLibrary.ps1` lo ejecuta en Blender 4.2 o
posterior sin interfaz (el más nuevo de `C:\Program Files\Blender Foundation`, o `-Blender`) y reescribe los archivos y
el manifiesto; Blender solo hace falta para regenerarlos, no para compilar ni ejecutar. Las texturas son CC0 de Poly
Haven (autor en el manifiesto). La aplicación lee la carpeta de `<proyecto>/Library` y los tests unitarios la comprueban.

### Assets generados (`Content/`)
`Content/` solo tiene lo que genera `.\Scripts\GenerateAssets.ps1` (commandlet `Floor101GenerateAssets`, editor
headless): hoy `/Game/Floor101/Materials/M_Floor101Finish`, el material base de los acabados (ADR-0012), y
`M_Floor101Glass`, el vidrio translúcido (ADR-0013). Se versionan en
Git (LFS) y se regenera tras cambiar el commandlet, con el editor cerrado y después de `Build.ps1`. Se cocina siempre
(`DirectoriesToAlwaysCook` en `Config/DefaultGame.ini`).

### Tests unitarios (CMake)
`Tests/Unit/CMakeLists.txt` compila los módulos puros como librerías estáticas con los mismos flags de generación de
código que UBT (sin `/EHsc`, `_HAS_EXCEPTIONS=0`, `/GR-`) y más avisos (`/W4 /WX /permissive-`). doctest en modo sin
excepciones (`DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS`) y con cabeceras estándar reales
(`DOCTEST_CONFIG_USE_STD_HEADERS`, que MSVC 14.50 necesita). Build en `Intermediate/UnitTests/<Config>`. El entorno
MSVC 14.50 se carga con `vcvars64 -vcvars_ver=14.50` solo durante el comando (`Invoke-WithMsvcEnvironment`).

PowerShell parte los argumentos nativos con punto (`-X=127.0.0.1`, `-D...=cl.exe`): en los scripts van entre comillas.
`Build.ps1` llama a `Engine/Build/BatchFiles/Build.bat` (UnrealBuildTool). Log completo de UBT:
`%LOCALAPPDATA%\UnrealBuildTool\Log.txt`. Requiere el editor cerrado (UBT no compila con Live Coding activo);
con el editor abierto, compilar con Live Coding vía MCP (ver `docs/unreal-mcp.md`).

Unreal Build Accelerator se ejecuta en modo local (`-UBADisableRemote -UBAHost=127.0.0.1`): no hay agentes
remotos y así su servidor no escucha en todas las interfaces de red.

## Estructura
```
Floor101.uproject
Source/Floor101CadCore/         C++ puro: modelo CAD (docs/cad-core.md)
Source/Floor101Geometry/        C++ puro: geometría derivada (docs/geometry.md)
Source/Floor101SceneModel/      C++ puro: escena neutral (docs/scene-model.md)
Source/Floor101Drawing/         C++ puro: planos 2D con cotas y su SVG (docs/drawing.md)
Source/Floor101UnrealAdapter/   escena → actores Unreal (docs/unreal-integration.md)
Source/Floor101/                aplicación: documento y edición, entrada, cámara, UI Slate, RenderCheck
Source/Floor101Editor/          solo editor: toolset MCP
Tests/Unit/                     tests unitarios CMake + doctest
ThirdParty/doctest/             doctest 2.5.3 (MIT)
ThirdParty/nlohmann/            nlohmann/json 3.12.0 (MIT), solo dentro del CAD Core (project.json)
Config/Default*.ini             configuración versionada (render en DefaultEngine.ini)
Scripts/                        PowerShell 7
```
`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/` y los ficheros de solución generados no se versionan.

## Notas del entorno
- `bash` desde PowerShell resuelve a WSL (`C:\Windows\System32\bash.exe`, distro `docker-desktop`); los scripts no
  lo usan. Claude Code ejecuta sus hooks con Git Bash, donde `bash` es el de Git.
- Git: `core.longpaths=true`; binarios de Unreal y arte fuente en Git LFS (`.gitattributes`).
- CPU i7-9700K (8 hilos) y SSD SATA: la compilación de shaders es el cuello de botella de la primera ejecución.
- Si el juego o el editor fallan, `CrashReportClientEditor.exe` puede quedarse con `UnrealEditor-Floor101.dll` abierta y
  el siguiente `Build.ps1` falla al enlazar (LNK1104): cerrar ese proceso primero.
