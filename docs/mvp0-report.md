# Informe MVP 0 (Fase G) — 2026-09-28

**Estado:** MVP 0 completo (Fases A–F). Código en `main` (Fase E: commit `5e4d372`). La arquitectura crítica está
demostrada de principio a fin: modelo CAD en milímetros → geometría → escena neutral → Unreal con Lumen por hardware,
con tests sin Unreal, tests de integración, validación por MCP y validación de la ruta RTX. No se ha empezado MVP 1.

## Versiones

| Componente | Versión |
|---|---|
| Windows | 11 Pro 25H2, build 26200, x64 |
| Unreal Engine | 5.8.3 (CL 58210709), instalación del Epic Games Launcher |
| Visual Studio / MSVC | VS 2026 18.9.3; **MSVC 14.50.35738** (elegido por UBT); 14.51 también instalado |
| Windows SDK | 10.0.26100.0 |
| CMake / Ninja | 4.3.1-msvc1 (de Visual Studio) |
| doctest | 2.5.3 (en `ThirdParty/`) |
| PowerShell / Git | 7.6.6 / 2.55 + LFS 3.7.1 |
| Claude Code / plugin de Epic | 2.1.246 / `unreal-engine-skills-for-claude-code` 3.1.1 (alcance proyecto) |
| Herramientas externas | Blender 5.2.0 LTS; Twinmotion 2026.1 HF2 (base; 2026.2 es Preview); RealityScan 2.2 |

## GPU, RHI, ray tracing y Lumen (verificado en ejecución)

| | Resultado |
|---|---|
| GPU | NVIDIA GeForce RTX 5070 Ti, 16 GB, driver 617.14, adaptador 0 |
| RHI | D3D12, feature level SM6, shader platform `PCD3D_SM6`, Agility SDK; sin fallback (D3D11 eliminado) |
| Capacidades | DXR 1.1 + bindless, ray tracing shaders + inline, SM6.6 atomic64, mesh shaders; NVIDIA Cluster Operations y Extended RTPSO activos |
| Ray tracing | `Ray tracing is enabled (dynamic)` |
| Lumen | GI y reflexiones con HWRT: 9 pases de ray tracing por hardware en ProfileGPU (8 async compute + 1 graphics) |
| Pared en Lumen | la surface cache pasa de 1/6 a **2/12** mesh cards/cards al añadir la pared (ADR-0005) |
| VSM / Nanite / AA | activos / activo / TSR (resolución interna automática 1400×788 → 1080p) |
| Rendimiento (escena trivial) | GPU 3,55 ms media, p95 3,98 ms; frame 4,96 ms @ 1920×1080 |

## Estructura

```
Source/Floor101CadCore        C++ puro: unidades, primitivas 2D, IDs, Wall, Project, Document
Source/Floor101Geometry       C++ puro: MeshData, WallGeometryBuilder
Source/Floor101SceneModel     C++ puro: Scene, SceneNode, compileScene (IDs deterministas)
Source/Floor101UnrealAdapter  escena → actores transitorios con UDynamicMeshComponent
Source/Floor101               aplicación: UFloor101DocumentSubsystem, f101.Demo.*, RenderCheck
Source/Floor101Editor         solo editor: toolset MCP Floor101DocumentToolset
Tests/Unit                    doctest + CMake (sin Unreal)
Scripts                       Setup, Build, Test, Run, Clean, CheckRendering, RunEditorMcp, CheckMcp
```

## Comandos

```powershell
git clone https://github.com/corpjorge/101st-floor.git
.\Scripts\Setup.ps1 ; .\Scripts\Build.ps1 ; .\Scripts\Test.ps1 -Integration ; .\Scripts\Run.ps1
.\Scripts\CheckRendering.ps1 -Demo
.\Scripts\RunEditorMcp.ps1 ; .\Scripts\CheckMcp.ps1
```

## Tests y validación (Fase F)

| Validación | Resultado |
|---|---|
| **Clon limpio desde GitHub** (`C:\Dev\f101-verify`) | Setup 11 PASS; Build desde cero 98 s, 52 acciones, 0 errores y 0 warnings; Test -Integration todo en verde |
| Arquitectura | módulos puros sin cabeceras ni tipos de Unreal; `Build.cs` limpios |
| Unitarios (sin Unreal) | 34 casos, 2293 aserciones |
| Integración Unreal | 8/8: conversión, pared de referencia (bounds, 0 triángulos invertidos, regeneración), toolset (ciclo de vida + 5 errores) |
| Render RTX con la pared | todo PASS (`CheckRendering.ps1 -Demo`) |
| MCP | `CheckMcp.ps1` 6/6 (57 toolsets, solo loopback); recorrido 3550 → 4200 mm desde Claude Code: bounds 355 → 420 cm, mismo actor, escala 1 |
| Automation por MCP | 8/8 (`RunTestsByFilter` + `GetTestResults`) |
| App | `Run.ps1`: juego en ventana con la pared cargada en 0,7 ms |

### Checklist del MVP 0 (sección 42 del prompt maestro)

| # | Objetivo | Evidencia |
|---|---|---|
| 1–5 | repo, Windows, RTX, DX12/SM6, UE 5.8.x | GitHub privado; Fase A; `Setup.ps1` |
| 6–7 | Floor101 y build | `Floor101.uproject`, `Build.ps1` |
| 8–9 | HWRT y Lumen HWRT | `DefaultEngine.ini`, `CheckRendering.ps1` |
| 10–11 | Unreal MCP y plugin de Epic | `.mcp.json`, `.claude/settings.json`, `CheckMcp.ps1` |
| 12–15 | CAD Core, mm, Point2D, Wall | `Floor101CadCore`, tests de unidades y pared |
| 16–18 | Geometry, Scene Model, Adapter | tres módulos más sus tests |
| 19–20 | generar la pared y mostrarla | `Run.ps1`, capturas del editor por MCP |
| 21 | tests | unitarios + integración + arquitectura |
| 22 | validar vía MCP | tabla anterior |
| 23 | ruta gráfica RTX | `CheckRendering.ps1 -Demo` |
| §43 | 3550 → 4200 mm regenerando, sin escalar | tests unitarios, de integración y MCP |
| §44 | 3550/120/2400 mm = 355/12/240 cm, ID persistente, MeshData, Scene | `Floor101.Adapter.*`, `Tests/Unit/*` (serialización: MVP 1 por decisión del usuario) |

## Problemas encontrados y cómo se resolvieron

| Problema | Causa | Solución |
|---|---|---|
| `-UBAHost=127.0.0.1` y `-DCMAKE_CXX_COMPILER=cl.exe` llegaban partidos | PowerShell parte los argumentos nativos con punto | entrecomillados; documentado |
| El servidor de UBA escuchaba en `0.0.0.0` | valor por defecto | `-UBADisableRemote -UBAHost=127.0.0.1` |
| El editor escribía un `SecurityToken` en `DefaultEngine.ini` | plugin Android File Server | plugin deshabilitado (proyecto solo Windows) |
| El ADR-0003 decía que había que invertir el orden de los triángulos | error mío | el código de GeometryCore (`edge2 × edge1`) demuestra lo contrario; corregido y cubierto por un test con `GetTriNormal` |
| La GI de Lumen parecía no usar HWRT | el parser solo leía la cola gráfica y Lumen GI corre en async compute | se analizan todas las colas de ProfileGPU |
| Un test de unidades fallaba | premisa falsa (`4.35 * 1000` es exacto en `double`) | se usa el caso real `1.005 m` |
| doctest no compilaba con MSVC 14.50 | `std::ostream` solo declarado | `DOCTEST_CONFIG_USE_STD_HEADERS` |
| `TUniquePtr` con tipo incompleto en un `UObject` | UHT genera un constructor en `.gen.cpp` | `TPimplPtr` |
| El test del adaptador no enlazaba | UBT no enlaza dependencias transitivas | el test construye el `Project` como dato: el adaptador sigue sin depender del CAD Core |
| Los tests de error del toolset no detectaban nada | `RaiseScriptError` necesita un frame de script | `FToolCallExceptionHandler::CaptureErrorsIn` (patrón de Epic) |
| MCP no puede ejecutar comandos de consola | ningún toolset lo ofrece | toolset propio `Floor101DocumentToolset` (solo editor) |

## Limitaciones conocidas

- Mapa de plantilla del motor (`Template_Default`); todavía no hay nivel propio del proyecto.
- Material por defecto del motor; aún no existe `ArchitecturalMaterial`.
- Nodos de escena con transform identidad (geometría en coordenadas de escena).
- Sin serialización ni undo/redo (MVP 1, decisión del usuario).
- Cards de Lumen por bounding box: exactas para paredes rectas; aproximadas con huecos o formas en L (plan en ADR-0005).
- Rendimiento medido solo con una escena trivial.
- Ruido del motor: 14 `LogAutomationTest` de `UnifiedErrorTests` al arrancar el editor y avisos de `LogEditorDataStorageUI` en `-game`.
- `Package.ps1` pendiente (no hay build distribuible todavía).
- El antivirus de Windows en tiempo real ralentiza las compilaciones (Dev Drive o exclusiones: decisión del usuario).

## Decisiones

ADR-0001 (mm enteros y parser exacto, cm por defecto), ADR-0002 (núcleo C++ puro con doble compilación UBT y CMake),
ADR-0003 (escena neutral con IDs deterministas y conversión de ejes por consumidor), ADR-0004 (solo DX12/SM6/RTX),
ADR-0005 (`UDynamicMeshComponent`). Decisiones menores: MSVC 14.50, doctest incluido en el repo, CMake de Visual
Studio, toolset MCP solo de editor, Substrate activo, sin formatos D3D11, UBA local, plugin Android File Server
deshabilitado.

## Siguiente paso recomendado: MVP 1 (no iniciado)

1. **En el CAD Core, sin Unreal:** comandos con undo/redo sobre la API de `Document` (`CreateWall`, `DeleteWall`,
   `MoveWallEndpoint`, `ChangeWallThickness`, `ChangeWallHeight`) y `project.json` versionado (schemaVersion, mm, IDs).
   Todo testeable en segundos.
2. **Snapping básico** en el CAD Core: rejilla, extremos y ángulos de 0/15/30/45/90°.
3. **Nivel propio del proyecto** generado por código (luz, cielo, suelo), sin depender del mapa de plantilla.
4. **Wall Tool en Unreal:** clic → preview (nodo de escena transitorio) → longitud visible → escribir `355` / `3.55 m`
   (el parser ya existe) → Enter → pared exacta. Selección y panel de propiedades en Slate.
