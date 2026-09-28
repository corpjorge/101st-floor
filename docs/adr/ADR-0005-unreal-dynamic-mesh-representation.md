# ADR-0005: Representación en Unreal con `UDynamicMeshComponent`

Estado: aceptado (Fase D, 2026-09-28); se valida en MVP 0.

## Contexto
Las paredes se generan en ejecución a partir del modelo y se regeneran al editarlas; no son assets creados en el editor.
Deben verse bien con Lumen por hardware: sombras, reflexiones y, sobre todo, **rebote de la GI**, porque una pared que no
rebota luz arruina un interior. La app final corre empaquetada, sin editor.

## Alternativas (comprobadas en el código de UE 5.8.3)
| Opción | Ray tracing | Cards de Lumen (GI en la surface cache) | Runtime empaquetado | Notas |
|---|---|---|---|---|
| `UProceduralMeshComponent` | sí (`r.RayTracing.Geometry.ProceduralMeshes`) | **no** | sí | API sencilla, pero sin cards las paredes aportan una GI pobre en Lumen |
| **`UDynamicMeshComponent`** (GeometryFramework) | sí (`bEnableRaytracing=true`) | **sí**: `UpdateLumenCardsFromBounds()`, cvar `geometry.DynamicMesh.AllowMeshCardGeneration=1`, no depende del editor | sí | malla moderna de UE5, colisión, grupos de material; distance fields obsoletos (irrelevante con HWRT) |
| `UStaticMesh` creado en runtime (`BuildFromMeshDescriptions`) | sí | **no** en la app empaquetada: sin editor solo se permite *fast build* (`check(Params.bFastBuild)`), y esa ruta no genera cards ni distance fields | parcial | pensado para assets, no para edición continua |
| Nanite | — | — | — | innecesario para paredes de pocos polígonos; se reserva para assets densos |

## Decisión
- `Floor101UnrealAdapter` representa cada nodo de escena con un `AFloor101SceneNodeActor` transitorio que contiene un
  `UDynamicMeshComponent`. Ray tracing activado y un grupo de material por sección de `MeshData`.
- La geometría se reconstruye al cambiar el modelo; nunca se escala el componente.
- Esta elección queda **encapsulada en el adaptador**: la escena y el núcleo no la conocen, así que puede cambiarse sin
  tocarlos.

## Riesgo y plan
Las cards a partir del bounding box son exactas para una pared recta (una caja), pero aproximadas para geometría con
huecos o forma en L. Se verifica en MVP 0 con la visualización de Lumen y comparando la GI. Si no basta:
`r.Lumen.HardwareRayTracing.LightingMode=2`, que evalúa la iluminación en el impacto en reflexiones (o `1` también en GI,
más caro), o dividir las paredes complejas en varias piezas con cards propias.

## Verificación
MVP 0: un test de integración comprueba el actor y el componente, los bounds de 355×12×240 cm y la regeneración a 420 cm
con escala 1. `CheckRendering.ps1` sobre un mapa con la pared confirma que no hay regresión en los pases HWRT. También se
inspecciona visualmente la GI (`r.Lumen.Visualize`).
