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
  material base con su parámetro `Tint`, `Context` (plantas de abajo) muestra los acabados oscurecidos y `Ghost` (la
  arquitectura con la capa de instalaciones, ADR-0021) usa `MI_Floor101Ghost`, el vidrio gris azulado casi transparente,
  sin sombras. Muros, `JointFill`,
  suelos y forjados de habitación y la carpintería de cada hueco (ADR-0013) son nodos como cualquier otro.
- `UFloor101FinishLibrary` (subsistema de motor): una `UMaterialInstanceDynamic` de `M_Floor101Finish` por acabado,
  con la textura sintetizada por el Scene Model (`UTexture2D` transitoria sRGB con todos sus mips), la rugosidad y la
  repetición en metros (`BaseTexture`, `Roughness`, `RepeatU`, `RepeatV`). Los acabados translúcidos (opacidad < 1, el
  vidrio) son una instancia de `M_Floor101Glass` (translúcido, iluminado por píxel; `Tint`, `Opacity`, `Roughness`).
  Se crean al primer uso y se comparten entre actores y mundos. Si un material falta, registra un error
  (`CheckRendering` falla).
  Assets importados (ADR-0017): `SetAssetFinish(id, {imagen, volteo, tinte, rugosidad, repetición})` define el acabado
  de una textura o de un material de un modelo y actualiza en su sitio el material que ya exista; cada imagen se
  decodifica una vez por archivo (`FImageUtils::LoadImage`, sRGB) y pasa por `textureFromImage`. Una imagen ilegible
  deja el tinte solo y lo registra (`IsAssetImageMissing`); `GetAssetFinishImage` da la imagen vigente para las
  muestras de la UI. `GetPicture(ruta)` carga una imagen tal cual para la UI (las fichas de la biblioteca, ADR-0019), una
  vez por archivo (`FImageUtils::ImportFileAsTexture2D`).
- `FFloor101SceneReconciler`: por `SceneNodeId`, crea, actualiza (si cambió el `contentHash` **o** el estilo) o destruye
  actores. Nunca escala: la escala sigue en 1.

## Aplicación (`Source/Floor101`)
- `UFloor101DocumentSubsystem` (un `UWorldSubsystem` por mundo de juego, PIE o editor) posee el `Document` y su
  `EditController`. `Edit(acción)` ejecuta una acción de edición, refresca la vista y guarda el mensaje de estado
  (errores del núcleo, "Guardado: …"). `UpdatePointer(rayo)` solo cambia la vista. `SaveProject`/`OpenProject`
  (ruta por defecto `Documentos/101st Floor/project.json`; al guardar en otra carpeta copia antes los assets, y al abrir
  la ruta nueva rige antes de reconstruir la escena), `HasUnsavedChanges`. Assets (ADR-0017): `ImportAsset(ruta)`
  (copia a `assets/<uuid>/` junto al proyecto y lo añade; estado "Importado: …"), `ChangeAsset` (vuelve a medir un
  modelo si cambian unidad, eje o frente), `RemoveAsset`, `GetAssetFilePath`, `GetAssetProblem`; antes de cada
  compilación registra los acabados de los assets y da a la escena los modelos de un `ModelCache`. Biblioteca integrada
  (ADR-0019): `GetLibrary()` lee `<proyecto>/Library` una vez por ejecución (vacía, con un error en el log, si no se
  puede leer); `AddFromLibrary(id)` copia el elemento al proyecto como asset o devuelve el ya añadido (estado "Añadido de
  la biblioteca: …"). Modo Foto (ADR-0018): `SetPhotoMode` compila la escena sin marcas de edición; `SavePhoto(ruta)`
  pide la captura sin UI (`.png`); `GetDefaultPhotoPath()` = `<proyecto>-foto-<fecha>-<hora>.png`. Exportación para
  Twinmotion (ADR-0020): `ExportGltf(ruta)` escribe todo el proyecto como `.glb` con `f101::exporters::exportProjectGlb`
  (sus modelos del mismo `ModelCache`, nombres en español de `Floor101Names`) y devuelve objetos, materiales, imágenes,
  triángulos, bytes y avisos; estado "Exportado para Twinmotion: … (N objetos, M materiales, X MB)";
  `GetDefaultGltfPath()` = `<proyecto>.glb` junto al proyecto. Cada cambio →
  `compileScene(project, overlay)` → `Reconcile`, con log `F101: scene resync in X ms`, el estado de cada muro y cada
  habitación (`F101: room <clave> area=…`); los refrescos de vista van a `Verbose`. Dentro de `Edit` el listener del
  documento no recompila: se hace una sola vez al final, con la vista del controlador ya actualizada (si no, el muro
  recién creado aparecería también como vista previa). `GetRooms()` da el área y la posición de etiqueta (a la cota de
  su planta) de cada habitación de la planta activa, o de todas con "Ver todas" (siguen al gesto en curso);
  `FindRoomFloorActor(clave)` y `FindRoomSlabActor(clave)` su suelo y su forjado; `FindFurnitureActor(id)`,
  `FindColumnActor(id)` y `FindServiceRunActor(id)` el de un mueble, un pilar o un tramo (este, solo con la capa de
  instalaciones y con su red a la vista). Con la capa (ADR-0021), además de la arquitectura en `Ghost`, los `AStaticMeshActor` del mapa (el suelo
  de la plantilla) toman el material fantasma y recuperan los suyos al apagarla, para ver los tramos bajo la planta baja;
  la foto conserva la capa. Plano 2D (ADR-0015):
  `SetPlanView`/`IsPlanView`; mientras está visible, cada refresco dibuja la planta activa con las marcas de edición
  (`GetPlanDrawing`, textos en español y coma decimal). `ExportPlan(ruta, planta)` escribe el plano sin marcas (con las
  instalaciones si su capa está activa) en SVG
  (`writeSvg`) o PNG (`SFloor101PlanView` pintado con `FWidgetRenderer`, 5 mm por píxel, 4096 px como máximo; con
  `-nullrhi` devuelve un error claro); `GetDefaultPlanPath(ext)` = `<proyecto>-<planta>.<ext>` junto al proyecto.
- `UFloor101GameViewportClient` (`GameViewportClientClassName`): cursor → rayo de escena (deproyección +
  `ToScenePosition`), `MapKey` (función pura del mapa de teclas), caracteres → longitud escrita, rueda por
  `MouseWheelAxis`. El doble clic llega al controlador como `pointerDoubleClicked` (termina una tubería). Con la consola
  abierta todo va al motor. Registra cada tecla en `VeryVerbose`
  (`-LogCmds="LogFloor101 VeryVerbose"`). `SetPlanView` enciende el plano 2D: cámara ortográfica del pawn,
  `bDisableWorldRendering` y el plano del subsistema; T (cenital) lo apaga. `SetPhotoMode` (R) sale del plano, fija las
  muestras por píxel (`SetPhotoSamples`: 64, 256 o 1024) y activa el path tracer (`Floor101::PathTracing`: modo de vista
  `VMI_PathTracing`, progreso leído por una `FSceneViewExtension`, acumulación reiniciada al terminar de compilar
  shaders); en Modo Foto el clic izquierdo no edita.
- `AFloor101EditingPawn`: cámara sin colisión, orbital al editar y libre al navegar (foco, guiñada, cabeceo orbital entre −89° y −5°, distancia 0,02–2000 m);
  `Frame` encuadra con margen usando el campo de visión vertical; `TopView` es la vista en planta (−89°, norte arriba);
  `RaiseFocus` sube o baja el foco: el viewport client lo usa para que la cámara siga la cota de la planta activa, y
  "Encuadrar" solo mira las plantas visibles. `SetPlanView`: cámara ortográfica recta hacia abajo (norte arriba) a 1 km
  sobre el foco, tan ancha como la vista; sin órbita; `Frame` encaja la huella con sitio para las cotas.
- `AFloor101GameMode` (`GlobalDefaultGameMode`) + `AFloor101PlayerController`: cursor visible, modo de entrada juego y
  UI, y el overlay `SFloor101Overlay` (barra con Seleccionar, Muro, Puerta, Ventana, Pintar, Amueblar, Pilar,
  Instalación y "Rayos X (X)" y fila de plantas,
  panel de propiedades del muro, del hueco (con su tipo, bisagras, sentido, apertura y carpintería), del mueble (medidas,
  giro, acabado), del pilar (forma, medidas, giro, acabado), de la tubería (tipo, longitud, puntos, alturas) o de la
  habitación seleccionados, la forma y medidas del pilar (en Pilar), el tipo y la altura de la tubería (en Instalación),
  el tipo de las puertas o ventanas que se van a colocar
  (en Puerta y Ventana), la paleta de Pintar (pestañas Paredes/Suelos, secciones por categoría con las fotografías de la
  biblioteca, "Texturas importadas") o el catálogo de Amueblar (pestañas por categoría con fichas con imagen: tipos
  paramétricos y modelos de la biblioteca; "Modelos importados") o, sin selección, de la planta activa; en Modo Foto,
  solo su panel (muestras, calidad, guardar imagen, volver), barra de estado, etiqueta del cursor y el área de cada habitación proyectada sobre su suelo). En el
  plano 2D, `SFloor101PlanView` pinta el plano sobre toda la vista (rellenos con vértices propios, líneas, textos
  girados) con la proyección de la cámara ortográfica; no toma el ratón, así que la edición sigue en el viewport.
- `FFloor101FileDialog::OpenFile`: el diálogo de abrir archivo de Windows (`GetOpenFileNameW`, `comdlg32`), con filtro
  por tipo y la carpeta de la última vez; lo usan "Importar textura…" e "Importar modelo…".

Desarrollo:
- `-F101Demo` carga la pared de referencia al empezar el juego (`Run.ps1`, `CheckRendering.ps1 -Demo`). Al empezar
  el juego, las mallas propias del mapa (suelo y esfera del cielo de la plantilla) dejan de proyectar sombra: desbordaban
  la cola de VSM para mallas no Nanite (`docs/rendering-rtx.md`). `Run.ps1` lanza con `-DisablePython` (la app no usa
  Python; sin él, el `init_unreal.py` de ToolsetRegistry, solo de editor, falla al arrancar en `-game`).
- Consola: `f101.Demo.Load`, `f101.Demo.SetLength <longitud>` (acepta `420`, `4.2 m`, `4,2 m`, `4200 mm`),
  `f101.Demo.Clear`, `f101.Demo.Status`, `f101.Undo`, `f101.Redo`, `f101.Project.Save [ruta]`,
  `f101.Project.Open [ruta]`, `f101.Plan.Export [ruta .svg|.png]`, `f101.Asset.Import <ruta>`,
  `f101.Export.Gltf [ruta .glb]`.

## Controles
| Entrada | Acción |
|---|---|
| clic izquierdo | Seleccionar: elige un muro, una puerta o una ventana (clic en el hueco), una habitación (clic en su suelo) o limpia la selección; arrastrar un asa amarilla mueve esa esquina (y todos los muros que la comparten). Con un hueco seleccionado, arrastrar el hueco lo mueve y arrastrar un asa de su borde cambia el ancho (rejilla de 50 mm, mínimo 30 cm, sin pasar uniones ni otros huecos; la etiqueta del cursor da posición y ancho). Muro: inicio / crear (sobre un extremo o un muro se une a él). Pintar: aplica el acabado de la paleta a la cara del muro que toca el cursor o al suelo de la habitación (un acabado solo de suelo, de la pestaña Suelos, mira a través de los muros hasta el suelo; la barra de estado dice qué se pintará). Puerta / Ventana: coloca el hueco, del tipo elegido en el panel ("Puerta: elige el tipo": abatible, de dos hojas, corredera empotrada; "Ventana: elige el tipo": practicable, de dos hojas, corredera, fija; cada clase recuerda el suyo), en el muro señalado (centrado en el cursor, borde en la rejilla de 50 mm, en el tramo libre de uniones y huecos más cercano), abriendo hacia el lado del muro donde está el cursor; el modo sigue activo para colocar más. Amueblar: coloca el mueble elegido en su categoría (tipo paramétrico o modelo de la biblioteca, que se copia al proyecto al elegirlo; centrado en el cursor, en la rejilla de 50 mm; los que van contra un muro, a menos de su medio fondo + 40 cm de una cara, se apoyan de espaldas en ella y se paran en los muros con que chocan). Seleccionar un mueble (clic en él) y arrastrarlo lo mueve con el mismo ajuste. Pilar: coloca el pilar de la forma y medidas del panel, ajustado a las esquinas, a los ejes de los muros o a la rejilla; en Seleccionar se elige antes que el muro en que está y se arrastra con el mismo ajuste. Instalación: cada clic añade un punto a la altura del panel (Bajo suelo, Pared, Techo o escrita), ajustado como el extremo de un muro; doble clic termina; cambiar la altura a mitad sube o baja en vertical. Con la capa, en Seleccionar, un clic en una tubería la elige a través de los muros: arrastrarla la mueve por la rejilla y sus asas mueven un codo |
| W / P / O / M / A / C / I / V | herramienta Muro / Puerta / Ventana / Pintar / Amueblar / Pilar / Instalación / Seleccionar (Ctrl+O sigue siendo abrir) |
| X / botón "Rayos X (X)" | capa de instalaciones: muestra las tuberías y deja translúcidos muros, suelos, forjados, carpintería y el suelo del mapa; Instalación la enciende sola. Con la capa, la fila "Redes" (Electricidad, Agua, Aguas negras, Gas, Todas, Ninguna) elige qué redes se ven en 3D, en el plano 2D y en el plano exportado |
| Espacio | con Puerta o Ventana: las siguientes con las bisagras en la otra jamba; con un hueco seleccionado: invierte sus bisagras (o hacia dónde corre); con Amueblar o Pilar: gira 90° los siguientes; con un mueble o un pilar seleccionado: lo gira 90°. Escribiendo una longitud sigue siendo un espacio |
| escribir `355`, `3.55 m`, `3550 mm` | longitud del muro que se dibuja (sin unidad = cm); Retroceso borra. Mientras se escribe, M y C son texto (no Pintar) |
| R / botón "Foto (R)" | Modo Foto: la vista con path tracing, sin marcas de edición; el panel da el progreso, la calidad (Borrador 64, Media 256, Alta 1024 muestras) y "Guardar imagen" (PNG en SDR, EXR con HDR); la cámara se mueve igual y reinicia la imagen; R o Esc vuelven a editar |
| F11 / botón "Pantalla completa" | pantalla completa dedicada a la resolución nativa del monitor y render al 100 %; vuelve al tamaño de ventana anterior. Disponible también en Modo Foto |
| N / botón "Navegar"; derecho + WASD al editar | WASD desplaza la cámara, Q/E baja/sube, Shift acelera; derecho arrastrando mira desde la cámara. N/Esc vuelve a editar. En plano 2D, WASD desplaza el plano. [Detalle](camera-navigation.md) |
| Selector "Velocidad" | niveles 1–10 (0,1–80 m/s); inicial 3 m/s, independiente del zoom; guarda la elección. Shift multiplica por cuatro |
| Botón "Sincronización vertical" | activa/desactiva VSync inmediatamente y guarda la elección; conserva HDR y resolución |
| Botón "Salir" (arriba a la derecha) | cierra la aplicación; con cambios sin guardar pregunta si guardar, descartar o cancelar. Cancelar el guardado o un error conserva la sesión. En PIE termina la sesión de juego |
| "Resolución" y fila Pantalla / Imagen / Render | elegir un modo admitido por el monitor y comprobar dimensiones nativas, viewport efectivo y render interno antes de TSR. En plano 2D se indica "plano 2D"; detalle en [pantalla y resolución](display.md) |
| Tab / botón "Plano 2D (Tab)" | plano 2D de la planta activa, editable como en 3D (rueda: zoom, central: desplazar); Tab o T vuelven a 3D |
| "Plano: SVG / PNG" | exporta el plano de la planta activa junto al proyecto (`<proyecto>-<planta>.svg|png`) |
| "Twinmotion (.glb)…" | exporta la casa entera (todas las plantas, forjados, puertas, ventanas, muebles, pilares, instalaciones, materiales y texturas) donde elijas con el diálogo de Windows (propone la última carpeta usada para Twinmotion, si no la del proyecto) (`<proyecto>.glb`); en Twinmotion: Importar → el `.glb`; tras cambios, exportar otra vez y refrescar con "Use scene materials" |
| Enter | crea el muro previsualizado; en Instalación, termina la tubería |
| Esc o clic derecho sin arrastrar | cancela: el arrastre, luego el muro o la tubería en curso, luego el modo de herramienta, luego la selección. En PIE, Esc detiene la sesión (tecla del editor): usar clic derecho |
| Supr | elimina el muro, el hueco, el mueble, el pilar o la tubería seleccionados |
| Retroceso | borra lo escrito de la longitud; en Instalación, quita el último punto |
| RePág / AvPág | editar la planta de arriba / de abajo (la cámara la sigue) |
| L | ver todas las plantas con sus forjados, o solo la activa y, atenuadas, las de abajo |
| fila de plantas | subir, bajar, "+ Nueva planta" (encima de todas, igual que la de arriba, y pasa a editarla), "Ver todas (L)" |
| panel de propiedades | muro: longitud, espesor, altura y el acabado de cada cara ("Cara norte"...); habitación: suelo y techo; en Pintar, la paleta de acabados; puerta o ventana: ancho, alto, antepecho (ventanas), posición (desde el inicio del muro hasta el borde del hueco), tipo (puertas: abatible, abatible de dos hojas, corredera empotrada; ventanas: practicable, practicable de dos hojas, corredera, fija), bisagras o hacia dónde se abre la corredera ("Lado oeste", Invertir), hacia qué cara abre ("Cara norte", Invertir), apertura (%) y carpintería (lacado blanco, maderas, aluminio antracita); mueble: ancho, fondo, alto, giro (grados), "Girar 90°" y acabado ("Original" en un modelo importado: sus propios materiales); pilar: Rectangular/Redondo, ancho y fondo (o diámetro), giro, "Girar 90°" y acabado de muro; tubería: su tipo (Electricidad, Agua, Aguas negras, Gas, con su color y diámetro), longitud, puntos y alturas; en Pilar, la forma y medidas de los siguientes; en Instalación, el tipo, la altura (Bajo suelo, Pared, Techo o escrita) y los puntos colocados; en Pintar, "Texturas importadas" con su imagen y "Importar textura…" (nombre, ancho y alto de la imagen, rugosidad, "Eliminar textura"); en Amueblar, las pestañas Camas, Sillas, Sofás, Mesas, Almacenaje, Cocina, Baño, Luces y Decoración con sus fichas, "Modelos importados" e "Importar modelo…" (nombre, unidades del archivo, "Cambiar eje arriba", "Girar frente 90°", "Se apoya en los muros", tamaño y triángulos, "Eliminar modelo"); sin selección, la planta activa: nombre, altura de planta, forjado, "Eliminar planta". Sin unidad = cm; Enter aplica |
| Ctrl+Z / Ctrl+Y o Ctrl+Shift+Z | deshacer / rehacer |
| Ctrl+S / Ctrl+Shift+S / Ctrl+O, botones "Guardar", "Guardar como…", "Abrir…" | guardar en el archivo actual (una casa nueva pregunta dónde) / guardar como (el diálogo de Windows: carpeta y nombre) / abrir (el diálogo, en la última carpeta usada). "Archivo" muestra el archivo actual. Las últimas ubicaciones elegidas (casa y Twinmotion) se recuerdan entre sesiones (`FFloor101FileActions`, `GameUserSettings.ini`, sección `[Floor101.Paths]`) |
| derecho arrastrando / central arrastrando / rueda / F / T | orbitar / desplazar / zoom / encuadrar todo / vista 3D cenital (norte arriba) |

## Herramientas de editor (`Source/Floor101Editor`, solo editor)
Toolset MCP `Floor101Editor.Floor101DocumentToolset`: `GetLevels`, `AddLevel`, `SetLevel`, `RemoveLevel`, `SetLevelView`,
`GetFinishes`, `SetWallFinish`, `SetRoomFinish`,
`GetWalls`, `AddWall` (en una planta), `SetWallEnd`, `SetWallLength`, `MoveCorner`, `GetRooms`,
`SetWallThickness`, `SetWallHeight`, `AddOpening`, `SetOpening`, `SetOpeningFinish`, `SetOpeningOperation`,
`RemoveOpening`, `RemoveWall`, `GetFurnitureTypes`, `GetFurniture`, `AddFurniture`, `SetFurniture`,
`SetFurnitureFinish`, `RemoveFurniture`, `GetColumns`, `AddColumn`, `SetColumn`, `SetColumnFinish`, `RemoveColumn`,
`GetServiceRuns`, `AddServiceRun`, `SetServiceRun`, `RemoveServiceRun`, `SetServicesView`, `GetAssets`, `ImportAsset`, `SetTextureAsset`, `SetModelAsset`, `RemoveAsset`,
`GetLibrary`, `AddFromLibrary`, `ExportGltf`,
`Undo`, `Redo`,
`SaveProject`, `OpenProject`, `ExportPlan`, en mm enteros. Los muros y muebles de plantas que la vista oculta no tienen actor (`SetLevelView`
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
rechazada; plano 2D: dibujo con la selección y las asas, cámara ortográfica y encuadre, exportación SVG y ruta por
defecto, PNG con la tinta del corte donde hay renderer; muebles: colocar un sofá contra un muro, su actor y medidas,
seleccionarlo, arrastrarlo, girarlo, borrarlo y deshacer, en el plano, guardar y abrir; assets: importar una textura,
un sillón GLB y una lámpara OBJ, pintar, colocar, materiales e imágenes, guardar en otra carpeta y reabrir, también
desde un proyecto sin esos archivos; biblioteca: una silla de la `Library/` real añadida una vez y colocada con sus
materiales, un laminado fotográfico en el suelo, esquema 9; Modo Foto: sin tinte de selección ni arcos de apertura;
exportación: el `.glb` junto a un proyecto con ruta no ASCII, cabecera, nombres en español y el PNG del ladrillo
decodificado por Unreal igual, píxel a píxel, que la textura de la aplicación; instalaciones: pilar ajustado a la esquina, de
hormigón y tan alto como la planta; capa con el muro y el suelo del mapa en fantasma y el pilar sólido; tubería en tres
clics; plano y SVG con el tubo verde; X la oculta y devuelve sus materiales; seleccionarla la vuelve a mostrar; guardar
y abrir con el esquema 10) y
`Floor101.Editor.DocumentToolset.*` (cada herramienta y cada error).
`Test.ps1 -Integration` los ejecuta en un editor headless; también se pueden lanzar por MCP.

## Métricas observadas
Resync de la escena con una pared: 0,7–0,9 ms al crearla y 0,2 ms al regenerarla (MVP 0); 0,2–0,4 ms con dos paredes y
refrescos de vista de 0,01–0,02 ms sin cambios (MVP 1, log `LogFloor101`, Development). Render en `-game` a 1920×1080
con la pared: GPU 2,98 ms de media.
