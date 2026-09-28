# ADR-0003: Scene Model neutral, derivado y con identidad determinista

Estado: aceptado (Fase D, 2026-09-28)

## Contexto
Unreal y Twinmotion (vía exportadores) deben consumir la misma escena, sin que Unreal sea el puente. El modelo CAD
conserva la semántica editable (paredes, habitaciones, restricciones). Hace falta una representación intermedia para
render y exportación que no se convierta accidentalmente en fuente de verdad y que permita actualizaciones incrementales
(Scene Diff, sincronización futura con Twinmotion).

## Alternativas
| Opción | Ventajas | Riesgos |
|---|---|---|
| Actores de Unreal como escena | nada que construir | acopla todo a Unreal; exportar implicaría pasar por Unreal (prohibido por el prompt maestro) |
| Exportar directamente desde el modelo CAD en cada exportador | menos capas | cada consumidor reimplementa geometría, materiales e identidad; incoherencias entre destinos |
| **Scene Model propio, compilado desde el modelo** | un contrato único para todos los consumidores; testeable sin Unreal | una capa más que mantener |

## Decisión
- `Floor101SceneModel` define `Scene`, `SceneNode`, `SceneMesh` (y, más adelante, materiales portables, luces y
  cámaras). Se genera con `SceneCompiler::compile(project)` y es **de solo lectura para los consumidores**. Nunca se
  serializa como proyecto ni se edita.
- **Identidad determinista:** `SceneNodeId` se deriva de la entidad de origen y su rol (`<wallId>/body`), nunca es
  aleatorio. Recompilar produce los mismos IDs, lo que permite reconciliar en Unreal, calcular un `SceneDiff` y
  reimportar en Twinmotion.
- **Sistema de coordenadas de la escena:** el del modelo. Dextrógiro, X este, Y norte, Z arriba, en milímetros (`double`
  para la geometría). Cada consumidor aplica una única conversión documentada:
  - Unreal (levógiro, cm): `(x, y, z) → (x/10, −y/10, z/10)`, con inversión del orden de los triángulos.
  - glTF (dextrógiro, Y arriba, m), en el futuro: `(x, y, z) → (x, z, −y) / 1000`.
- Cada nodo guarda los metadatos de origen (tipo e ID de la entidad) para inspección, selección y exportación.

## Consecuencias
- Cualquier consumidor nuevo (vista 2D, USD, Datasmith) se escribe contra la escena, no contra el modelo ni Unreal.
- MVP 0 recompila la escena completa; el diseño admite recompilado incremental más adelante sin cambiar el contrato.
- El inverso del espejo de ejes es un punto clásico de error: se cubre con tests (normales hacia fuera, orden de los
  triángulos, bounds con signo).

## Verificación
Tests unitarios: compilar un proyecto con una pared da un nodo con el `SceneNodeId` esperado; recompilar tras un cambio
mantiene el ID; bounds en mm. Test de integración en Unreal: bounds 355×12×240 cm y el mismo actor tras cambiar a 4200 mm.
