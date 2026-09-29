# Unreal MCP

MCP es la mano de Claude dentro de Unreal Editor, **no** el cerebro de 101st Floor. Sirve para inspeccionar,
automatizar y validar durante el desarrollo. Ninguna lógica de producto depende de él y el CAD Core no lo conoce.
Si MCP cambia o desaparece, el producto sigue funcionando.

El servidor MCP vive **dentro del proceso de Unreal Editor**: solo existe mientras el editor está abierto con el
servidor iniciado. No es una API del runtime empaquetado.

## Piezas

| Pieza | Dónde | Estado |
|---|---|---|
| Plugin `ModelContextProtocol` (servidor, Experimental) | `Floor101.uproject`, solo target **Editor** | habilitado |
| Plugin `AllToolsets` (agrega todos los toolsets) | `Floor101.uproject`, solo target **Editor** | habilitado |
| Plugin `LiveCodingToolset` (no incluido en `AllToolsets`) | `Floor101.uproject`, solo target **Editor** | habilitado |
| Plugin de Claude Code `unreal-engine-skills-for-claude-code` (Epic) | `.claude/settings.json` (`enabledPlugins`, alcance proyecto) | v3.1.1 |
| Config de cliente MCP | `.mcp.json` junto al `.uproject` (build del Launcher) | generado con el comando oficial |
| Arranque reproducible | `Scripts/RunEditorMcp.ps1` | |
| Prueba de humo | `Scripts/CheckMcp.ps1` | |

`TargetAllowList: ["Editor"]` impide que el servidor MCP (que tiene módulos Runtime) entre en un build del juego.
`AllToolsets` es cómodo para el MVP; si seguridad o rendimiento lo justifican, sustituirlo por toolsets concretos.

## Uso

```powershell
.\Scripts\RunEditorMcp.ps1                        # abre el editor con -ModelContextProtocolStartServer y espera al MCP
.\Scripts\RunEditorMcp.ps1 -GenerateClientConfig  # además ejecuta ModelContextProtocol.GenerateClientConfig ClaudeCode
.\Scripts\CheckMcp.ps1                            # initialize + tools/list + list_toolsets + operación read-only
```

`RunEditorMcp.ps1` usa los argumentos de línea de comandos de UE 5.8 (`-ModelContextProtocolStartServer`,
`-ModelContextProtocolPort=<n>`) en vez de `bAutoStartServer`, que vive en
`Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` (por usuario, no versionado). Si ya hay un editor de
este proyecto arrancando, lo espera en lugar de abrir otro. Al final comprueba que el puerto solo escucha en loopback.

La primera vez Claude Code pide aprobar el servidor `unreal-mcp` del proyecto (decisión del usuario: da control
amplio sobre el editor). Una vez aprobado, las herramientas `mcp__unreal-mcp__*` aparecen en la sesión y, si el editor
se reinicia, el cliente se reconecta en la siguiente llamada. Con el editor cerrado las llamadas fallan con
`ECONNREFUSED`: relanzar con `RunEditorMcp.ps1`.

### Build del Launcher vs. build desde código fuente
`ModelContextProtocol.GenerateClientConfig` escribe `.mcp.json` en `FPaths::ProjectDir()` cuando el motor es una
instalación (`FApp::IsEngineInstalled()`, caso actual) y en la raíz del workspace (junto a `Engine/`) en una build
desde código fuente (`ModelContextProtocolClientConfig.cpp`). `Scripts/Common.ps1` detecta el tipo de instalación.

## Herramientas
Con *tool search* activo (default, `bEnableToolSearch=true`) el servidor expone solo tres meta-herramientas:
`list_toolsets`, `describe_toolset` y `call_tool`. Flujo: descubrir el toolset → leer su esquema → invocar con
`call_tool` → comprobar el resultado.

## Toolset propio: `Floor101Editor.Floor101DocumentToolset`
Paredes del documento de 101st Floor: `GetWalls`, `AddWall`, `SetWallEnd`, `SetWallLength`, `SetWallThickness`,
`MoveCorner` (mueve todos los extremos de una esquina), `GetRooms` (habitaciones con área neta en m², muros y suelo),
`SetWallHeight`, `RemoveWall`, `Undo`/`Redo` (devuelven la etiqueta, p. ej. `Create wall`) y `SaveProject`/
`OpenProject` (rutas absolutas), con coordenadas y dimensiones en mm enteros. **Mundo objetivo:** el de la sesión PIE
mientras exista (lo que el usuario está editando; empieza vacío), si no el del editor. Cada cambio es un paso
deshacible y regenera el actor; el resultado incluye su referencia para inspeccionarlo con `ActorTools`
(`get_actor_bounds`, `get_actor_transform`, `get_components`) o `SceneTools.find_actors` por la etiqueta
`f101.node:<wallId>/body`. Es una capa fina sobre la API del documento: no contiene lógica de producto.

Verificado (MVP 1): pared en el mundo del editor → `EditorAppToolset.StartPIE` → `GetWalls` vacío (documento de PIE)
→ `AddWall` con actor en `UEDPIE_0_…` → `Undo`/`Redo` (`Create wall`) → `StopPIE` → el documento del editor conserva su
pared.

Verificado (MVP 2, por el transporte MCP con `Scripts/McpClient.ps1`): 4 `AddWall` → `GetRooms` = 1 habitación de
11,1744 m² con su suelo → `MoveCorner(4000,0 → 4500,0)` mueve 2 muros y el área pasa a 11,8920 m² con la misma clave →
`Undo` = `Move corner` → 11,1744 m².

## Particularidades observadas (UE 5.8.3)
- **Consola:** ningún toolset de Epic ejecuta comandos de consola (`EditorAppToolset` solo lee cvars;
  `ProgrammaticToolset` es un sandbox que orquesta herramientas). Por eso existe el toolset propio.
- **`CaptureViewport`:** exige `captureTransform` y `annotations` aunque su esquema los marca como opcionales. Devuelve
  la imagen en base64 dentro del JSON (cientos de KB): hay que capturarla con `Scripts/McpClient.ps1` y guardarla en un
  fichero, en lugar de pasarla por el contexto del agente.
- **`AutomationTestToolset.RunTestsByFilter`:** si los tests provocan a propósito errores de script de un toolset
  (tests de errores), la llamada MCP devuelve esos mensajes como error aunque los tests pasen, porque el registro los
  captura. El resultado real está en `GetTestResults`.
- **`FocusOnActors`:** en una pared alineada con X deja la cámara mirándola de canto; para capturas se usa una pose
  explícita en `captureTransform`.

## Serialización (política del proyecto)
Las llamadas MCP a Unreal se hacen **siempre en serie**, nunca solapadas, aunque parezcan independientes.
El servidor atiende cada petición en el game thread, pero las herramientas asíncronas pueden solaparse y el orden de
las peticiones no garantiza el de ejecución. La skill de Epic (v3.1.1, commit "Permit safe concurrent tool calls")
permite paralelizar trabajo independiente; el prompt maestro de 101st Floor es más estricto y prevalece.
`Scripts/McpClient.ps1` es secuencial por construcción.

## Seguridad
- El servidor HTTP del motor escucha por defecto en loopback (`FHttpServerListenerConfig::BindAddress = "localhost"`)
  y valida `Origin` contra DNS rebinding. `RunEditorMcp.ps1` falla si detecta otra dirección.
- `localhost` **no** es una frontera frente a otros procesos del mismo usuario.
- `AllToolsets` incluye ejecución de Python arbitrario en el editor (`execute_tool_script`) y herramientas que
  mutan/borran assets. Commit (o punto de recuperación) antes de sesiones MCP largas o mutativas.
- No usar Claude Code con `--dangerously-skip-permissions` mientras el plugin esté cargado, salvo entorno desechable.
- La build del Launcher no incluye el proxy opcional (`Extras/Proxy`); se usa conexión HTTP directa.

## Hook `SessionStart` del plugin
`bash ${CLAUDE_PLUGIN_ROOT}/hooks/unreal-context.sh`. Claude Code ejecuta los hooks con Git Bash en Windows (el
`bash` de PowerShell es WSL y no sirve). Verificado ejecutándolo con Git Bash 5.3: devuelve el contexto de proyecto
Unreal (`Floor101.uproject`) y detecta si falta `.mcp.json`.

## Compilar con el editor abierto
UnrealBuildTool se niega a compilar mientras hay un editor con Live Coding activo
(`Unable to build while Live Coding is active`); `Build.ps1` lo detecta antes y lo explica. Con el editor abierto se
compila con Live Coding vía MCP: `LiveCodingToolset.LiveCodingToolset` → `CompileLiveCoding` (bloquea hasta terminar y
devuelve los diagnósticos de MSVC). Live Coding sirve para cambios de implementación; cambios de cabeceras con
reflexión (`UCLASS`, `UPROPERTY`) o de módulos requieren cerrar el editor y `Build.ps1`.

## Efectos secundarios del editor
- Al abrirse, el editor escribía en `Config/DefaultEngine.ini` (versionado) una sección de **Android File Server** con
  un `SecurityToken` aleatorio. El proyecto es solo Windows: el plugin `AndroidFileServer` está deshabilitado en el
  `.uproject` y la sección se eliminó.
- El editor añade a `Config/DefaultGame.ini` la sección `AssetManagerSettings` (tipos primarios por defecto más
  `GameFeatureData`, porque `AllToolsets` habilita GameFeatures). Es determinista y sin secretos: se versiona tal cual.
- El editor puede reescribir `.ini` con finales de línea CRLF; `.gitattributes` los normaliza (sin cambio de contenido).
- Cada arranque del editor registra 14 `LogAutomationTest: Error: Condition failed` en el frame 0. Vienen de un test
  experimental del motor (`Runtime/Core/Tests/Experimental/UnifiedError/UnifiedErrorTests.cpp`), idénticos en todos
  los arranques y ausentes en `-game`. Ruido conocido del motor, no del proyecto.
- `EditorStartupMap` es un mapa de plantilla del motor: el editor abre una copia sin guardar (`/Temp/Untitled_1`).

## Resultados (2026-09-28)

Flujo reproducible verificado de principio a fin: `Build.ps1` → `RunEditorMcp.ps1` → `CheckMcp.ps1`.

| Comprobación | Resultado |
|---|---|
| Plugins | `ModelContextProtocol` + `AllToolsets` + `LiveCodingToolset` cargados; 56 toolsets registrados |
| Arranque | `RunEditorMcp.ps1`: MCP listo en 18–30 s (`LogModelContextProtocol: Starting MCP server on port 8000`) |
| Enlace de red | solo `127.0.0.1:8000` (`netstat`/`Get-NetTCPConnection`) |
| Config de cliente | `ModelContextProtocol.GenerateClientConfig ClaudeCode` → `.mcp.json` junto al `.uproject` (build del Launcher) |
| Handshake | MCP `2025-06-18`, `Mcp-Session-Id` emitido, sesión cerrada con `DELETE` |
| `tools/list` | `list_toolsets`, `describe_toolset`, `call_tool` (tool search activo) |
| Operación read-only | `SceneTools.get_current_level` → `/Temp/Untitled_1`; `SceneTools.find_actors` → 15 actores (~0,3 s por llamada) |
| Live Coding vía MCP | `CompileLiveCoding` → `Result: NoChanges` (3,5 s) |
| Claude Code | servidor aprobado por el usuario; `mcp__unreal-mcp__call_tool` → `get_current_level` = `/Temp/Untitled_1`, `find_actors` = 15 actores (llamadas en serie). Tras reiniciar el editor, Claude Code se reconecta solo |
| Plugin Claude Code | v3.1.1, alcance proyecto; hook `SessionStart` ejecutado con Git Bash → contexto `Floor101.uproject` |
| Render tras el cambio | `CheckRendering.ps1` 14/14 (sin regresión) |

### Validación MVP 0 por MCP (2026-09-28, en serie desde Claude Code)
| Paso | Herramienta | Resultado |
|---|---|---|
| Crear la pared de referencia | `Floor101DocumentToolset.AddWall(0,0,3550,0,120,2400)` | UUID v4 `7a942f96-…`, actor `Floor101SceneNodeActor_0` |
| Bounds | `ActorTools.get_actor_bounds` | min (0, −6, 0), max (355, 6, 240) cm |
| Transform | `ActorTools.get_actor_transform` | origen, rotación 0, escala 1 |
| Componentes | `ActorTools.get_components` | `Mesh` (`UDynamicMeshComponent`) |
| Identidad | `SceneTools.find_actors(tag=f101.node:<id>/body)` | el mismo actor |
| 3550 → 4200 mm | `Floor101DocumentToolset.SetWallEnd` | mismo id y actor; bounds máx. (420, 6, 240) cm; escala 1 |
| Logs | `LogsToolset.GetLogEntries(LogFloor101)` | `scene resync in 0.912 ms (created 1)` → `0.212 ms (updated 1)` |
| Automation Tests | `AutomationTestToolset.RunTestsByFilter("StartsWith:Floor101.")` + `GetTestResults` | 8/8 pasan |
| Imagen | `EditorAppToolset.CaptureViewport` (vía `McpClient.ps1`) | pared visible y soleada, con sombra VSM |
