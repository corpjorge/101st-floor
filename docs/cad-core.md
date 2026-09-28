# CAD Core (`Source/Floor101CadCore`, `f101::cad`)

Modelo arquitectónico editable y fuente de verdad. C++20 puro (ADR-0002): sin Unreal, sin excepciones, sin RTTI.

| Pieza | Fichero | Qué es |
|---|---|---|
| Unidades | `F101/Cad/Units.h` | `parseLength` (entrada del usuario → mm exactos, cm por defecto), `quantizeMillimeters` (único punto de reentrada de valores calculados), `MaxAbsMillimeters` = 10^9 |
| Primitivas 2D | `F101/Cad/Geometry2D.h` | `Point2D`, `Vector2D`, `Segment2D` en `int64_t` mm (planta: X este, Y norte) |
| Identidad | `F101/Cad/Id.h` | `Uuid` (v4 aleatorio o secuencial en tests), `TypedId<Tag>` (`WallId`) |
| Pared | `F101/Cad/Wall.h` | `Wall{id, start, end, thicknessMm, heightMm}`, `validateWall` |
| Proyecto | `F101/Cad/Project.h` | `Project{walls}` |
| Documento | `F101/Cad/Document.h` | mutaciones validadas (`addWall`, `setWallEnd`, `removeWall`) y notificación `ChangeSet` a los suscriptores |
| Errores | `F101/Cad/Result.h`, `Assert.h` | `Result<T>`/`Status` para errores esperables; `F101_ASSERT` para invariantes |

Reglas:
- Una mutación rechazada deja el proyecto intacto y no notifica.
- Los suscriptores se ejecutan de forma síncrona tras cada mutación aceptada (así se regenera la escena).
- `Export.h` define `DLLEXPORT`/`DLLIMPORT` igual que Unreal para que las macros `FLOOR101*_API` funcionen en el build
  modular del editor sin incluir cabeceras de Unreal.

Siguiente (MVP 1): comandos con undo/redo sobre esta API, snapping, serialización `project.json`.
Tests: `Tests/Unit/UnitsTests.cpp`, `IdTests.cpp`, `Geometry2DTests.cpp`, `WallTests.cpp`.
