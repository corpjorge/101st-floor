# Informe MVP 1 — 2026-09-28

**Estado:** MVP 1 implementado y verificado. Entorno y versiones iguales a los del MVP 0 (`docs/mvp0-report.md`),
más nlohmann/json 3.12.0. No se ha empezado otro MVP.

## Alcance (prompt maestro §19, §20, §46 y decisiones del usuario)

| Requisito | Implementación | Verificación |
|---|---|---|
| Clic → preview → mover → longitud en pantalla → escribir → Enter → muro exacto | `WallTool` + `EditController`; etiqueta junto al cursor | doctest §19 con `355`, `355 cm`, `3.55 m`, `3,55 m`, `3550 mm` → (0,0)–(3550,0); `Floor101.Interaction.EditFlow`; prueba real |
| Selección | `pickWall` (rayo contra la caja del muro); tinte naranja; asas amarillas | doctest, `Floor101.Adapter.ViewStyles`, prueba real |
| Borrar | `DeleteWallCommand` (Supr o botón) | doctest, EditFlow, prueba real |
| Cambiar longitud / espesor / altura | panel de propiedades con entrada escrita (cm por defecto) | doctest, EditFlow, prueba real (4.2 m → 420 cm) |
| Mover extremo | arrastre de asa con snapping; un solo comando al soltar; original oculto y vista previa azul | doctest, prueba real (355 → 595 cm y deshacer) |
| Deshacer / rehacer | transacciones por entidad (ADR-0006); Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z, botones | doctest, EditFlow, toolset, prueba real |
| `project.json` | nlohmann/json, validación estricta, guardado atómico (ADR-0007); Ctrl+S / Ctrl+O y ruta en la barra | doctest (golden, round trip, rechazos, ruta no ASCII), EditFlow, toolset, prueba real |
| Unidad de entrada por defecto | cm (decisión del usuario) | doctest |
| Snapping en el núcleo (§20) | extremos (150 mm), ángulo (15°), rejilla (50 mm) | doctest |

Arquitectura: ADR-0006 (comandos y deshacer), ADR-0007 (`project.json`), ADR-0008 (interacción). La lógica está en el
CAD Core puro; Unreal traduce entrada (`UFloor101GameViewportClient`), muestra (overlay de escena, Slate) y orbita la
cámara (`AFloor101EditingPawn`).

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **90 casos, 2873 aserciones, todo verde** + arquitectura OK |
| Integración Unreal (`Test.ps1 -Integration`) | **18/18**: adaptador (3), interacción (3), toolset (12) |
| `CheckRendering.ps1 -Demo` (nuevo game mode y overlay) | todo PASS: DX12/SM6, RTX 5070 Ti, HWRT, Lumen GI + reflexiones (9 pases HWRT), VSM, Nanite; cards 2/12; GPU 2,98 ms media @ 1920×1080 |
| `CheckMcp.ps1` | 6/6 |
| MCP en PIE | `StartPIE` → `GetWalls` vacío (documento de PIE) → `AddWall` con actor en `UEDPIE_0` → `Undo`/`Redo` → `StopPIE` → el documento del editor intacto; el overlay aparece en PIE |

### Prueba real con ratón y teclado (`-game`, computer use)
Seleccionar (naranja, asas, panel 355/12/240 cm) → arrastrar el extremo (vista previa azul, "595 cm") → Ctrl+Z → panel:
longitud `4.2 m` → W + clic + mover + `300` + Enter → Supr → guardar en `Saved/ManualTest/project.json` → abrir (muros
restaurados, historial limpio) → F → rueda.

Fallos encontrados en la prueba real y corregidos (con test de regresión):
1. **Longitud escrita en diagonal:** `300` a 75° creaba 2999 mm. La dirección salía del punto ya redondeado del cursor y
   cada coordenada se redondeaba aparte. Ahora se usa la dirección exacta del ajuste (`SnapResult::constrained`) y
   `pointAtDistance` elige el milímetro de longitud más cercana (error < 0,5 mm): 3000 mm verificado en pantalla y en
   el log. `ChangeWallLengthCommand` usa la misma función.
2. **Rueda:** varias muescas en un evento hacían un solo paso de zoom. Ahora se usa `MouseWheelAxis`.
3. **Encuadre (F):** quedaba demasiado cerca. Ahora ajusta al campo de visión vertical con margen.

## Limitaciones conocidas
- **Esc:** en PIE el editor lo reserva para detener la sesión; se cancela con clic derecho sin arrastrar. La
  herramienta de automatización tampoco entrega la pulsación de Esc (ni a las cajas de texto de Slate), así que Esc no
  se probó con teclado real. `Floor101.Interaction.KeyMap` cubre la tecla y el clic derecho probó el mismo `cancel()`.
- Orbitar y desplazar (derecho y central arrastrando) no se probaron con el ratón real: la herramienta solo arrastra
  con el botón izquierdo. `Floor101.Interaction.CameraPawn` cubre la cámara.
- Mensajes del núcleo (errores, etiquetas de deshacer) en inglés dentro de una UI en español.
- Sin diálogo de archivo nativo: la ruta se escribe en la barra. Por defecto `Documentos/101st Floor/project.json`.
- Cada movimiento del cursor recompila la escena completa (el reconciliador solo toca lo que cambia); suficiente para
  el MVP.
- En la prueba automatizada, los clics instantáneos (pulsar y soltar en el mismo instante) no daban foco a las cajas
  de propiedades; con clics de duración normal sí. Conviene confirmarlo a mano.

## Archivos principales
`Source/Floor101CadCore/…/{Document,ProjectFile,Snapping,Picking,WallTool,EditController}.h/.cpp`,
`Source/Floor101SceneModel/…/SceneCompiler.h` (overlay), `Source/Floor101UnrealAdapter/…/{Floor101SceneSpace.h,
Floor101SceneNodeActor,Floor101SceneReconciler}`, `Source/Floor101/…/{Floor101DocumentSubsystem,
Floor101GameViewportClient,Floor101EditingPawn,Floor101GameMode,Floor101PlayerController,SFloor101Overlay}`,
`Source/Floor101Editor/…/Floor101DocumentToolset`, `Config/DefaultEngine.ini`, `ThirdParty/nlohmann/`,
`Tests/Unit/*`, `docs/adr/ADR-0006..0008`.
