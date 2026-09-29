# Arquitectura

Estado: **aprobada (Fase D, 2026-09-28); MVP 4 implementado.** Decisiones importantes en `docs/adr/`.

## Principio

101st Floor es la fuente de verdad. El modelo CAD (paramétrico, en milímetros enteros) genera geometría, la geometría
compila una escena neutral, y cada consumidor (Unreal, exportadores, vista 2D) representa esa escena. Nada fluye en
sentido contrario: Unreal no escribe en el modelo, y un nivel de Unreal nunca se guarda como dato del proyecto.

```
   UI Slate ─► EditController ─ comandos ─►  CAD Core  ◄── Asset System (futuro)
                                               │ cambios
                                               ▼
                                        Geometry Engine        (Wall → MeshData)
                                               │
                                               ▼
                                          Scene Model          (derivada, IDs deterministas)
                          ┌────────────────────┼────────────────────┐
                          ▼                    ▼                    ▼
                  Unreal Adapter        Scene Exporters         Vista 2D
                          │           (glTF → USD → Datasmith)   (futuro)
                          ▼
             Unreal Runtime (DX12 · Lumen HWRT)
```

## Módulos

Cinco módulos de producto y uno de desarrollo, solo para el editor. Los tres primeros son **C++20 estándar**: no
incluyen cabeceras de Unreal, no usan tipos de Unreal y compilan igual con UnrealBuildTool y con CMake (tests sin
Unreal). Ver ADR-0002.

| Módulo | Tipo | Namespace | Depende de | Contenido |
|---|---|---|---|---|
| `Floor101CadCore` | C++ puro | `f101::cad` | — (nlohmann/json interno) | unidades, `Point2D`/`Vector2D`/`Segment2D`/`Bounds`, IDs, `Project`/`Level`/`Wall`, validación, `Document` (comandos, deshacer/rehacer, notificación de cambios), snapping, picking, `WallTool`, `EditController`, `project.json` |
| `Floor101Geometry` | C++ puro | `f101::geometry` | CadCore | `MeshData` propio, uniones de muros (`solveWallJoins`), `WallGeometryBuilder` sobre huellas, polígonos y prismas, habitaciones (`detectRooms`) y suelos; más adelante huecos, techos |
| `Floor101SceneModel` | C++ puro | `f101::scene` | CadCore, Geometry | `Scene` (nodos y habitaciones), `SceneNode` (con `NodeStyle`), `SceneCompiler` (Project → muros unidos, uniones, suelos; con `ViewOverlay` que compila el proyecto como quedaría); más adelante materiales portables, luces, cámaras, `SceneDiff` |
| `Floor101UnrealAdapter` | Unreal | — | SceneModel, Engine, GeometryFramework | reconciliación `Scene` → actores con `UDynamicMeshComponent` (hash + estilo); estilos de vista con materiales tintados; conversión de unidades y ejes en ambos sentidos; tests de integración |
| `Floor101` | Unreal, módulo principal | — | CadCore, SceneModel, UnrealAdapter, Slate | aplicación: `UFloor101DocumentSubsystem` (posee `Document` y `EditController` de cada mundo; guardar/abrir), entrada (`UFloor101GameViewportClient`), cámara orbital, game mode, overlay Slate `SFloor101Overlay`, comandos de consola `f101.*`, `RenderCheck` |
| `Floor101Editor` | Unreal, **solo editor** | — | Floor101, CadCore, ToolsetRegistry | herramientas de desarrollo: toolset MCP `Floor101DocumentToolset` (paredes en mm, deshacer/rehacer, guardar/abrir; actúa en PIE si está activo). Nunca entra en el juego |

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
  levógira de Unreal se compensan (ADR-0003). Cada consumidor tiene una única conversión documentada (glTF, en el
  futuro: `(x, z, −y) / 1000` m).

## Identidad

- Entidades (`WallId`, `LevelId`, …): UUID v4 con tipo propio por entidad. Generador inyectable para que los tests sean
  deterministas. Nunca se usan índices como identidad.
- Nodos de escena: **derivados de forma determinista** de la entidad de origen y su rol (`<wallId>/body`). Recompilar
  la escena produce los mismos IDs, y eso permite reconciliar actores de Unreal, calcular un `SceneDiff` y, en el futuro,
  reimportar en Twinmotion sin perder el trabajo artístico.

## Flujo de un cambio (MVP 2–4)

```
clic / tecla / texto en el viewport                     // UFloor101GameViewportClient: rayo en mm, MapKey, caracteres
  → UFloor101DocumentSubsystem::Edit(acción)            // mensaje de estado para la UI
  → EditController (CAD Core)                           // planta activa (rayo bajado a su cota), modo, selección,
                                                        // WallTool, huecos, arrastre de esquinas, snapping
  → Document::execute(comando)                          // valida, aplica y registra la transacción (ADR-0006);
                                                        // el ChangeSet se cuenta, sin recompilar a mitad de acción
  → compileScene(project, overlay)                      // al terminar Edit: proyecto como quedaría, planta a planta:
                                                        // uniones, habitaciones, suelos y forjados (ADR-0009, 0011)
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

## Uniones y habitaciones (MVP 2, ADR-0009)

- **Derivadas, nunca guardadas:** en cada compilación el Geometry Engine resuelve cómo se unen los muros (inglete entre
  extremos que coinciden, corte en la cara del muro anfitrión en las T, relleno en nudos de tres o más) y detecta las
  habitaciones como caras acotadas del grafo de ejes, con su suelo (caras interiores) y su área neta. `project.json` no
  cambia.
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

## Interacción y UI (MVP 1–4, ADR-0008)

- **Lógica en el núcleo:** `EditController` (modos Seleccionar/Muro/Puerta/Ventana, selección de muros y huecos,
  arrastre de esquinas, propiedades, deshacer/rehacer) y `WallTool` (clic, vista previa, longitud escrita, Enter) viven en el CAD Core y se prueban sin
  Unreal. El snapping también: extremos, eje de otro muro (T; donde lo cruza el ángulo), ángulo cada 15°, rejilla de
  50 mm. Si el rayo del cursor toca un muro, el punto de planta es el que queda bajo el impacto, no el suelo de detrás.
- **Unreal traduce:** `UFloor101GameViewportClient` convierte el cursor en rayo de escena, las teclas en comandos
  (`MapKey`) y los caracteres en texto de la herramienta; `AFloor101EditingPawn` es una cámara orbital (derecho orbita,
  central desplaza, rueda acerca, F encuadra, T vista en planta; sigue a la planta activa). RePág/AvPág cambian de planta
  y L muestra todas. `AFloor101GameMode` y el viewport client se configuran
  en `Config/DefaultEngine.ini` y rigen también en PIE.
- **UI:** Slate en C++ (`SFloor101Overlay`, textos en español): barra con modos, deshacer/rehacer, encuadrar, planta,
  ruta de archivo y Guardar/Abrir, y la fila de plantas (planta activa con su cota, subir, bajar, nueva planta, ver
  todas); panel de propiedades del muro seleccionado (longitud, espesor, altura, eliminar), del hueco seleccionado
  (ancho, alto, antepecho, posición, eliminar) o, sin selección, de la planta activa (nombre, altura, forjado, eliminar);
  barra de estado con ayuda contextual y errores; etiqueta junto al cursor con la longitud y lo escrito; área de cada
  habitación sobre su suelo. La UI solo llama al subsistema; nunca toca actores.
- **Vista:** selección (naranja), vista previa (azul) y asas (amarillo) son un `ViewOverlay` que se compila con el
  proyecto y nunca se guarda.
- **Archivo:** `project.json` (ADR-0007); ruta por defecto `Documentos/101st Floor/project.json`; Ctrl+S / Ctrl+O o
  `f101.Project.Save|Open [ruta]`.

## Tests

| Nivel | Qué | Herramienta | Se ejecuta con |
|---|---|---|---|
| Unitarios (sin Unreal) | CadCore, Geometry, SceneModel (`Tests/Unit/`) | CMake + Ninja (VS Build Tools) + MSVC 14.50 + **doctest** 2.5.3 (`ThirdParty/doctest`) | `Test.ps1` (segundos) |
| Arquitectura | módulos puros sin cabeceras ni tipos de Unreal; `Build.cs` solo con `Core` y módulos puros | script + compilación CMake | `Test.ps1` |
| Integración Unreal | adaptador (`Floor101.Adapter.*`): actor, componente, bounds 355×12×240 cm, triángulos según `GetTriNormal`, regeneración a 420 cm sin escalar, estilos de vista, muro con puerta (7 secciones); interacción (`Floor101.Interaction.*`): mapa de teclas, cámara, flujo completo por el subsistema con `project.json`, habitación con suelo, área y arrastre de esquina, plantas (forjado, planta de abajo atenuada, muro dibujado en la cota de su planta, ocultar y ver todas); toolset (`Floor101.Editor.*`) | Unreal Automation (`UnrealEditor-Cmd -nullrhi`) o MCP `AutomationTestToolset` | `Test.ps1 -Integration` |
| Render | DX12/SM6/HWRT/Lumen | `CheckRendering.ps1` | manual o en la validación de cada fase |
| MCP | handshake, toolsets, lectura | `CheckMcp.ps1` | manual o en la validación de cada fase |

Los módulos puros se compilan en CMake con `/W4 /WX /permissive- /EHs-c-`, igual o más estricto que Unreal: un `throw`
no compila. Se descarta Low Level Tests de UE: exige compilar targets de programa contra el motor, es lento y acopla los
tests a la instalación de Unreal.

## Scripts

`Setup.ps1` (comprobaciones de la Fase A automatizadas) · `Build.ps1` (UBT) · `Test.ps1` (unitarios → arquitectura →
integración) · `Run.ps1` (app en `-game`) · `CreateDesktopShortcut.ps1` (acceso directo a `Run.ps1` en el escritorio) ·
`RunEditorMcp.ps1` · `CheckRendering.ps1` · `CheckMcp.ps1` ·
`Clean.ps1` · `Package.ps1` (cuando exista algo que empaquetar). Todos localizan motor, CMake y MSVC con
`vswhere`/registro, nunca por `PATH` (hay un `cmake` ajeno en el `PATH` de esta máquina).

## Render / RTX

Modo Diseño: configuración validada en la Fase B (DX12, SM6, Lumen HWRT, VSM, TSR). Riesgo a verificar en MVP 0: que las
paredes generadas contribuyan a la GI de Lumen (cards por bounding box). Si no basta, alternativa documentada:
`r.Lumen.HardwareRayTracing.LightingMode=2`, que evalúa la iluminación en el impacto del rayo, en las reflexiones.
Modo Foto (Path Tracer / Lumen máxima calidad) queda fuera del MVP 0. Nanite se reserva para assets densos (Blender); las
paredes son geometría simple.

## Estado de implementación (MVP 4, 2026-09-29)

MVP 0: los cinco módulos de producto más `Floor101Editor` y la pared de referencia 3550 → 4200 mm por todo el pipeline
(`docs/mvp0-report.md`). MVP 1: herramienta de muro interactiva con longitud escrita, selección, borrar, longitud,
espesor y altura, arrastre de extremos, deshacer/rehacer, `project.json`, cámara orbital y UI Slate
(`docs/mvp1-report.md`). MVP 2: uniones de muros (L, T, X, espesores distintos), habitaciones con suelo y área neta,
arrastre de esquinas compartidas, snapping a muros y vista en planta, verificados con tests, MCP y ratón y teclado
reales (`docs/mvp2-report.md`). MVP 3: puertas y ventanas como huecos del muro (herramientas, selección, propiedades,
deshacer, `project.json` esquema 2, MCP) (`docs/mvp3-report.md`). MVP 4: plantas apiladas con forjados derivados, edición
por planta, `project.json` esquema 3 y herramientas MCP de plantas (`docs/mvp4-report.md`). Detalle por capa:
`docs/cad-core.md`, `docs/geometry.md`, `docs/scene-model.md`, `docs/unreal-integration.md`.

## Fuera del MVP 4 (la arquitectura no lo bloquea)

Escaleras y huecos en forjados, sótanos, voladizos y terrazas, cubiertas inclinadas, forjados editables, carpintería
de puertas y ventanas (marcos, hojas, vidrio), huecos no rectangulares, validación de huecos contra las uniones,
nombres y propiedades persistentes de habitaciones (entidad `Room`), materiales
arquitectónicos, exportadores, Asset System, vista 2D ortográfica, diálogos de archivo nativos, traducción de los
mensajes del núcleo, reasignación de teclas, recompilación incremental para proyectos grandes.
