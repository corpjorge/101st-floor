# Arquitectura

Estado: **aprobada (Fase D, 2026-09-28); MVP 13 implementado.** Decisiones importantes en `docs/adr/`.

## Principio

101st Floor es la fuente de verdad. El modelo CAD (paramétrico, en milímetros enteros) genera geometría, la geometría
compila una escena neutral, y cada consumidor (Unreal, exportadores, vista 2D) representa esa escena. Nada fluye en
sentido contrario: Unreal no escribe en el modelo, y un nivel de Unreal nunca se guarda como dato del proyecto.

```
   UI Slate ─► EditController ─ comandos ─►  CAD Core  ◄── Assets (importar y biblioteca integrada, ADR-0017/0019)
                                               │ cambios
                                               ▼
                                        Geometry Engine        (Wall → MeshData)
                                               │
                         ┌─────────────────────┴──────────────────────┐
                         ▼                                            ▼
                    Scene Model        (derivada, IDs deterministas)  Drawing (planos 2D, ADR-0015)
               ┌─────────┴──────────┐                                 │
               ▼                    ▼                                 ▼
       Unreal Adapter        Scene Exporters                 Vista 2D · SVG · PNG
               │           (glTF .glb → Twinmotion, ADR-0020;
               │            USD, Datasmith: futuro)
               ▼
  Unreal Runtime (DX12 · Lumen HWRT)
```

## Módulos

Diez módulos de producto y uno de desarrollo, solo para el editor. CadCore, Geometry, SceneModel, Drawing, Assets y Exporters son
**C++20 estándar**: no incluyen cabeceras de Unreal, no usan tipos de Unreal y compilan igual con UnrealBuildTool y con
CMake (tests sin Unreal). Ver ADR-0002.

| Módulo | Tipo | Namespace | Depende de | Contenido |
|---|---|---|---|---|
| `Floor101Diagnostics` | Unreal temprano | `Floor101::Diagnostics` | Core | registro del producto en Shipping y ventana opcional; no cambia la ABI del motor instalado (ADR-0028) |
| `Floor101Startup` | Unreal temprano | `Floor101::Startup` | Core, Slate, PreLoadScreen, Diagnostics | splash propio, barra de actividad y transición al juego; carga en PreLoadingScreen (ADR-0027) |
| `Floor101CadCore` | C++ puro | `f101::cad` | — (nlohmann/json interno) | unidades, `Point2D`/`Vector2D`/`Segment2D`/`Bounds`, IDs, `Project`/`Level`/`Wall`, validación, `Document` (comandos, deshacer/rehacer, notificación de cambios), snapping, picking, `WallTool`, `EditController`, mobiliario (`Furniture`, catálogo), pilares e instalaciones (`Column`, `ServiceRun`, ADR-0021), `project.json` |
| `Floor101Geometry` | C++ puro | `f101::geometry` | CadCore | `MeshData` propio, uniones de muros (`solveWallJoins`) y tramos que ocupan en cada muro (`wallObstructions`), `WallGeometryBuilder` sobre huellas (con huecos), carpintería de huecos (`buildOpeningMesh`), polígonos y prismas, habitaciones (`detectRooms`), suelos, forjados y rodapiés (`buildBaseboardMesh`, ADR-0030), muebles (`furnitureShape`, `buildFurnitureMesh`), pilares y tuberías (`buildColumnMesh`, `buildServiceRunMesh`) |
| `Floor101SceneModel` | C++ puro | `f101::scene` | CadCore, Geometry | `Scene` (nodos y habitaciones), `SceneNode` (con `NodeStyle`), `SceneCompiler` (Project → muros unidos, uniones, suelos, rodapiés, carpintería, muebles, pilares, tuberías; con `ViewOverlay` que compila el proyecto como quedaría y la capa de instalaciones con la arquitectura en `Ghost`); más adelante materiales portables, luces, cámaras, `SceneDiff` |
| `Floor101Drawing` | C++ puro | `f101::drawing` | CadCore, Geometry | planos 2D de una planta (`buildPlanDrawing`: muros y pilares cortados, carpintería, muebles, cotas, habitaciones, tuberías con la capa, marcas de edición) y su SVG (`writeSvg`); cotas manuales y preferencias automáticas compartidas con CAD (ADR-0031); paleta común (ADR-0015) |
| `Floor101Assets` | C++ puro | `f101::assets` | CadCore, Geometry (nlohmann/json interno) | assets importados (ADR-0017): cabeceras PNG/JPEG, lectores glTF 2.0 (`.gltf`, `.glb`) y OBJ/MTL, normalización de un modelo al marco del mueble (`normalizeModel`), importación con sus dependencias a `assets/<uuid>/` (`importAsset`), medida con otros ajustes, copia al guardar en otra carpeta y `ModelCache`; biblioteca integrada (ADR-0019): manifiestos de `Library/`, `loadLibrary`, `importLibraryFurniture`/`importLibraryTexture` |
| `Floor101Exporters` | C++ puro | `f101::exporters` | CadCore, Geometry, SceneModel, Assets (nlohmann/json interno) | exportación de la escena a formatos de intercambio (ADR-0020): glTF 2.0 binario para Twinmotion (`writeGlb`, `exportProjectGlb`: plantas, grupos por clase (con Pilares e Instalaciones), materiales con sus imágenes, metros con Y arriba) y codificador PNG propio (`encodePng`, `zlibCompress`) |
| `Floor101UnrealAdapter` | Unreal | — | SceneModel, Engine, GeometryFramework, ImageCore | reconciliación `Scene` → actores con `UDynamicMeshComponent` (hash + estilo); estilos de vista con materiales tintados; acabados de imágenes importadas; imágenes de la biblioteca para la UI; conversión de unidades y ejes en ambos sentidos; tests de integración |
| `Floor101` | Unreal, módulo principal | — | CadCore, Geometry, SceneModel, Drawing, Assets, Exporters, UnrealAdapter, Slate, UMG | aplicación: `UFloor101DocumentSubsystem` (posee `Document` y `EditController` de cada mundo; guardar/abrir; plano 2D y su exportación; importar assets; biblioteca integrada; Modo Foto; exportación para Twinmotion), nombres de los catálogos en español (`Floor101Names`), path tracing del Modo Foto (`Floor101::PathTracing`), diálogo de archivo nativo, entrada (`UFloor101GameViewportClient`), cámara orbital u ortográfica, game mode, overlay Slate `SFloor101Overlay` y capa del plano `SFloor101PlanView`, comandos de consola `f101.*`, `RenderCheck` |
| `Floor101Editor` | Unreal, **solo editor** | — | Floor101, CadCore, Geometry, SceneModel, Assets, ToolsetRegistry | herramientas de desarrollo: toolset MCP `Floor101DocumentToolset` (plantas, paredes en mm, huecos, muebles, pilares, instalaciones y su capa, acabados, assets, biblioteca, deshacer/rehacer, guardar/abrir, planos, exportación para Twinmotion; actúa en PIE si está activo) y el commandlet que genera los materiales. Nunca entra en el juego |

La dirección de dependencias se hace cumplir en compilación: un módulo solo ve las cabeceras de sus dependencias
declaradas (UBT y CMake), y UBT tampoco enlaza dependencias transitivas. Por ejemplo, el adaptador no puede llamar a
funciones del CAD Core sin declararlo; sus tests construyen el `Project` directamente, como dato. Los módulos puros declaran únicamente `Core` en su `Build.cs`, y solo para el fichero de
registro del módulo (`IMPLEMENT_MODULE`), que CMake excluye. Una prueba de arquitectura en `Test.ps1` falla si un fichero
puro incluye una cabecera de Unreal.

Reglas del C++ puro (para compilar idéntico dentro de Unreal, ver ADR-0002):
- C++20, **sin excepciones** (`throw`) y **sin RTTI** (`dynamic_cast`, `typeid`): son los defaults de UE 5.8.
- Errores esperables → `Result<T>`/`Status` explícitos; errores de programación → `F101_ASSERT`.
- Nombres dentro de `f101::`; no usar identificadores que Unreal define como macro (`check`, `verify`, `ensure`, `TEXT`).

## Unidades y coordenadas (ADR-0001, ADR-0003)

- **Persistente:** milímetros enteros `int64_t` (`Point2D{ x_mm, y_mm }`, espesor, altura). Resolución del modelo: 1 mm.
- **Entrada del usuario:** `355`, `355 cm`, `3.55 m`, `3,55 m`, `3550 mm` → 3550 mm. Sin unidad se aplica la unidad de
  entrada del proyecto (por defecto **cm**, como el ejemplo del prompt maestro). El parser es decimal **exacto** (nunca
  pasa por `double`) y redondea a 1 mm (mitad lejos de cero).
- **Cálculo:** `double` para trigonometría, intersecciones y geometría derivada. Si un resultado calculado vuelve al
  modelo (p. ej. un punto de intersección que pasa a ser extremo de pared), se cuantiza a mm en ese punto, de forma
  explícita. `MeshData` y la escena no se cuantizan.
- **Ejes:** modelo y escena son dextrógiros, X este, Y norte, Z arriba, en mm. El adaptador de Unreal (levógiro, cm)
  aplica `(x, y, z) mm → (x/10, −y/10, z/10) cm` y conserva el orden de los triángulos: el espejo y la convención
  levógira de Unreal se compensan (ADR-0003). Cada consumidor tiene una única conversión documentada (glTF para
  Twinmotion: `(x, z, −y) / 1000` m, una rotación que también conserva los triángulos; ADR-0020).

## Identidad

- Entidades (`WallId`, `LevelId`, …): UUID v4 con tipo propio por entidad. Generador inyectable para que los tests sean
  deterministas. Nunca se usan índices como identidad.
- Nodos de escena: **derivados de forma determinista** de la entidad de origen y su rol (`<wallId>/body`). Recompilar
  la escena produce los mismos IDs, y eso permite reconciliar actores de Unreal, calcular un `SceneDiff` y, en el futuro,
  reimportar en Twinmotion sin perder el trabajo artístico.

## Flujo de un cambio (MVP 2–7)

```
clic / tecla / texto en el viewport                     // UFloor101GameViewportClient: rayo en mm, MapKey, caracteres
  → UFloor101DocumentSubsystem::Edit(acción)            // mensaje de estado para la UI
  → EditController (CAD Core)                           // planta activa (rayo bajado a su cota), modo, selección,
                                                        // WallTool, huecos, arrastre de esquinas y huecos, snapping
  → Document::execute(comando)                          // valida (huecos: también contra las uniones, ADR-0013),
                                                        // aplica y registra la transacción (ADR-0006);
                                                        // el ChangeSet se cuenta, sin recompilar a mitad de acción
  → compileScene(project, overlay)                      // al terminar Edit: proyecto como quedaría, planta a planta:
                                                        // uniones, habitaciones, suelos, forjados y carpintería
                                                        // (ADR-0009, 0011, 0013)
  → FFloor101SceneReconciler::Reconcile(scene)          // mismo actor por SceneNodeId; malla regenerada, escala 1;
                                                        // estilo por materiales tintados
```

Un movimiento del cursor recorre el mismo camino sin `Document::execute`: solo cambia el overlay (vista previa, asas).
Los cambios que llegan fuera de una edición (consola, toolset MCP, abrir archivo) recompilan en el momento.
Hoy se recompila la escena completa en cada cambio; la reconciliación por ID y hash evita tocar los actores que no
cambian y es la base del recompilado incremental.

## Geometry Engine

`WallGeometryBuilder(wall) → MeshData`. Pared recta con línea de referencia en el **eje** y espesor centrado.
`MeshData` guarda posiciones y normales en mm (`double`), UV en metros reales (1,0 = 1000 mm, para que los materiales
arquitectónicos tengan escala física) e índices de triángulos agrupados en **secciones por cara**: izquierda, derecha,
superior, inferior, inicio y fin, más `reveal` (el interior de los huecos). Así cada cara puede llevar su propio
material. Caras con sombreado plano: 24 vértices y 12 triángulos por pared sin huecos.

## Unreal Adapter (ADR-0005)

- Un actor por nodo de escena (`AFloor101SceneNodeActor`), **transitorio** (`RF_Transient`): nunca se guarda en un
  nivel. Lleva la etiqueta `f101.node:<SceneNodeId>` para localizarlo por MCP (`find_actors` por tag) y en tests.
- La malla va en un **`UDynamicMeshComponent`**, que en UE 5.8 aporta geometría de ray tracing y genera en ejecución las
  *mesh cards* de Lumen a partir del bounding box (`UpdateLumenCardsFromBounds`). `ProceduralMeshComponent` solo aporta
  ray tracing sin cards, así que las paredes no rebotarían bien la luz en Lumen.
- La reconciliación crea, actualiza o elimina actores comparando los `SceneNodeId`. La geometría nunca se escala: se
  regenera.
- **Materiales (ADR-0012):** cada sección de un nodo lleva su acabado; `UFloor101FinishLibrary` crea una instancia de
  `M_Floor101Finish` (generado desde código por `Scripts/GenerateAssets.ps1`) y una textura sintetizada por acabado. Los
  estilos de vista tiñen el mismo material. Los acabados translúcidos (vidrio, ADR-0013) usan `M_Floor101Glass`, generado
  igual.

## Uniones y habitaciones (MVP 2, ADR-0009)

- **Geometría derivada:** en cada compilación el Geometry Engine resuelve cómo se unen los muros (inglete entre
  extremos que coinciden, corte en la cara del muro anfitrión en las T, relleno en nudos de tres o más) y detecta las
  habitaciones como caras acotadas del grafo de ejes, con su suelo (caras interiores) y su área neta.
- **Tramos editables guardados (ADR-0025):** crear o modificar ejes divide las paredes de la misma planta en sus T y
  cruces. Cada tramo tiene su ID, huecos y acabados; las divisiones son parte de la misma transacción. El Geometry Engine
  transfiere acabados de habitaciones por un proveedor inyectado en `Document`, sin invertir dependencias. Abrir un
  proyecto antiguo no lo divide automáticamente. Ver [división de paredes](wall-splitting.md).
- **Esquinas:** `MoveJointCommand` mueve de una vez todos los extremos que comparten un punto; es lo que hace arrastrar
  el asa de una esquina.
- **Vista previa = proyecto como quedaría:** el overlay sustituye o añade muros y se compila todo, así que ingletes,
  habitaciones y áreas siguen al gesto en vivo.

## Puertas y ventanas (MVP 3, ADR-0010)

- **Dentro del muro:** `Wall::openings` (puerta o ventana: posición a lo largo del eje desde el inicio del muro, ancho,
  alto, antepecho; mm enteros). Borrar o deshacer el muro arrastra sus huecos. Los invariantes (cabe en el muro, deja
  dintel, puertas en el suelo, sin solapes) son parte de la validación del muro.
- **Malla directa, sin booleanas:** las caras largas se parten alrededor de los huecos, las puertas cortan la cara
  inferior y las mochetas cierran el hueco; la luz de Lumen pasa por él.
- **`project.json` esquema 2** (`openings` por muro); los archivos de esquema 1 se siguen leyendo.

## Plantas y forjados (MVP 4, ADR-0011)

- **Plantas apiladas:** `Project::levels` de abajo arriba (`Level`: altura suelo a suelo, espesor de forjado, nombre
  opcional); la cota de cada una se deriva de las de abajo. Cada muro tiene su planta y geometría local a ella.
- **Todo por planta:** uniones y habitaciones se resuelven planta a planta y se suben a su cota. **Forjado derivado**
  por habitación en la franja alta de su planta (techo de la habitación, suelo de la de arriba); los muros nuevos van
  de suelo a suelo y siguen la altura de su planta si cambia.
- **Edición en la planta activa:** el controlador baja el rayo a la cota de la planta activa y solo ve sus muros (más
  la planta de abajo como guía de snapping). La vista oculta las plantas de arriba y los forjados de la activa y atenúa
  las de abajo; "Ver todas" muestra el edificio entero.
- **`project.json` esquema 3** (`levels`, `level` por muro); los archivos 1 y 2 se leen en una sola planta.

## Acabados (MVP 5, ADR-0012)

- **Catálogo en el núcleo:** 11 acabados con id estable, superficies que admiten y aspecto neutral (color, rugosidad,
  patrón de ladrillo, baldosa, tablas o moteado). El modelo guarda ids: por cara de muro y, por clave de habitación, su
  suelo y su techo (las habitaciones siguen siendo derivadas).
- **Escena y texturas:** la escena asigna un acabado a cada sección (caras del muro, suelo, cara inferior del forjado)
  y el Scene Model sintetiza sus texturas (sin costuras, con mips) para cualquier consumidor.
- **Unreal:** un único material base generado por un commandlet; una instancia por acabado, compartida.
- **`project.json` esquema 4** (`finishes` por muro, `roomFinishes`).

## Carpintería y vidrio (MVP 6, ADR-0013)

- **Derivada del hueco:** marco centrado en el grueso del muro; puertas con hoja cerrada y manillas; ventanas con
  travesaño, vidrio y montante si son anchas. Un nodo de escena por hueco; nada nuevo se guarda salvo el acabado.
- **Acabado por hueco:** `Opening::finish` (lacado blanco por defecto, maderas, aluminio antracita); el vidrio es un
  acabado translúcido del catálogo (`opacity`) que Unreal pinta con `M_Floor101Glass`.
- **Uniones:** el Geometry Engine da los tramos de cada muro ocupados por los muros que se le unen (ingletes, T,
  cruces) y el `Document` los recibe inyectados: añadir, mover o ensanchar un hueco sobre ellos se rechaza.
- **Ratón:** las herramientas de hueco lo colocan en el tramo libre más cercano; un hueco seleccionado se arrastra
  (mover) o se estira por sus asas (ancho), con vista previa y un solo cambio deshacible.
- **`project.json` esquema 5** (`finish` por hueco).

## Tipos de puertas y ventanas (MVP 7, ADR-0014)

- **Cómo abre cada hueco:** operación (abatible, de dos hojas, corredera, fija), extremo de las bisagras, cara hacia la
  que abre y apertura en % (solo cambia cómo se muestra). Las puertas correderas son empotradas: su bolsillo, tan largo
  como la puerta, ocupa muro y se valida como un hueco más (dentro del muro, sin pisar huecos ni uniones).
- **Geometría:** hojas y hojas de ventana giran en un marco local alrededor de la bisagra o se desplazan por sus vías.
- **Símbolo de apertura:** arcos de las puertas abatibles en el suelo, solo en la escena de edición (estilo `Symbol`).
- **Edición:** la puerta abre hacia el lado del muro al que apunta el cursor; Espacio cambia las bisagras; panel con
  tipo, bisagras, sentido y apertura.
- **`project.json` esquema 6** (`operation`, `hinge`, `opensTo`, `openPercent`).

## Planos 2D (MVP 8, ADR-0015)

- **Un dibujo puro:** `Floor101Drawing` convierte la planta en rellenos, trazos y rótulos en mm: muros cortados a 1,20 m,
  símbolos de ventanas y puertas, cotas a ejes (cadena por los huecos y total, por el lado sin habitación), nombre y área
  de cada habitación, y las marcas de edición. De ahí salen la pantalla, el SVG (puro) y el PNG.
- **Vista 2D editable:** Tab pone una cámara ortográfica recta hacia abajo, apaga el render 3D y una capa Slate pinta el
  plano con la proyección de la cámara. La entrada es la misma: los rayos del cursor bajan en vertical al
  `EditController`, así que todo se edita igual que en 3D y el plano se recalcula en cada refresco.
- **Exportación:** SVG y PNG de la planta junto al proyecto, sin marcas de edición (botones, consola, MCP).

## Mobiliario (MVP 9, ADR-0016)

- **Modelo mínimo:** cada mueble guarda su tipo del catálogo (12: camas, sofá, mesa, silla, armario, cocina, frigorífico,
  inodoro, lavabo, bañera, plato de ducha), su planta, el centro de su huella, el giro en centésimas de grado enteras,
  ancho, fondo, alto y acabado; se valida contra los rangos de su tipo. Comandos propios y `project.json` esquema 7.
- **Forma derivada:** el Geometry Engine genera cajas por tipo (con cubetas abiertas) y los contornos de detalle del
  plano; ranuras de material para el cuerpo (acabado del mueble) y partes fijas (lino, cerámica, acero, piedra...).
- **Escena y plano:** un nodo por mueble elevado a su planta, con selección y vista previa; en el plano, su contorno y
  detalles en gris.
- **Ratón:** modo Amueblar (A) con paleta; el mueble sigue al cursor en la rejilla y, si va contra un muro, se apoya de
  espaldas en la cara más cercana y se detiene en los muros con que choca; Espacio gira 90°. En Seleccionar se elige,
  arrastra (mismo snapping), gira, edita y borra.

## Assets importados (MVP 10, ADR-0017)

- **Texturas y modelos del usuario:** una imagen PNG/JPEG se usa como acabado (con el tamaño que cubre una repetición y
  su rugosidad) y un modelo glTF 2.0 u OBJ como tipo de mueble (con su unidad, eje arriba, frente y si se apoya en los
  muros). Los dos se nombran `asset:<uuid>`, así que todo lo que acepta un acabado o un tipo de mueble los acepta; la
  validación que depende de ellos recibe el proyecto. `project.json` esquema 8 guarda la biblioteca (`assets`).
- **Archivos junto al proyecto:** importar copia el archivo y sus dependencias (MTL, buffers, texturas; las imágenes
  empaquetadas se escriben aparte) en `<carpeta del proyecto>/assets/<uuid>/`; el modelo guarda rutas relativas y dentro
  de esa carpeta. Guardar en otra carpeta copia los assets; borrar un asset en uso se rechaza.
- **Lectura pura:** `Floor101Assets` lee y normaliza los modelos (Z arriba, mm, frente hacia −y, centrado en su huella)
  sin Unreal; el Scene Model los recibe por un `ModelProvider` inyectado y los estira por eje a las medidas de cada mueble
  (una caja si el modelo no se puede leer). Cada material del modelo es un acabado derivado (`asset:<uuid>#<i>`) salvo
  que el mueble tenga el suyo.
- **Unreal:** el subsistema registra los acabados de las imágenes y de los materiales del modelo en la librería de
  acabados antes de compilar; la UI importa con el diálogo de archivo nativo de Windows (Pintar: texturas; Amueblar:
  modelos) y edita sus ajustes.

## Catálogo con variantes, biblioteca y Modo Foto (MVP 11, ADR-0018, ADR-0019)

- **Acabados por categoría:** 80 acabados en el núcleo (pinturas, baldosas, madera, piedra, moquetas, carpintería,
  tejidos, vidrio), con los patrones nuevos espiga y ajedrezado sintetizados en el Scene Model. Pintar tiene pestañas
  Paredes/Suelos y el `EditController` decide el destino por el acabado (`paintTarget`): la cara del muro, o el suelo de
  la habitación a través de los muros si el acabado es solo de suelo.
- **Biblioteca integrada:** `Library/` trae 53 muebles modelados por código en Blender (GLB con imagen) y 14 texturas
  fotográficas CC0, descritos por manifiestos que lee `Floor101Assets`. Usar un elemento lo copia al proyecto como un
  asset que recuerda su id (`Asset::library`, `project.json` esquema 9), una vez por proyecto: el proyecto no depende de
  la biblioteca. Amueblar agrupa por categoría los tipos paramétricos y los modelos con su imagen.
- **Modo Foto:** R cambia el viewport del juego al path tracer de Unreal (`VMI_PathTracing`) y la escena se compila sin
  marcas de edición; el panel muestra las muestras acumuladas (leídas de la vista por una extensión), la calidad
  (64/256/1024 muestras) y guarda el PNG junto al proyecto sin la UI.

## Exportación para Twinmotion (MVP 12, ADR-0020)

- **Un `.glb` desde la escena neutral:** `Floor101Exporters` (puro) compila todo el proyecto sin estado de vista y lo
  escribe como glTF 2.0 binario: plantas, grupos por clase y un objeto por nodo con nombres estables; un material por
  acabado con su imagen (texturas sintetizadas codificadas en PNG por el propio módulo, fotografías e imágenes
  importadas como su archivo) y la repetición horneada en los UV. Metros con Y arriba.
- **Twinmotion solo presenta:** recibe una copia; cada cambio se exporta de nuevo y se refresca la importación (con "Use
  scene materials" conserva sus materiales). Direct Link queda para después y reutilizará esta conversión.
- **Aplicación:** botón "Twinmotion (.glb)…" (diálogo para elegir dónde; propone la última carpeta usada, si no junto al proyecto, `<proyecto>.glb`), consola `f101.Export.Gltf` y MCP
  `ExportGltf`; nombres en el idioma de la UI. `Scripts/CheckGltf.ps1` da una segunda opinión con Blender.

## Pilares e instalaciones (MVP 13, ADR-0021)

- **Modelo:** `Column` (planta, centro, rectangular o redondo, ancho, fondo, giro, acabado de muro; tan alto como su
  planta) y `ServiceRun` (planta, tipo de red y una polilínea de puntos `x, y, z` con z sobre el suelo de su planta). El
  tipo fija color y diámetro: electricidad verde 25 mm, agua blanca 25 mm, aguas negras amarilla 110 mm, gas dorada
  22 mm (acabados del catálogo para `Surface::Services`). `project.json` esquema 10.
- **Derivado:** caja o cilindro para el pilar; tubo con codos esféricos y extremos planos para el tramo
  (`ServicesBuilder`). En la escena, los pilares siempre; los tramos con la capa de instalaciones, que deja muros,
  suelos, forjados y carpintería en `Ghost`; en el plano, pilares cortados y tuberías con vaina oscura bajo su color.
- **Ratón:** Pilar (C) se ajusta a esquinas, ejes de muro y rejilla; Instalación (I) dibuja por tramos a la altura
  elegida (bajo el suelo, pared, techo o escrita), con subidas verticales al cambiar de altura; doble clic o Enter
  terminan. X muestra u oculta la capa; con ella el suelo del mapa también se vuelve translúcido, y la fila "Redes"
  elige qué redes se ven (electricidad, agua, aguas negras, gas, todas o ninguna) en 3D, en el plano y en su exportación.

## Interacción y UI (MVP 1–13, ADR-0008)

- **Lógica en el núcleo:** `EditController` (modos Seleccionar/Muro/Puerta/Ventana/Pintar/Amueblar, selección de muros,
  huecos, muebles y habitaciones, arrastre de esquinas, huecos y muebles, propiedades, acabados, deshacer/rehacer) y
  `WallTool` (clic, vista previa,
  longitud escrita, Enter) viven en el CAD Core y se prueban sin Unreal. Las habitaciones se localizan con un
  `RoomLocator` inyectado desde el Geometry Engine. El snapping también: extremos, eje de otro muro (T; donde lo cruza el ángulo), ángulo cada 15°, rejilla de
  50 mm. Si el rayo del cursor toca un muro, el punto de planta es el que queda bajo el impacto, no el suelo de detrás.
- **Unreal traduce:** `UFloor101GameViewportClient` convierte el cursor en rayo de escena, las teclas en comandos
  (`MapKey`) y los caracteres en texto de la herramienta; `AFloor101EditingPawn` es una cámara orbital (derecho orbita,
  central desplaza, rueda acerca, F encuadra, T vista en planta; sigue a la planta activa). RePág/AvPág cambian de planta,
  L muestra todas, M elige Pintar, A elige Amueblar, C Pilar, I Instalación, X la capa de instalaciones, Espacio cambia las bisagras del hueco que se coloca o del
  seleccionado (o gira 90° el mueble o el pilar que se coloca o el seleccionado), Tab alterna el plano 2D y R el Modo Foto. Mientras se escribe una longitud, las letras de sus unidades (m, c) son texto. `AFloor101GameMode` y el viewport client se configuran
  en `Config/DefaultEngine.ini` y rigen también en PIE.
- **UI:** Slate en C++ (`SFloor101Overlay`, textos en español): barra con modos, deshacer/rehacer, encuadrar, cenital,
  plano 2D, archivo actual, Guardar, "Guardar como…" y "Abrir…" con los diálogos de Windows (recuerdan la última carpeta), exportar el plano (SVG, PNG), Foto y "Twinmotion (.glb)", y la fila de plantas (planta activa con su cota, subir, bajar, nueva planta, ver
  todas); panel de propiedades del muro seleccionado (longitud, espesor, altura, acabado de cada cara nombrada por su
  orientación, eliminar), del hueco seleccionado (ancho, alto, antepecho, posición, tipo, bisagras, sentido, apertura,
  carpintería, eliminar), del mueble seleccionado (ancho, fondo, alto, giro, girar 90°, acabado, eliminar), del pilar
  seleccionado (forma, medidas, giro, acabado), de la tubería seleccionada (tipo, longitud, puntos, alturas), de la
  habitación seleccionada (área, suelo, techo), el tipo de las puertas o ventanas que se van a colocar (en Puerta y Ventana), la forma y medidas del pilar (en Pilar), el tipo y la altura de la tubería (en Instalación), la paleta de
  Pintar (pestañas Paredes/Suelos, secciones por categoría con las fotografías de la biblioteca, texturas importadas e
  "Importar textura…"), el catálogo de Amueblar (pestañas por categoría con fichas con imagen, modelos importados e
  "Importar modelo…") o, sin selección, la planta activa (nombre, altura, forjado, eliminar); en Modo Foto, solo el
  panel de la foto (muestras, calidad, guardar imagen, volver);
  barra de estado con ayuda contextual y errores; etiqueta junto al cursor con la longitud y lo escrito (o la
  posición y el ancho del hueco arrastrado); área de cada
  habitación sobre su suelo. La UI solo llama al subsistema; nunca toca actores.
- **Vista:** selección (naranja), vista previa (azul), asas (amarillo) y símbolos de apertura (líneas oscuras) son un
  `ViewOverlay` que se compila con el proyecto y nunca se guarda.
- **Archivo:** `project.json` (ADR-0007) y, si hay assets, la carpeta `assets/` a su lado (ADR-0017); ruta por defecto
  `Documentos/101st Floor/project.json`; Ctrl+S / Ctrl+Shift+S / Ctrl+O (diálogos de Windows) o `f101.Project.Save|Open [ruta]`; `f101.Asset.Import <ruta>`; `f101.Export.Gltf [ruta]` (ADR-0020).

## Tests

| Nivel | Qué | Herramienta | Se ejecuta con |
|---|---|---|---|
| Unitarios (sin Unreal) | CadCore, Geometry, SceneModel, Drawing, Assets, Exporters (`Tests/Unit/`, exportación en `PngTests.cpp` y `GltfExportTests.cpp`, mobiliario en `FurnitureTests.cpp`, pilares e instalaciones en `ServicesTests.cpp`, assets en `AssetTests.cpp` y `AssetImportTests.cpp` con muestras exportadas con Blender en `Tests/Data/Assets`, biblioteca en `LibraryTests.cpp`, que también comprueba la `Library/` real) | CMake + Ninja (VS Build Tools) + MSVC 14.50 + **doctest** 2.5.3 (`ThirdParty/doctest`) | `Test.ps1` (segundos) |
| Arquitectura | módulos puros sin cabeceras ni tipos de Unreal; `Build.cs` solo con `Core` y módulos puros | script + compilación CMake | `Test.ps1` |
| Integración Unreal | adaptador (`Floor101.Adapter.*`): actor, componente, acabados (material base, textura, material por sección), bounds 355×12×240 cm, triángulos según `GetTriNormal`, regeneración a 420 cm sin escalar, estilos de vista, muro con puerta (7 secciones); interacción (`Floor101.Interaction.*`): mapa de teclas, cámara, flujo completo por el subsistema con `project.json`, habitación con suelo, área y arrastre de esquina, plantas (forjado, planta de abajo atenuada, muro dibujado en la cota de su planta, ocultar y ver todas), acabados (pintar y seleccionar habitación), carpintería (vidrio translúcido, rechazo en una T, arrastre, Esc), tipos de hueco (hoja abierta, símbolo sin sombra, colocar con Espacio, corredera), plano 2D (dibujo, cámara ortográfica, exportación SVG/PNG), muebles (colocar contra un muro, seleccionar, arrastrar, girar, borrar, plano, guardar y abrir), assets (importar textura, GLB y OBJ, pintar, colocar, materiales e imágenes, guardar en otra carpeta y reabrir), biblioteca (silla y laminado de la `Library/` real, una vez por proyecto), Modo Foto (sin marcas de edición), exportación para Twinmotion (el `.glb` releído y su PNG decodificado por Unreal igual que la textura de la aplicación), instalaciones (pilar en la esquina, capa con muros y suelo del mapa translúcidos, tubería por clics, plano y SVG, guardar y abrir); toolset (`Floor101.Editor.*`) | Unreal Automation (`UnrealEditor-Cmd -nullrhi`) o MCP `AutomationTestToolset` | `Test.ps1 -Integration` |
| Render | DX12/SM6/HWRT/Lumen y path tracing del Modo Foto | `CheckRendering.ps1` | manual o en la validación de cada fase |
| MCP | handshake, toolsets, lectura | `CheckMcp.ps1` | manual o en la validación de cada fase |

Los módulos puros se compilan en CMake con `/W4 /WX /permissive- /EHs-c-`, igual o más estricto que Unreal: un `throw`
no compila. Se descarta Low Level Tests de UE: exige compilar targets de programa contra el motor, es lento y acopla los
tests a la instalación de Unreal.

## Scripts

`Setup.ps1` (comprobaciones de la Fase A automatizadas) · `Build.ps1` (UBT) · `Test.ps1` (unitarios → arquitectura →
integración) · `Run.ps1` (app en `-game`) · `CreateDesktopShortcut.ps1` (acceso directo a `Run.ps1` en el escritorio) ·
`GenerateAssets.ps1` (los materiales de acabados y vidrio, desde código) · `GenerateLibrary.ps1` (los muebles de la
biblioteca, con Blender) · `CheckGltf.ps1` (un `.glb` exportado, importado y renderizado en Blender) · `RunEditorMcp.ps1` · `CheckRendering.ps1` · `CheckMcp.ps1` ·
`Clean.ps1` · `Package.ps1` · `CheckPackage.ps1` · `Installer.ps1`. Todos localizan motor, CMake y MSVC con
`vswhere`/registro, nunca por `PATH` (hay un `cmake` ajeno en el `PATH` de esta máquina).

## Render / RTX

Modo Diseño: configuración validada en la Fase B (DX12, SM6, Lumen HWRT, VSM, TSR). Riesgo a verificar en MVP 0: que las
paredes generadas contribuyan a la GI de Lumen (cards por bounding box). Si no basta, alternativa documentada:
`r.Lumen.HardwareRayTracing.LightingMode=2`, que evalúa la iluminación en el impacto del rayo, en las reflexiones.
Modo Foto (MVP 11, ADR-0018): el path tracer de Unreal en el mismo viewport, progresivo y con denoiser. Nanite se
reserva para assets densos (Blender); las paredes son geometría simple.

## Estado de implementación (MVP 13, 2026-09-30)

MVP 0: los cinco módulos de producto más `Floor101Editor` y la pared de referencia 3550 → 4200 mm por todo el pipeline
(`docs/mvp0-report.md`). MVP 1: herramienta de muro interactiva con longitud escrita, selección, borrar, longitud,
espesor y altura, arrastre de extremos, deshacer/rehacer, `project.json`, cámara orbital y UI Slate
(`docs/mvp1-report.md`). MVP 2: uniones de muros (L, T, X, espesores distintos), habitaciones con suelo y área neta,
arrastre de esquinas compartidas, snapping a muros y vista en planta, verificados con tests, MCP y ratón y teclado
reales (`docs/mvp2-report.md`). MVP 3: puertas y ventanas como huecos del muro (herramientas, selección, propiedades,
deshacer, `project.json` esquema 2, MCP) (`docs/mvp3-report.md`). MVP 4: plantas apiladas con forjados derivados, edición
por planta, `project.json` esquema 3 y herramientas MCP de plantas (`docs/mvp4-report.md`). MVP 5: acabados de un
catálogo por cara de muro, suelo y techo, herramienta Pintar, texturas sintetizadas, material generado desde código y
`project.json` esquema 4 (`docs/mvp5-report.md`). MVP 6: carpintería derivada (marcos, hojas, manillas, vidrio
translúcido), huecos rechazados en las uniones, colocación en el tramo libre, arrastre y ancho de huecos con el ratón y
`project.json` esquema 5 (`docs/mvp6-report.md`). MVP 7: tipos de puertas y ventanas (abatibles, de dos hojas,
correderas empotradas con bolsillo validado, correderas y fijas), bisagras, sentido y apertura, símbolo de apertura y
`project.json` esquema 6 (`docs/mvp7-report.md`). MVP 8: planos 2D con cotas y rótulos (módulo puro
`Floor101Drawing`), vista 2D editable con cámara ortográfica y exportación SVG/PNG (`docs/mvp8-report.md`). MVP 9:
mobiliario paramétrico (12 tipos, snapping contra muros, 3D y plano, `project.json` esquema 7) (`docs/mvp9-report.md`).
MVP 10: assets importados (texturas PNG/JPEG y modelos glTF/OBJ copiados junto al proyecto, usados como acabados y
muebles, diálogo de archivo nativo, `project.json` esquema 8) (`docs/mvp10-report.md`). MVP 11: Modo Foto con path
tracing, catálogo con variantes (80 acabados por categoría, Pintar suelos con pestañas), biblioteca integrada de 53
muebles y 14 texturas fotográficas, tipo de puerta o ventana elegido antes de colocarla y `project.json` esquema 9
(`docs/mvp11-report.md`). MVP 12: exportación para Twinmotion en glTF binario (módulo puro `Floor101Exporters`,
botón, consola y MCP) (`docs/mvp12-report.md`). MVP 13: pilares rectangulares y redondos e instalaciones de
electricidad, agua, aguas negras y gas dibujadas por tramos, capa con rayos X y `project.json` esquema 10
(`docs/mvp13-report.md`). Detalle por capa: `docs/cad-core.md`, `docs/geometry.md`,
`docs/scene-model.md`, `docs/drawing.md`, `docs/exporters.md`, `docs/unreal-integration.md`.

## Fuera del MVP 13 (la arquitectura no lo bloquea)

Escaleras y huecos en forjados, sótanos, voladizos y terrazas, cubiertas inclinadas, forjados editables, puertas
plegables y oscilobatientes, correderas de superficie, huecos no rectangulares, propiedades adicionales
de habitaciones (los nombres ya se guardan por clave, ADR-0029), acabados con relieve (mapas de normales), fotos a otra resolución o con
Movie Render Queue, sincronización en vivo con Twinmotion (Datasmith Direct Link), exportación USD, más formatos de import (FBX, USD, extensiones glTF como Draco o KTX2), mapas de
normales y metal de los modelos, biblioteca del usuario compartida entre proyectos, cotas a caras e interiores, cajetín y
escala gráfica en los planos, exportación PDF/DXF, diálogo nativo para guardar y abrir proyectos, traducción de los
mensajes del núcleo, reasignación de teclas, recompilación incremental para proyectos grandes, pilares que corten
habitaciones o se unan a muros, trazado automático de instalaciones con pendientes, accesorios y cálculo de diámetros,
vigas y estructura de cubierta.
