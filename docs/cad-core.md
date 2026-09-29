# CAD Core (`Source/Floor101CadCore`, `f101::cad`)

Modelo arquitectónico editable y fuente de verdad. C++20 puro (ADR-0002): sin Unreal, sin excepciones, sin RTTI.

| Pieza | Fichero | Qué es |
|---|---|---|
| Unidades | `F101/Cad/Units.h` | `parseLength` (entrada del usuario → mm exactos, cm por defecto), `quantizeMillimeters` (único punto de reentrada de valores calculados), `formatLength` (mm → "355 cm", "3.55 m"), `MaxAbsMillimeters` = 10^9 |
| Primitivas 2D | `F101/Cad/Geometry2D.h` | `Point2D`, `Vector2D`, `Segment2D` en `int64_t` mm (planta: X este, Y norte); `PlanPoint` en `double` para el cursor y la geometría derivada |
| Identidad | `F101/Cad/Id.h` | `Uuid` (v4 aleatorio o secuencial en tests), `TypedId<Tag>` (`WallId`) |
| Pared | `F101/Cad/Wall.h` | `Wall{id, start, end, thicknessMm, heightMm}`, `validateWallGeometry` (sin id, p. ej. vistas previas), `validateWall` |
| Proyecto | `F101/Cad/Project.h` | `Project{walls}` |
| Documento | `F101/Cad/Document.h` | comandos como datos (`CreateWall`, `DeleteWall`, `MoveWallEndpoint`, `MoveJoint` (esquina compartida), `ChangeWallLength/Thickness/Height`), `execute`, `undo`/`redo` con etiquetas, `replaceProject`, notificación `ChangeSet` (ADR-0006) |
| Archivo | `F101/Cad/ProjectFile.h` | `project.json`: escribir/leer texto, guardar (atómico) y abrir archivos (ADR-0007) |
| Snapping | `F101/Cad/Snapping.h` | `snapPoint` (extremos, eje de un muro, rejilla), `snapFromAnchor` (extremos, muro — donde lo cruza el ángulo o su punto más cercano —, ángulo, longitud en rejilla; `constrained` = dirección exacta), listas de muros a ignorar, `pointAtDistance` (longitud escrita, error < 0,5 mm) |
| Picking | `F101/Cad/Picking.h` | `Ray3` en mm; `pickWall` (caja del muro, con muros a ignorar), `intersectPost` (asas), `intersectFloor` (z = 0) |
| Herramienta de muro | `F101/Cad/WallTool.h` | clic → vista previa → longitud escrita → Enter/clic; encadena muros (§19) |
| Edición | `F101/Cad/EditController.h` | modos Seleccionar/Muro, selección, arrastre de esquinas (todos los muros que la comparten, con desfase de agarre), propiedades escritas, cancelar, deshacer/rehacer; el cursor en planta es el punto bajo el muro señalado o el suelo; `view()` con los muros previstos (ADR-0008, ADR-0009) |
| Errores | `F101/Cad/Result.h`, `Assert.h` | `Result<T>`/`Status` para errores esperables; `F101_ASSERT` para invariantes |

Reglas:
- Todo cambio del modelo es un comando validado: uno rechazado deja el proyecto y el historial intactos; uno que no
  cambia nada no se registra.
- Los suscriptores se ejecutan de forma síncrona tras cada transacción aplicada (así se regenera la escena).
- La interacción recibe rayos en espacio de escena; nunca tipos de un motor. El snapping es del núcleo (§20).
- Las uniones y las habitaciones no son parte del modelo: se derivan de los muros en el Geometry Engine (ADR-0009).
- `Export.h` define `DLLEXPORT`/`DLLIMPORT` igual que Unreal para que las macros `FLOOR101*_API` funcionen en el build
  modular del editor sin incluir cabeceras de Unreal.
- nlohmann/json 3.12.0 (`ThirdParty/nlohmann`) se usa solo dentro de `ProjectFile.cpp`; no aparece en cabeceras
  públicas.

Tests: `Tests/Unit/UnitsTests.cpp`, `IdTests.cpp`, `Geometry2DTests.cpp`, `WallTests.cpp`, `CommandTests.cpp`,
`ProjectFileTests.cpp`, `SnappingTests.cpp`, `PickingTests.cpp`, `WallToolTests.cpp`, `EditControllerTests.cpp`.
