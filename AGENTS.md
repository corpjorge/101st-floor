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

## Detailed Product and Development Rules

Producto visible: **101st Floor**. Proyecto Unreal y módulo C++: `Floor101`. Namespace propio: `floor101`/`f101`.
Antes de una feature importante leer `docs/architecture.md` (módulos, dependencias, flujo) y los ADRs en `docs/adr/`.
Otros: build `docs/build.md`, render `docs/rendering-rtx.md`, MCP `docs/unreal-mcp.md`.

## Arquitectura (no romper sin ADR)
- Fuente de verdad: el modelo de 101st Floor (CAD Core). Unreal solo representa; Twinmotion solo presenta.
- CAD Core, Geometry Engine, Scene Model, Drawing, Assets y Exporters: C++ estándar, sin tipos de Unreal, sin MCP,
  testeables sin Unreal.
- Unidades persistentes: milímetros enteros (`int64_t`). `double` solo como cálculo temporal con cuantización explícita.
- Sin lógica de producto en Blueprints. Sin pasos manuales en el Editor: todo reproducible desde Git + `Scripts/`.
- La interacción vive en `f101::cad::EditController` (puro); Unreal solo traduce la entrada y muestra (ADR-0008).
- Uniones de muros y habitaciones se derivan de los muros en cada compilación; no se guardan (ADR-0009).
- Puertas y ventanas viven dentro de su muro (`Wall::openings`, medidas a lo largo del eje); `project.json` esquema 2
  (ADR-0010).
- Plantas apiladas con cota derivada; cada muro en una planta (geometría local a ella); forjados derivados por
  habitación; se edita en la planta activa. `project.json` esquema 3 (ADR-0011).
- Acabados: ids de un catálogo en el núcleo, por cara de muro y por clave de habitación; texturas sintetizadas en el
  Scene Model; el material base se genera desde código (`Scripts/GenerateAssets.ps1`). `project.json` esquema 4
  (ADR-0012).
- Carpintería (marco, hoja, manillas, vidrio) derivada de cada hueco; solo se guarda su acabado (`Opening::finish`).
  Los huecos no pueden pisar uniones (el `Document` recibe `WallObstructions` del Geometry Engine). Vidrio con
  `M_Floor101Glass`, generado igual. `project.json` esquema 5 (ADR-0013).
- Tipos de hueco: operación, bisagras, cara y apertura % en `Opening`; las puertas correderas son empotradas y su
  bolsillo (`occupiedSpan`) se valida como el hueco; los arcos de apertura solo existen en la escena de edición.
  `project.json` esquema 6 (ADR-0014).
- Planos 2D: el módulo puro `Floor101Drawing` dibuja la planta (corte, carpintería, cotas a ejes, habitaciones) y su
  SVG; la vista 2D es una cámara ortográfica + capa Slate (la edición sigue en el viewport); el PNG pinta la misma capa
  con `FWidgetRenderer` (ADR-0015).
- Mobiliario: tipo del catálogo del núcleo + planta, centro, giro en centésimas de grado enteras, medidas y acabado; la
  forma (cajas) se deriva en el Geometry Engine; snapping contra muros en el `EditController`. `project.json` esquema 7
  (ADR-0016).
- Assets importados: texturas (PNG/JPEG) y modelos (glTF/GLB/OBJ) se leen en el módulo puro `Floor101Assets` y se copian
  en `<carpeta del proyecto>/assets/<uuid>/`; se referencian como `asset:<uuid>` (acabado o tipo de mueble) y el
  proyecto solo guarda rutas relativas dentro de esa carpeta. `project.json` esquema 8 (ADR-0017).
- Biblioteca integrada: `Library/<carpeta>/manifest.json` (muebles GLB generados con `Scripts/GenerateLibrary.ps1` en
  Blender, texturas CC0); usar un elemento lo copia al proyecto como asset con `Asset::library`, una vez por proyecto.
  Acabados y muebles con categoría; Pintar decide pared o suelo por el acabado (`paintTarget`). `project.json` esquema 9
  (ADR-0019).
- Modo Foto: path tracer de Unreal en el viewport del juego (R), escena sin marcas de edición, PNG sin UI; se valida con
  `CheckRendering.ps1` (ADR-0018).
- Exportación a Twinmotion: el módulo puro `Floor101Exporters` escribe la escena neutral como `.glb` (metros, Y arriba
  `(x, z, −y)/1000`, nombres estables, ids en `extras`, repetición de las texturas horneada en los UV, PNG propio);
  Twinmotion solo recibe copias. Segunda opinión: `Scripts/CheckGltf.ps1` con Blender (ADR-0020).
- Pilares (`Column`: rectangulares o redondos, tan altos como su planta) — en 3D 1 mm más altos y anchos (`scene::ColumnProudMm`) para quedar encima de muros y forjados sin parpadeo — e instalaciones (`ServiceRun`: polilínea 3D con z
  sobre el suelo de su planta; color y diámetro fijados por la red: electricidad, agua, aguas negras, gas). Los tramos
  solo se ven con la capa de instalaciones (X), que deja la arquitectura en `Ghost`; los exportadores los llevan siempre.
  `project.json` esquema 10 (ADR-0021).
- Pantalla: HDR opcional por preferencia local (fotos EXR con HDR, PNG sin él), pantalla completa nativa y render al
  100 % sin resolución dinámica (ADR-0022, ADR-0023).
- Cámara: órbita, Navegar (N) y WASD/QE con velocidad por niveles; solo cambia la vista, nunca el modelo (ADR-0024).
- Al crear o mover ejes, las paredes de la planta se dividen en tramos en sus T y cruces, en la misma transacción; los
  huecos conservan su posición y un corte que los atraviese se rechaza. Abrir un proyecto no lo divide. Acabados,
  nombres y rodapiés de habitación pasan a las claves nuevas por un proveedor del Geometry Engine (ADR-0025).
- Interfaz de estudio en Slate/C++ con modos Diseñar, Recorrer y Foto (ADR-0026). Arranque propio en `Floor101Startup`
  (ADR-0027).
- Sistema de diseño Cota Cero (ADR-0033): colores, tipos y medidas solo desde `Floor101Style`; componentes en
  `Floor101Ui` e iconos en `SFloor101Icon`. Nada de colores ni tamaños literales en las pantallas; cada control conserva
  la llamada que ejecuta. Fuentes de Windows leídas en su sitio (Segoe UI, Bahnschrift), sin redistribuirlas.
- Registro: `F101_LOG` (es `UE_LOG` en Development; en Shipping escribe el log propio `Floor101-product.log`). No activar
  `bUseLoggingInShipping`: rompe la ABI del motor instalado. Distribución Shipping, paquete y MSI/Burn (ADR-0028).
- Por clave de habitación, separados de los acabados: nombres (`roomNames`, esquema 11, ADR-0029) y rodapiés
  (`roomBaseboards`, esquema 12, ADR-0030).
- Cotas: anotaciones por planta en `Project::dimensions`; las automáticas se recalculan del modelo (id
  `auto/<muro>/…`) y lo guardado solo cambia separación, rótulo y visibilidad. Medidas de las cifras compartidas por
  el picking y el dibujo. `project.json` esquema 13 (ADR-0031).
- Arrastrar el cuerpo de una pared la traslada con sus uniones (`TranslateWallCommand`), sin cambio de esquema
  (ADR-0032).
- Cotas interiores automáticas de cara a cara: una por tramo recto del suelo de cada habitación (`Room::sides`), salvo la
  cara con la cota de partición y la repetida de dos caras opuestas; el nombre y el área se apartan de sus cifras.
  Vista → Cotas interiores las oculta (estado del `EditController`, preferencia local). Sin cambio de esquema (ADR-0034).
- Rótulos de habitación por clave (`roomLabels`): altura del nombre y del área y, si se arrastraron en el plano, el
  desplazamiento de cada texto desde `Room::labelPoint`; los movidos no se apartan de las cifras. El `EditController` los
  elige por las cajas que publica el dibujo (`PlanDrawing::roomTexts`). `project.json` esquema 14 (ADR-0035).
- MCP es herramienta de desarrollo, nunca lógica de producto. Llamadas MCP de Unreal siempre en serie.
- Catálogo paramétrico de puertas y ventanas: 24 familias en `openingCatalogue()` (12 por clase), id estable en
  `Opening::family`, vacío para el aspecto anterior. Geometría y miniaturas derivadas del mismo diseño; búsqueda,
  filtro de apertura y medidas antes de colocar. Cambiar familia conserva medidas y acabado; cambiar mecanismo
  elimina una familia incompatible. `project.json` esquema 15; MCP `GetOpeningFamilies`/`SetOpeningFamily` (ADR-0036).
- Luminarias: `Furniture::elevationMm` y emisión puntual opcional en coordenadas locales (`FurnitureLight`, lúmenes,
  radio, color y encendido). La luz sigue las transacciones del mueble; SceneModel la deriva y Unreal reconcilia
  `APointLight` transitorios. El GLB lleva `KHR_lights_punctual`. `project.json` esquema 16 (ADR-0037).
- Cubiertas translúcidas por clave de habitación (`roomRoofs`): sustituyen la losa solo en la última planta, con
  tejas onduladas y perfilería derivadas por Geometry. Vidrio independiente en `Opening::glazing`; recubrimiento
  reflectivo PBR compartido por Unreal y GLB. `project.json` esquema 17 (ADR-0038).

## Entorno
- Windows 11 + RTX 5070 Ti + UE 5.8.x (Launcher). Scripts en PowerShell 7: `Scripts/*.ps1`.
- Nunca invocar `bash` desde `.ps1` (en PowerShell resuelve a WSL). Localizar el motor con `Scripts/Common.ps1`.
- Compilar: `.\Scripts\Build.ps1` (con el editor cerrado; abierto → Live Coding vía MCP). Validar render: `.\Scripts\CheckRendering.ps1`.
- Tests: `.\Scripts\Test.ps1` (arquitectura + unitarios CMake, segundos); `-Integration` añade Unreal Automation.
- Distribución: `.\Scripts\Package.ps1`, `.\Scripts\CheckPackage.ps1` (ejecutable Shipping real) e `.\Scripts\Installer.ps1`;
  detalle en `docs/distribution.md`.
- Editor + MCP: `.\Scripts\RunEditorMcp.ps1`; comprobar: `.\Scripts\CheckMcp.ps1`. Detalle en `docs/unreal-mcp.md`.
- Para editar el documento por MCP: toolset `Floor101Editor.Floor101DocumentToolset` (plantas, paredes en mm, esquinas,
  habitaciones, puertas y ventanas con su carpintería, muebles, pilares, instalaciones y su capa, assets importados,
  biblioteca, acabados, vista por planta, cotas interiores visibles u ocultas,
  deshacer/rehacer, guardar/abrir, exportar el plano y el `.glb` para Twinmotion). Con PIE activo actúa sobre el documento de PIE; si no, sobre el del editor.
- Unreal MCP: descubrir con `list_toolsets`/`describe_toolset`, invocar con `call_tool`, verificar cada resultado.
  Commit antes de sesiones MCP largas o mutativas.
- Render: solo DX12/SM6; los formatos D3D11 están eliminados a propósito (un fallback debe fallar, no pasar desapercibido).

## Forma de trabajo
- Inspeccionar antes de modificar; compilar y ejecutar tests de verdad; no declarar éxito sin validación.
- No desactivar tests ni silenciar warnings para obtener verde; no desactivar features para esconder fallos.
- Decisiones importantes → ADR en `docs/adr/`. No crear ADRs para decisiones triviales.
- No empezar la siguiente fase o MVP sin que el usuario lo pida.
