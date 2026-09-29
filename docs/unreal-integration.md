# Integración con Unreal

## Unreal Adapter (`Source/Floor101UnrealAdapter`)
Única frontera entre la escena y Unreal (ADR-0005). Conoce la escena, nunca el modelo CAD.

- `Floor101SceneSpace.h` (público): mm ↔ cm y espejo de Y (dextrógiro ↔ levógiro) en ambos sentidos
  (`ToUnrealPosition`, `ToScenePosition`, `ToSceneDirection`), también para convertir el rayo del cursor.
- `Floor101UnrealConversion.h` (privado): `ToDynamicMesh` crea `FDynamicMesh3` con normales, UV, polygroups e ID de
  material por sección. **Conserva el orden de los triángulos**: el espejo y la convención de Unreal
  (`VectorUtil::Normal` = `edge2 × edge1`) se compensan.
- `AFloor101SceneNodeActor`: `UCLASS(Transient, NotPlaceable)`, raíz `UDynamicMeshComponent` con ray tracing activado,
  material por defecto `/Engine/BasicShapes/BasicShapeMaterial` (referencia dura, se cocina con el juego), etiqueta
  `f101.node:<SceneNodeId>`. Estilos de vista: `Selected` (naranja), `Preview` (azul) y `Handle` (amarillo) con una
  `UMaterialInstanceDynamic` por estilo sobre el parámetro `Color` del material por defecto. Muros, `JointFill` y
  suelos de habitación son nodos como cualquier otro.
- `FFloor101SceneReconciler`: por `SceneNodeId`, crea, actualiza (si cambió el `contentHash` **o** el estilo) o destruye
  actores. Nunca escala: la escala sigue en 1.

## Aplicación (`Source/Floor101`)
- `UFloor101DocumentSubsystem` (un `UWorldSubsystem` por mundo de juego, PIE o editor) posee el `Document` y su
  `EditController`. `Edit(acción)` ejecuta una acción de edición, refresca la vista y guarda el mensaje de estado
  (errores del núcleo, "Guardado: …"). `UpdatePointer(rayo)` solo cambia la vista. `SaveProject`/`OpenProject`
  (ruta por defecto `Documentos/101st Floor/project.json`), `HasUnsavedChanges`. Cada cambio →
  `compileScene(project, overlay)` → `Reconcile`, con log `F101: scene resync in X ms`, el estado de cada muro y cada
  habitación (`F101: room <clave> area=…`); los refrescos de vista van a `Verbose`. Dentro de `Edit` el listener del
  documento no recompila: se hace una sola vez al final, con la vista del controlador ya actualizada (si no, el muro
  recién creado aparecería también como vista previa). `GetRooms()` da el área y la posición de etiqueta de cada
  habitación de la última escena (siguen al gesto en curso); `FindRoomFloorActor(clave)` su suelo.
- `UFloor101GameViewportClient` (`GameViewportClientClassName`): cursor → rayo de escena (deproyección +
  `ToScenePosition`), `MapKey` (función pura del mapa de teclas), caracteres → longitud escrita, rueda por
  `MouseWheelAxis`. Con la consola abierta todo va al motor. Registra cada tecla en `VeryVerbose`
  (`-LogCmds="LogFloor101 VeryVerbose"`).
- `AFloor101EditingPawn`: cámara orbital sin colisión (foco, guiñada, cabeceo entre −89° y −5°, distancia 0,5–2000 m);
  `Frame` encuadra con margen usando el campo de visión vertical; `TopView` es la vista en planta (−89°, norte arriba).
- `AFloor101GameMode` (`GlobalDefaultGameMode`) + `AFloor101PlayerController`: cursor visible, modo de entrada juego y
  UI, y el overlay `SFloor101Overlay` (barra con Seleccionar, Muro, Puerta y Ventana, panel de propiedades del muro o del
  hueco seleccionado, barra de estado, etiqueta del cursor y el área de cada habitación proyectada sobre su suelo).

Desarrollo:
- `-F101Demo` carga la pared de referencia al empezar el juego (`Run.ps1`, `CheckRendering.ps1 -Demo`).
- Consola: `f101.Demo.Load`, `f101.Demo.SetLength <longitud>` (acepta `420`, `4.2 m`, `4,2 m`, `4200 mm`),
  `f101.Demo.Clear`, `f101.Demo.Status`, `f101.Undo`, `f101.Redo`, `f101.Project.Save [ruta]`,
  `f101.Project.Open [ruta]`.

## Controles
| Entrada | Acción |
|---|---|
| clic izquierdo | Seleccionar: elige un muro, una puerta o una ventana (clic en el hueco) o limpia la selección; arrastrar un asa amarilla mueve esa esquina (y todos los muros que la comparten). Muro: inicio / crear (sobre un extremo o un muro se une a él). Puerta / Ventana: coloca el hueco en el muro señalado (centrado en el cursor, borde en la rejilla de 50 mm); el modo sigue activo para colocar más |
| W / P / O / V | herramienta Muro / Puerta / Ventana / Seleccionar (Ctrl+O sigue siendo abrir) |
| escribir `355`, `3.55 m`, `3550 mm` | longitud del muro que se dibuja (sin unidad = cm); Retroceso borra |
| Enter | crea el muro previsualizado |
| Esc o clic derecho sin arrastrar | cancela: el arrastre, luego el muro en curso, luego el modo Muro, Puerta o Ventana, luego la selección. En PIE, Esc detiene la sesión (tecla del editor): usar clic derecho |
| Supr | elimina el muro o el hueco seleccionado |
| panel de propiedades | muro: longitud, espesor, altura; puerta o ventana: ancho, alto, antepecho (ventanas) y posición (desde el inicio del muro hasta el borde del hueco). Sin unidad = cm; Enter aplica |
| Ctrl+Z / Ctrl+Y o Ctrl+Shift+Z | deshacer / rehacer |
| Ctrl+S / Ctrl+O | guardar / abrir el archivo actual (o el de por defecto) |
| derecho arrastrando / central arrastrando / rueda / F / T | orbitar / desplazar / zoom / encuadrar todo / vista en planta (norte arriba) |

## Herramientas de editor (`Source/Floor101Editor`, solo editor)
Toolset MCP `Floor101Editor.Floor101DocumentToolset`: `GetWalls`, `AddWall`, `SetWallEnd`, `SetWallLength`, `MoveCorner`, `GetRooms`,
`SetWallThickness`, `SetWallHeight`, `AddOpening`, `SetOpening`, `RemoveOpening`, `RemoveWall`, `Undo`, `Redo`,
`SaveProject`, `OpenProject`, en mm enteros. Actúa
sobre el documento de la sesión PIE mientras existe y, si no, sobre el del mundo del editor. Devuelve el actor para
inspeccionarlo con `ActorTools`. Se registra desde un `UEditorSubsystem` que depende de `UToolsetRegistrySubsystem`,
porque `RegisterToolsetClass` no hace nada si el registro aún no está listo. Ver `docs/unreal-mcp.md`.

## Tests de integración
`Floor101.Adapter.*` (conversión, pared de referencia, estilos de vista, muro con puerta: 36 triángulos y 7 secciones), `Floor101.Interaction.*` (mapa de teclas,
cámara, flujo completo por el subsistema: dibujar con longitud escrita, seleccionar, propiedades, deshacer, errores,
guardar y abrir con ruta no ASCII; habitación cerrada con suelo, área y arrastre de esquina con área en vivo) y
`Floor101.Editor.DocumentToolset.*` (cada herramienta y cada error).
`Test.ps1 -Integration` los ejecuta en un editor headless; también se pueden lanzar por MCP.

## Métricas observadas
Resync de la escena con una pared: 0,7–0,9 ms al crearla y 0,2 ms al regenerarla (MVP 0); 0,2–0,4 ms con dos paredes y
refrescos de vista de 0,01–0,02 ms sin cambios (MVP 1, log `LogFloor101`, Development). Render en `-game` a 1920×1080
con la pared: GPU 2,98 ms de media.
