# CAD Core (`Source/Floor101CadCore`, `f101::cad`)

Modelo arquitectónico editable y fuente de verdad. C++20 puro (ADR-0002): sin Unreal, sin excepciones, sin RTTI.

| Pieza | Fichero | Qué es |
|---|---|---|
| Unidades | `F101/Cad/Units.h` | `parseLength` (entrada del usuario → mm exactos, cm por defecto), `quantizeMillimeters` (único punto de reentrada de valores calculados), `formatLength` (mm → "355 cm", "3.55 m"), `MaxAbsMillimeters` = 10^9 |
| Primitivas 2D | `F101/Cad/Geometry2D.h` | `Point2D`, `Vector2D`, `Segment2D` en `int64_t` mm (planta: X este, Y norte); `PlanPoint` en `double` para el cursor y la geometría derivada |
| Identidad | `F101/Cad/Id.h` | `Uuid` (v4 aleatorio o secuencial en tests), `TypedId<Tag>` (`WallId`, `OpeningId`, `LevelId`, `FurnitureId`) |
| Planta | `F101/Cad/Level.h` | `Level{id, name, heightMm, slabThicknessMm}` (ADR-0011): altura suelo a suelo (2800 por defecto), forjado (250), `wallHeightMm`, `clearHeightMm`, `validateLevel` |
| Acabados | `F101/Cad/Finishes.h` | catálogo de 21 acabados (ADR-0012, ADR-0013, ADR-0016): id, superficies (muro, suelo, techo, carpintería, vidrio, muebles), color sRGB, junta, rugosidad, opacidad y patrón con tamaño de pieza; `findFinish`, `resolveFinish` (vacío = el de la superficie), `validateFinish`; `DefaultCarpentryFinish`, `DefaultGlazingFinish`, `HardwareFinish` |
| Mobiliario | `F101/Cad/Furniture.h` | catálogo fijo de 12 tipos (`FurnitureType`: medidas por defecto y rangos, si va contra un muro, acabado del cuerpo), `Furniture{id, type, level, position, rotationCentidegrees, widthMm, depthMm, heightMm, finish}` (huella centrada en `position`; en su marco x a lo ancho e y hacia la espalda, así que con giro 0 mira al sur), `makeFurniture`, `validateFurniture`, `resolveFurnitureFinish`, `toCentidegrees`, `furnitureAxes`/`furnitureToPlan`/`planToFurniture`/`furnitureFootprint`/`furnitureContains` (cuartos de vuelta exactos) (ADR-0016) |
| Pared | `F101/Cad/Wall.h` | `Wall{id, start, end, thicknessMm, heightMm, openings, level, finishLeft, finishRight}` (geometría local a su planta; acabado de cada cara, `WallSide`); `Opening{id, kind (Door/Window), offsetMm, widthMm, heightMm, sillMm, finish, operation, hinge, opensTo, openPercent}` (ADR-0010; `finish`, la carpintería, ADR-0013; cómo abre: `OpeningOperation` Hinged/DoubleHinged/Sliding/Fixed, `OpeningEnd` de las bisagras, cara hacia la que abre y apertura 0–100 %, ADR-0014); `AxisSpan` y `occupiedSpan` (hueco más el bolsillo de una puerta corredera); `validateWallGeometry` (sin id, p. ej. vistas previas; incluye los huecos: caben, dejan dintel, puertas en el suelo, sin solapes ni bolsillos fuera del muro o sobre otro hueco, una puerta no es fija, apertura 0–100, una fija no se abre), `validateWall` |
| Proyecto | `F101/Cad/Project.h` | `Project{levels, walls, roomFinishes, furniture}` (plantas de abajo arriba; acabados de suelo y techo por clave de habitación; muebles en orden de creación, `findFurniture`), `levelOf`, `levelElevationMm` (cota derivada), `wallsOnLevel`, `snapReference` (su planta y la de abajo); `WallObstructions` (tramos de un muro ocupados por los que se le unen; lo implementa el Geometry Engine, ADR-0013) |
| Documento | `F101/Cad/Document.h` | comandos como datos (`CreateWall`, `DeleteWall`, `MoveWallEndpoint`, `MoveJoint` (esquina compartida), `ChangeWallLength/Thickness/Height`, `AddOpening`, `ChangeOpening`, `DeleteOpening`, `AddLevel`, `ChangeLevel` (sus muros a altura completa la siguen), `DeleteLevel` (con sus muros y muebles), `SetWallFinish`, `SetRoomFinish`, `AddFurniture`, `ChangeFurniture`, `DeleteFurniture`; `CreateWall` y `MoveJoint` por planta), `execute`, `undo`/`redo` con etiquetas, `replaceProject`, notificación `ChangeSet` (ADR-0006); `setWallObstructions`: con él, `AddOpening` y un `ChangeOpening` que mueve o ensancha lo que ocupa el hueco (bolsillo incluido) se rechazan sobre una unión (ADR-0013, ADR-0014) |
| Archivo | `F101/Cad/ProjectFile.h` | `project.json` esquema 7 (plantas, huecos con su carpintería y cómo abren, acabados, muebles; lee 1 y 2 en una sola planta, y huecos de 1 a 5 como puertas abatibles y ventanas fijas): escribir/leer texto, guardar (atómico) y abrir archivos (ADR-0007, ADR-0010 a ADR-0014, ADR-0016) |
| Snapping | `F101/Cad/Snapping.h` | `snapPoint` (extremos, eje de un muro, rejilla), `snapFromAnchor` (extremos, muro — donde lo cruza el ángulo o su punto más cercano —, ángulo, longitud en rejilla; `constrained` = dirección exacta), listas de muros a ignorar, `pointAtDistance` (longitud escrita, error < 0,5 mm) |
| Picking | `F101/Cad/Picking.h` | `Ray3` en mm; `pickWall` (caja del muro, con muros a ignorar), `intersectPost` (asas), `intersectFloor` (z = 0), `pickFurniture` (caja girada del mueble, en una planta) |
| Herramienta de muro | `F101/Cad/WallTool.h` | clic → vista previa → longitud escrita → Enter/clic; encadena muros (§19); dibuja en una planta (altura de la planta por defecto) |
| Edición | `F101/Cad/EditController.h` | planta activa (rayo bajado a su cota; subir, bajar, añadir, borrar, propiedades; "ver todas"), modos Seleccionar/Muro/Puerta/Ventana/Pintar/Amueblar (acabado en la cara que toca el rayo o en el suelo de la habitación), selección de habitaciones por su suelo (`RoomLocator` inyectado), acabados de la selección, selección de muros y huecos (clic en el hueco), colocación de huecos (centrados en el puntero, borde en la rejilla de 50 mm, en el tramo libre de uniones y de otros huecos más cercano, muro anfitrión a menos de medio espesor + 150 mm), arrastre de esquinas (todos los muros que la comparten, con desfase de agarre), arrastre de huecos (el hueco lo mueve, sus asas cambian el ancho, mínimo 300 mm, dentro de su tramo libre con su bolsillo; un solo `ChangeOpening` al soltar), acabado de su carpintería y cómo abre (`setOpeningOperation`, `flipOpeningHinge`, `flipOpeningSide`, `setOpeningOpenPercent`; `flip()` con Espacio: bisagras de los siguientes huecos o del seleccionado; al colocar, abre hacia el lado del muro donde está el cursor), muebles (ADR-0016: colocación en la rejilla con el tipo elegido; los que van contra un muro se apoyan de espaldas en la cara más cercana del lado del cursor y se deslizan por el muro hasta los muros con que chocan; Espacio gira 90°; selección por su caja, más cerca que los muros; arrastre con el mismo snapping y un solo `ChangeFurniture` al soltar; ancho, fondo, alto, giro en grados y acabado; girar y borrar), propiedades escritas, cancelar, deshacer/rehacer; el cursor en planta es el punto bajo el muro señalado o el suelo; `view()` con los muros previstos y la planta activa (ADR-0008 a ADR-0011) |
| Errores | `F101/Cad/Result.h`, `Assert.h` | `Result<T>`/`Status` para errores esperables; `F101_ASSERT` para invariantes |

Reglas:
- Todo cambio del modelo es un comando validado: uno rechazado deja el proyecto y el historial intactos; uno que no
  cambia nada no se registra.
- Los suscriptores se ejecutan de forma síncrona tras cada transacción aplicada (así se regenera la escena).
- La interacción recibe rayos en espacio de escena; nunca tipos de un motor. El snapping es del núcleo (§20).
- Las uniones y las habitaciones no son parte del modelo: se derivan de los muros en el Geometry Engine (ADR-0009).
- Las cotas de las plantas no se guardan: se suman las alturas de abajo. Los forjados tampoco: se derivan de las
  habitaciones (ADR-0011).
- Los acabados son ids del catálogo; los de las habitaciones se guardan por su clave porque las habitaciones son
  derivadas (ADR-0012).
- Los huecos viven dentro de su muro y se miden a lo largo del eje desde su inicio: siguen al muro. Un cambio del muro
  que dejaría un hueco fuera se rechaza con el motivo; nunca se recolocan huecos en silencio (ADR-0010). Un hueco
  tampoco puede añadirse, moverse ni ensancharse sobre la unión con otro muro; la carpintería se deriva, solo se guarda
  su acabado (ADR-0013). Cómo abre sí se guarda; cuánto está abierto solo cambia cómo se muestra (ADR-0014).
- Los muebles guardan solo qué son y dónde están (tipo, planta, centro, giro, medidas, acabado); su forma se deriva en el
  Geometry Engine. El snapping a muros no crea relación: mover un muro no mueve sus muebles (ADR-0016).
- `Export.h` define `DLLEXPORT`/`DLLIMPORT` igual que Unreal para que las macros `FLOOR101*_API` funcionen en el build
  modular del editor sin incluir cabeceras de Unreal.
- nlohmann/json 3.12.0 (`ThirdParty/nlohmann`) se usa solo dentro de `ProjectFile.cpp`; no aparece en cabeceras
  públicas.

Tests: `Tests/Unit/UnitsTests.cpp`, `IdTests.cpp`, `Geometry2DTests.cpp`, `WallTests.cpp`, `CommandTests.cpp`,
`ProjectFileTests.cpp`, `SnappingTests.cpp`, `PickingTests.cpp`, `WallToolTests.cpp`, `EditControllerTests.cpp`,
`LevelTests.cpp`, `FinishTests.cpp`, `CarpentryTests.cpp`, `OpeningTypeTests.cpp` y `FurnitureTests.cpp` (también Geometry, Scene Model y Drawing).
