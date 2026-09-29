# Integración con Unreal

## Unreal Adapter (`Source/Floor101UnrealAdapter`)
Única frontera entre la escena y Unreal (ADR-0005). Conoce la escena, nunca el modelo CAD.

- `Floor101SceneSpace.h` (público): mm ↔ cm y espejo de Y (dextrógiro ↔ levógiro) en ambos sentidos
  (`ToUnrealPosition`, `ToScenePosition`, `ToSceneDirection`), también para convertir el rayo del cursor.
- `Floor101UnrealConversion.h` (privado): `ToDynamicMesh` crea `FDynamicMesh3` con normales, UV, polygroups e ID de
  material por sección. **Conserva el orden de los triángulos**: el espejo y la convención de Unreal
  (`VectorUtil::Normal` = `edge2 × edge1`) se compensan.
- `AFloor101SceneNodeActor`: `UCLASS(Transient, NotPlaceable)`, raíz `UDynamicMeshComponent` con ray tracing activado,
  material base `/Game/Floor101/Materials/M_Floor101Finish` (referencia dura y `DirectoriesToAlwaysCook`), etiqueta
  `f101.node:<SceneNodeId>`. Cada sección muestra su acabado (ADR-0012); los estilos de vista `Selected` (naranja),
  `Preview` (azul), `Handle` (amarillo) y `Symbol` (casi negro, sin sombra: los arcos de apertura, ADR-0014) tiñen el
  material base con su parámetro `Tint`, y `Context` (plantas de abajo) muestra los acabados oscurecidos. Muros, `JointFill`,
  suelos y forjados de habitación y la carpintería de cada hueco (ADR-0013) son nodos como cualquier otro.
- `UFloor101FinishLibrary` (subsistema de motor): una `UMaterialInstanceDynamic` de `M_Floor101Finish` por acabado,
  con la textura sintetizada por el Scene Model (`UTexture2D` transitoria sRGB con todos sus mips), la rugosidad y la
  repetición en metros (`BaseTexture`, `Roughness`, `RepeatU`, `RepeatV`). Los acabados translúcidos (opacidad < 1, el
  vidrio) son una instancia de `M_Floor101Glass` (translúcido, iluminado por píxel; `Tint`, `Opacity`, `Roughness`).
  Se crean al primer uso y se comparten entre actores y mundos. Si un material falta, registra un error
  (`CheckRendering` falla).
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
  recién creado aparecería también como vista previa). `GetRooms()` da el área y la posición de etiqueta (a la cota de
  su planta) de cada habitación de la planta activa, o de todas con "Ver todas" (siguen al gesto en curso);
  `FindRoomFloorActor(clave)` y `FindRoomSlabActor(clave)` su suelo y su forjado.
- `UFloor101GameViewportClient` (`GameViewportClientClassName`): cursor → rayo de escena (deproyección +
  `ToScenePosition`), `MapKey` (función pura del mapa de teclas), caracteres → longitud escrita, rueda por
  `MouseWheelAxis`. Con la consola abierta todo va al motor. Registra cada tecla en `VeryVerbose`
  (`-LogCmds="LogFloor101 VeryVerbose"`).
- `AFloor101EditingPawn`: cámara orbital sin colisión (foco, guiñada, cabeceo entre −89° y −5°, distancia 0,5–2000 m);
  `Frame` encuadra con margen usando el campo de visión vertical; `TopView` es la vista en planta (−89°, norte arriba);
  `RaiseFocus` sube o baja el foco: el viewport client lo usa para que la cámara siga la cota de la planta activa, y
  "Encuadrar" solo mira las plantas visibles.
- `AFloor101GameMode` (`GlobalDefaultGameMode`) + `AFloor101PlayerController`: cursor visible, modo de entrada juego y
  UI, y el overlay `SFloor101Overlay` (barra con Seleccionar, Muro, Puerta, Ventana y Pintar y fila de plantas, panel
  de propiedades del muro, del hueco (con su tipo, bisagras, sentido, apertura y carpintería) o de la habitación
  seleccionados o, sin selección, de la
  planta activa, barra de estado, etiqueta del cursor y el área de cada habitación proyectada sobre su suelo).

Desarrollo:
- `-F101Demo` carga la pared de referencia al empezar el juego (`Run.ps1`, `CheckRendering.ps1 -Demo`).
- Consola: `f101.Demo.Load`, `f101.Demo.SetLength <longitud>` (acepta `420`, `4.2 m`, `4,2 m`, `4200 mm`),
  `f101.Demo.Clear`, `f101.Demo.Status`, `f101.Undo`, `f101.Redo`, `f101.Project.Save [ruta]`,
  `f101.Project.Open [ruta]`.

## Controles
| Entrada | Acción |
|---|---|
| clic izquierdo | Seleccionar: elige un muro, una puerta o una ventana (clic en el hueco), una habitación (clic en su suelo) o limpia la selección; arrastrar un asa amarilla mueve esa esquina (y todos los muros que la comparten). Con un hueco seleccionado, arrastrar el hueco lo mueve y arrastrar un asa de su borde cambia el ancho (rejilla de 50 mm, mínimo 30 cm, sin pasar uniones ni otros huecos; la etiqueta del cursor da posición y ancho). Muro: inicio / crear (sobre un extremo o un muro se une a él). Pintar: aplica el acabado de la paleta a la cara del muro que toca el cursor o al suelo de la habitación. Puerta / Ventana: coloca el hueco en el muro señalado (centrado en el cursor, borde en la rejilla de 50 mm, en el tramo libre de uniones y huecos más cercano), abriendo hacia el lado del muro donde está el cursor; el modo sigue activo para colocar más |
| W / P / O / M / V | herramienta Muro / Puerta / Ventana / Pintar / Seleccionar (Ctrl+O sigue siendo abrir) |
| Espacio | con Puerta o Ventana: las siguientes con las bisagras en la otra jamba; con un hueco seleccionado: invierte sus bisagras (o hacia dónde corre). Escribiendo una longitud sigue siendo un espacio |
| escribir `355`, `3.55 m`, `3550 mm` | longitud del muro que se dibuja (sin unidad = cm); Retroceso borra |
| Enter | crea el muro previsualizado |
| Esc o clic derecho sin arrastrar | cancela: el arrastre, luego el muro en curso, luego el modo Muro, Puerta, Ventana o Pintar, luego la selección. En PIE, Esc detiene la sesión (tecla del editor): usar clic derecho |
| Supr | elimina el muro o el hueco seleccionado |
| RePág / AvPág | editar la planta de arriba / de abajo (la cámara la sigue) |
| L | ver todas las plantas con sus forjados, o solo la activa y, atenuadas, las de abajo |
| fila de plantas | subir, bajar, "+ Nueva planta" (encima de todas, igual que la de arriba, y pasa a editarla), "Ver todas (L)" |
| panel de propiedades | muro: longitud, espesor, altura y el acabado de cada cara ("Cara norte"...); habitación: suelo y techo; en Pintar, la paleta de acabados; puerta o ventana: ancho, alto, antepecho (ventanas), posición (desde el inicio del muro hasta el borde del hueco), tipo (puertas: abatible, abatible de dos hojas, corredera empotrada; ventanas: practicable, practicable de dos hojas, corredera, fija), bisagras o hacia dónde se abre la corredera ("Lado oeste", Invertir), hacia qué cara abre ("Cara norte", Invertir), apertura (%) y carpintería (lacado blanco, maderas, aluminio antracita); sin selección, la planta activa: nombre, altura de planta, forjado, "Eliminar planta". Sin unidad = cm; Enter aplica |
| Ctrl+Z / Ctrl+Y o Ctrl+Shift+Z | deshacer / rehacer |
| Ctrl+S / Ctrl+O | guardar / abrir el archivo actual (o el de por defecto) |
| derecho arrastrando / central arrastrando / rueda / F / T | orbitar / desplazar / zoom / encuadrar todo / vista en planta (norte arriba) |

## Herramientas de editor (`Source/Floor101Editor`, solo editor)
Toolset MCP `Floor101Editor.Floor101DocumentToolset`: `GetLevels`, `AddLevel`, `SetLevel`, `RemoveLevel`, `SetLevelView`,
`GetFinishes`, `SetWallFinish`, `SetRoomFinish`,
`GetWalls`, `AddWall` (en una planta), `SetWallEnd`, `SetWallLength`, `MoveCorner`, `GetRooms`,
`SetWallThickness`, `SetWallHeight`, `AddOpening`, `SetOpening`, `SetOpeningFinish`, `SetOpeningOperation`,
`RemoveOpening`, `RemoveWall`,
`Undo`, `Redo`,
`SaveProject`, `OpenProject`, en mm enteros. Los muros de plantas que la vista oculta no tienen actor (`SetLevelView`
elige planta y "ver todas"). Actúa sobre el documento de la sesión PIE mientras existe y, si no, sobre el del mundo del editor. Devuelve el actor para
inspeccionarlo con `ActorTools`. Se registra desde un `UEditorSubsystem` que depende de `UToolsetRegistrySubsystem`,
porque `RegisterToolsetClass` no hace nada si el registro aún no está listo. Ver `docs/unreal-mcp.md`.

## Tests de integración
`Floor101.Adapter.*` (conversión, pared de referencia, estilos de vista, acabados: material base, material por sección,
textura con mips, repetición y rugosidad, muro con puerta: 36 triángulos y 7 secciones; carpintería: vidrio translúcido,
marco lacado, medidas del actor; tipos: hoja abierta en el mismo actor, símbolo oscuro sin sombra, ninguno para
exportadores), `Floor101.Interaction.*` (mapa de teclas,
cámara, flujo completo por el subsistema: dibujar con longitud escrita, seleccionar, propiedades, deshacer, errores,
guardar y abrir con ruta no ASCII; habitación cerrada con suelo, área y arrastre de esquina con área en vivo; plantas: forjado entre muros, planta de abajo
atenuada, muro dibujado en la cota de su planta, ocultar y "Ver todas"; acabados: pintar una cara y un suelo,
seleccionar la habitación y su techo; carpintería: rechazo en una T, arrastre del hueco con el mismo actor, Esc a mitad
de arrastre; tipos: colocar abriendo hacia el cursor con Espacio, abrir al 100 %, corredera sin símbolo, puerta fija
rechazada) y
`Floor101.Editor.DocumentToolset.*` (cada herramienta y cada error).
`Test.ps1 -Integration` los ejecuta en un editor headless; también se pueden lanzar por MCP.

## Métricas observadas
Resync de la escena con una pared: 0,7–0,9 ms al crearla y 0,2 ms al regenerarla (MVP 0); 0,2–0,4 ms con dos paredes y
refrescos de vista de 0,01–0,02 ms sin cambios (MVP 1, log `LogFloor101`, Development). Render en `-game` a 1920×1080
con la pared: GPU 2,98 ms de media.
