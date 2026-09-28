# Integración con Unreal

## Unreal Adapter (`Source/Floor101UnrealAdapter`)
Única frontera entre la escena y Unreal (ADR-0005). Conoce la escena, nunca el modelo CAD.

- `Floor101UnrealConversion.h`: mm → cm (`/10`) y espejo de Y (dextrógiro → levógiro). **Conserva el orden de los
  triángulos**: el espejo y la convención de Unreal (`VectorUtil::Normal` = `edge2 × edge1`) se compensan.
  `ToDynamicMesh` crea `FDynamicMesh3` con normales, UV, polygroups e ID de material por sección.
- `AFloor101SceneNodeActor`: `UCLASS(Transient, NotPlaceable)`, raíz `UDynamicMeshComponent` con ray tracing activado,
  material por defecto `/Engine/BasicShapes/BasicShapeMaterial` (referencia dura, se cocina con el juego), etiqueta
  `f101.node:<SceneNodeId>`.
- `FFloor101SceneReconciler`: por `SceneNodeId`, crea, reconstruye (si cambió el `contentHash`) o destruye actores.
  Nunca escala: la escala sigue en 1.

## Aplicación (`Source/Floor101`)
`UFloor101DocumentSubsystem` (un `UWorldSubsystem` por mundo de juego, PIE o editor) posee el `Document`. Cada cambio →
`compileScene` → `Reconcile`, con log `F101: scene resync in X ms` y el estado de cada pared.

Desarrollo:
- `-F101Demo` carga la pared de referencia al empezar el juego (`Run.ps1`, `CheckRendering.ps1 -Demo`).
- Consola: `f101.Demo.Load`, `f101.Demo.SetLength <longitud>` (acepta `420`, `4.2 m`, `4,2 m`, `4200 mm`),
  `f101.Demo.Clear`, `f101.Demo.Status`.

## Herramientas de editor (`Source/Floor101Editor`, solo editor)
Toolset MCP `Floor101Editor.Floor101DocumentToolset`: `GetWalls`, `AddWall`, `SetWallEnd`, `RemoveWall` sobre el
documento del mundo del editor, en mm enteros. Devuelve el actor para inspeccionarlo con `ActorTools`. Se registra
desde un `UEditorSubsystem` que depende de `UToolsetRegistrySubsystem`, porque `RegisterToolsetClass` no hace nada si
el registro aún no está listo. Ver `docs/unreal-mcp.md`.

## Tests de integración
`Floor101.Adapter.Conversion`, `Floor101.Adapter.ReferenceWall` y `Floor101.Editor.DocumentToolset.*` (ciclo de vida
y cada error). `Test.ps1 -Integration` los ejecuta en un editor headless; también se pueden lanzar por MCP.

## Métricas observadas (MVP 0)
Resync de la escena con una pared: 0,7–0,9 ms al crearla y 0,2 ms al regenerarla (log `LogFloor101`, editor, Development).
