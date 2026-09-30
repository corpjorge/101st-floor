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
`AddOpening`/`SetOpening`/`RemoveOpening` (puertas y ventanas: posición desde el inicio del muro, ancho, alto,
antepecho; `GetWalls` las lista en `Openings`; fallan si el hueco pisa la unión con otro muro, ADR-0013),
`SetOpeningFinish` (acabado de la carpintería; `GetFinishes` marca los válidos con `bForCarpentry` e informa `Opacity`),
`SetOpeningOperation` (cómo abre, ADR-0014: operación, bisagras, cara y apertura; los huecos lo informan),
`ExportPlan` (plano 2D de una planta a .svg o .png, ADR-0015; el PNG necesita renderer), `GetFurnitureTypes`/`GetFurniture`/
`AddFurniture`/`SetFurniture`/`SetFurnitureFinish`/`RemoveFurniture` (muebles, ADR-0016: catálogo con medidas y rangos; alta
exacta, sin snapping, con planta, centro, giro en centésimas de grado y medidas, 0 = las del tipo; acabados con
`bForFurniture`), `GetLevels`/`AddLevel`/`SetLevel`/`RemoveLevel` (plantas: cota derivada,
altura, forjado; al cambiar la altura, los muros a altura completa la siguen) y `SetLevelView` (qué planta muestra el
mundo y si se ven todas; los muros de plantas ocultas no tienen actor). `AddWall` y `MoveCorner` reciben `LevelId`
(vacío = la primera planta); muros y habitaciones informan su `LevelId`. `GetFinishes`/`SetWallFinish`/`SetRoomFinish`
(acabados, ADR-0012: el catálogo; una cara de muro, izquierda o derecha de su eje; suelo y techo de una habitación por
su clave) y sus acabados en `GetWalls` y `GetRooms`,
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

Verificado (MVP 3, por el transporte MCP): `OpenProject` de un archivo de esquema 2 con una ventana → `AddOpening`
(puerta de 900 × 2100 en 300 mm) → otra puerta en 2000 mm rechazada con "The door overlaps another opening" →
`SetOpening` (ventana a 1500 mm de ancho, antepecho 1000) → `RemoveOpening` → `Undo` × 2 (`Delete opening`,
`Change opening`) → la ventana vuelve a 1300 × 1200 con antepecho 900.

Verificado (MVP 4, por el transporte MCP): `GetLevels` → planta baja (0, 2800, 250) → `AddLevel("Primera", 3000, 300)`
en cota 2800 → 4 `AddWall` en cada planta → `GetRooms` = 2 habitaciones de 11,1744 m², cada una con su `LevelId` (la de
arriba sin actor mientras la vista la oculta) → `SetLevel` de la planta baja a 3000: sus 4 muros pasan a 3000 y
"Primera" sube a 3000 → `Undo` = `Change level` → `RemoveLevel` de la planta baja (con sus muros) → borrar la única
planta que queda se rechaza → `Undo` = `Delete level` devuelve los 8 muros.

Verificado (MVP 5, por el transporte MCP): `GetFinishes` → 11 acabados con sus superficies → `SetWallFinish(Left,
"brick-red")` → poner `wood-oak` en una cara se rechaza ("The finish 'wood-oak' is not for walls") → `SetRoomFinish`
(`tile-grey`, `concrete`) visible en `GetRooms` → `Undo` × 2 (`Change room finish`, `Change finish`) → `SaveProject`
con `"right": "paint-sage"` en el muro.

Verificado (MVP 6, por el transporte MCP): `GetFinishes` → carpintería `wood-oak`, `wood-walnut`, `lacquer-white`,
`aluminium-anthracite`; translúcido `glass-clear` (0,18) → muro de 6 m con otro en T a 2 m → `AddOpening` de una
puerta en 1500 mm rechazada ("The door runs into a wall joined to this one (1950 to 2050 mm along it)") → puerta en
3000 mm → `SetOpeningFinish("wood-oak")`; `brick-red` rechazado ("is not for doors and window frames") → `Undo` =
`Change opening` (acabado vacío otra vez) → `SaveProject` con esquema 5 y `"finish": "aluminium-anthracite"` → 3
actores (2 muros y la carpintería) → `RemoveWall` × 2.

Verificado (MVP 7, por el transporte MCP): `OpenProject` de un archivo de esquema 6 → los 6 huecos con su operación,
bisagras, cara y apertura; 2 actores de símbolo (las dos puertas abatibles) → `AddOpening` (abatible, bisagras al
inicio, cerrada) junto a una T → `SetOpeningOperation(Fixed)` rechazado ("A door cannot be fixed"); corredera hacia la
T rechazada ("The sliding door's pocket runs into a wall joined to this one (1950 to 2050 mm along it)"); corredera
hacia el otro lado aceptada → `Undo` = `Change opening` → `SaveProject` con esquema 6. `CaptureViewport` mostró el arco
de una puerta abierta hacia fuera sobre el suelo exterior.

Verificado (MVP 8, por el transporte MCP): `AutomationTestToolset.RunTests(["Floor101.Interaction.PlanView", …])` dentro
del editor real (con renderer: el PNG se pinta y su corte sale en #262626) → `Success`; `OpenProject` de la casa del
MVP 7 → `ExportPlan` a .svg (17 KB) y .png (1994 × 1794) → .pdf rechazado ("Export the plan as .svg or .png").
`LiveCodingToolset.CompileLiveCoding` sirvió para probar un cambio del subsistema sin cerrar el editor.

Verificado (MVP 9, por el transporte MCP): 5 muros y una puerta → `GetFurnitureTypes` (12 tipos) → `AddFurniture` × 14
(cama doble, armario, sofá, mesa, 4 sillas, cocina, frigorífico, bañera, inodoro, lavabo, plato de ducha) → 14 con actor →
`SetFurnitureFinish` (tela arena, nogal) y `SetFurniture` (mesa a 180 cm) → `brick-red` rechazado ("The finish
'brick-red' is not for furniture") → `Undo`/`Redo` = `Change furniture` → `SaveProject` con esquema 7 y los 14 tipos →
`ExportPlan` .svg (14 KB) y .png → `CaptureViewport` del piso amueblado con Lumen; `RunTests` de
`Floor101.Interaction.Furniture`, `PlanView` y `KeyMap` dentro del editor real → `Success`.

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
