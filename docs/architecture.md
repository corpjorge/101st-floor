# Arquitectura

Estado: **aprobada (Fase D, 2026-09-28).** Decisiones importantes en `docs/adr/`.

## Principio

101st Floor es la fuente de verdad. El modelo CAD (paramétrico, en milímetros enteros) genera geometría, la geometría
compila una escena neutral, y cada consumidor (Unreal, exportadores, vista 2D) representa esa escena. Nada fluye en
sentido contrario: Unreal no escribe en el modelo, y un nivel de Unreal nunca se guarda como dato del proyecto.

```
          UI (Slate, MVP 1) ── comandos ──►  CAD Core  ◄── Asset System (futuro)
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

Cinco módulos de Unreal. Los tres primeros son **C++20 estándar**: no incluyen cabeceras de Unreal, no usan tipos de
Unreal y compilan igual con UnrealBuildTool y con CMake (tests sin Unreal). Ver ADR-0002.

| Módulo | Tipo | Namespace | Depende de | Contenido |
|---|---|---|---|---|
| `Floor101CadCore` | C++ puro | `f101::cad` | — | unidades, `Point2D`/`Vector2D`/`Segment2D`/`Bounds`, IDs, `Project`/`Level`/`Wall`, validación, `Document` (modelo + notificación de cambios); más adelante comandos, undo/redo, snapping, serialización |
| `Floor101Geometry` | C++ puro | `f101::geometry` | CadCore | `MeshData` propio, `WallGeometryBuilder`; más adelante uniones, huecos, suelos, techos |
| `Floor101SceneModel` | C++ puro | `f101::scene` | CadCore, Geometry | `Scene`, `SceneNode`, `SceneMesh`, `SceneCompiler` (Project → Scene); más adelante materiales portables, luces, cámaras, `SceneDiff` |
| `Floor101UnrealAdapter` | Unreal | — | SceneModel, Engine, GeometryFramework | reconciliación `Scene` → actores con `UDynamicMeshComponent`; conversión de unidades y ejes; tests de integración |
| `Floor101` (existente) | Unreal, módulo principal | — | todos | aplicación: posee el `Document`, comandos de consola de desarrollo, `RenderCheck`; UI Slate en MVP 1 |

La dirección de dependencias se hace cumplir en compilación: un módulo solo ve las cabeceras de sus dependencias
declaradas (UBT y CMake). Los módulos puros declaran únicamente `Core` en su `Build.cs`, y solo para el fichero de
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
  aplica `(x, y, z) mm → (x/10, −y/10, z/10) cm` e invierte el orden de los triángulos. Cada consumidor tiene una única
  conversión documentada (glTF, en el futuro: `(x, z, −y) / 1000` m).

## Identidad

- Entidades (`WallId`, `LevelId`, …): UUID v4 con tipo propio por entidad. Generador inyectable para que los tests sean
  deterministas. Nunca se usan índices como identidad.
- Nodos de escena: **derivados de forma determinista** de la entidad de origen y su rol (`<wallId>/body`). Recompilar
  la escena produce los mismos IDs, y eso permite reconciliar actores de Unreal, calcular un `SceneDiff` y, en el futuro,
  reimportar en Twinmotion sin perder el trabajo artístico.

## Flujo de un cambio (MVP 0)

```
Document::setWallEnd(wallId, {4200, 0})       // API de mutación validada; comandos + undo en MVP 1
  → notifica WallChanged(wallId)
  → SceneCompiler::compile(project)            // WallGeometryBuilder regenera MeshData
  → UnrealAdapter::reconcile(world, scene)     // actualiza el mismo actor (mismo SceneNodeId): la malla se
                                               // reconstruye; la escala del componente sigue en 1
```

En MVP 0 se recompila la escena completa (una pared). La reconciliación por ID ya es la base del recompilado incremental.

## Geometry Engine

`WallGeometryBuilder(wall) → MeshData`. Pared recta con línea de referencia en el **eje** y espesor centrado.
`MeshData` guarda posiciones y normales en mm (`double`), UV en metros reales (1,0 = 1000 mm, para que los materiales
arquitectónicos tengan escala física) e índices de triángulos agrupados en **secciones por cara**: izquierda, derecha,
superior, inferior, inicio y fin. Así cada cara puede llevar su propio material. Caras con sombreado plano: 24 vértices
y 12 triángulos por pared.

## Unreal Adapter (ADR-0005)

- Un actor por nodo de escena (`AFloor101SceneNodeActor`), **transitorio** (`RF_Transient`): nunca se guarda en un
  nivel. Lleva la etiqueta `f101.node:<SceneNodeId>` para localizarlo por MCP (`find_actors` por tag) y en tests.
- La malla va en un **`UDynamicMeshComponent`**, que en UE 5.8 aporta geometría de ray tracing y genera en ejecución las
  *mesh cards* de Lumen a partir del bounding box (`UpdateLumenCardsFromBounds`). `ProceduralMeshComponent` solo aporta
  ray tracing sin cards, así que las paredes no rebotarían bien la luz en Lumen.
- La reconciliación crea, actualiza o elimina actores comparando los `SceneNodeId`. La geometría nunca se escala: se
  regenera.

## UI (MVP 1)

Slate en C++, dentro del módulo `Floor101`: `MainWindow` con barra superior, panel de construcción, catálogo, viewport,
panel de propiedades y barra de estado. La UI solo emite comandos al `Document` y nunca toca actores. La herramienta de
pared convierte el rayo del cursor al plano CAD, cuantiza a mm y delega el snapping al CAD Core. El preview de la pared
recorre el mismo pipeline como nodo de escena transitorio.

## Tests

| Nivel | Qué | Herramienta | Se ejecuta con |
|---|---|---|---|
| Unitarios (sin Unreal) | CadCore, Geometry, SceneModel | CMake + Ninja (VS Build Tools) + MSVC 14.50 + **doctest** incluido en el repo | `Test.ps1` (segundos) |
| Arquitectura | módulos puros sin cabeceras ni tipos de Unreal | script + compilación CMake | `Test.ps1` |
| Integración Unreal | adaptador: actor, componente, bounds 355×12×240 cm, regeneración a 420 cm sin escalar | Unreal Automation (`UnrealEditor-Cmd -nullrhi`) o MCP `AutomationTestToolset` | `Test.ps1 -Integration` |
| Render | DX12/SM6/HWRT/Lumen | `CheckRendering.ps1` | manual o en la validación de cada fase |
| MCP | handshake, toolsets, lectura | `CheckMcp.ps1` | manual o en la validación de cada fase |

Los módulos puros se compilan en CMake con `/W4 /WX /permissive- /EHs-c-`, igual o más estricto que Unreal: un `throw`
no compila. Se descarta Low Level Tests de UE: exige compilar targets de programa contra el motor, es lento y acopla los
tests a la instalación de Unreal.

## Scripts

`Setup.ps1` (comprobaciones de la Fase A automatizadas) · `Build.ps1` (UBT) · `Test.ps1` (unitarios → arquitectura →
integración) · `Run.ps1` (app en `-game`) · `RunEditorMcp.ps1` · `CheckRendering.ps1` · `CheckMcp.ps1` ·
`Clean.ps1` · `Package.ps1` (cuando exista algo que empaquetar). Todos localizan motor, CMake y MSVC con
`vswhere`/registro, nunca por `PATH` (hay un `cmake` ajeno en el `PATH` de esta máquina).

## Render / RTX

Modo Diseño: configuración validada en la Fase B (DX12, SM6, Lumen HWRT, VSM, TSR). Riesgo a verificar en MVP 0: que las
paredes generadas contribuyan a la GI de Lumen (cards por bounding box). Si no basta, alternativa documentada:
`r.Lumen.HardwareRayTracing.LightingMode=2`, que evalúa la iluminación en el impacto del rayo, en las reflexiones.
Modo Foto (Path Tracer / Lumen máxima calidad) queda fuera del MVP 0. Nanite se reserva para assets densos (Blender); las
paredes son geometría simple.

## Fuera del MVP 0 (la arquitectura no lo bloquea)

Comandos con undo/redo, serialización `project.json`, uniones y esquinas, huecos (puertas y ventanas), habitaciones,
materiales arquitectónicos, UI, exportadores, Asset System, snapping. La serialización básica (`project.json`) se hará
en **MVP 1**, junto con undo/redo (decisión del usuario, 2026-09-28). El esquema se diseñará antes.
