# Geometry Engine (`Source/Floor101Geometry`, `f101::geometry`)

Geometría derivada del modelo. C++20 puro. Nunca se edita ni se guarda: se regenera (ADR-0009).

- `MeshData`: posiciones y normales en mm (`double`, escena dextrógira, Z arriba), UV en metros (1,0 = 1000 mm) e
  índices agrupados en `MeshSection` por slot de material. Triángulos en sentido antihorario vistos desde el lado al que
  apunta su normal.
- **Uniones** (`WallJoins.h`): `solveWallJoins(project)` devuelve la huella (`WallFootprint`, cuatro esquinas en mm) de
  cada muro y los `JointFill` de los nudos de tres o más extremos. Extremos que comparten un punto → inglete con el
  vecino angular (L, Y, X; espesores distintos); colineales → a escuadra; extremo dentro del cuerpo de otro muro → corte
  en su cara (T); esquina muy aguda (inglete > 4 × semiespesor) → escuadra; huella inválida → rectángulo.
  `plainFootprint(wall)` es el rectángulo de un muro suelto. `wallObstructions(levelWalls, wall)` da los tramos de su eje
  ocupados por otros muros (ADR-0013): los extremos hasta donde existen ambas caras (ingletes) y la huella de cada muro
  que lo toca (T, cruces) recortada a la banda del muro; fusionados y redondeados hacia fuera a mm enteros.
  `makeWallObstructions()` los entrega al `Document`, que así rechaza huecos sobre una unión.
- **Muros** (`WallGeometryBuilder.h`): `buildWallMesh(wall, footprint)` es el prisma sobre la huella, de z = 0 a la
  altura. Sombreado plano; sin huecos, 24 vértices, 12 triángulos y 6 secciones (`left`, `right`, `top`, `bottom`,
  `start`, `end`; izquierda y derecha respecto al eje inicio → fin). Cada puerta o ventana (ADR-0010) es un hueco: las
  caras largas se parten en una rejilla con los bordes de los huecos y se omiten las celdas interiores, las puertas
  recortan la cara inferior (Sutherland–Hodgman a lo largo del eje, `addConvexPolygon`) y las mochetas (jambas, dintel
  y alféizar de ventana) van en la sección `reveal`. Los huecos se recortan a donde existen ambas caras (un hueco que
  llega a un inglete no lo atraviesa). La malla sigue cerrada. Las caras largas siguen paralelas al eje;
  las de los extremos son los ingletes o cortes. `buildWallMesh(wall)` usa la huella simple (misma malla que en el
  MVP 1). El orden de cada cara se deduce de su normal (`addQuad`), así que es correcto por construcción.
- **Polígonos** (`PolygonGeometry.h`): `signedArea`, `containsPoint`, `triangulatePolygon` (orejas, admite cóncavos y
  vértices colineales) y `buildPrismMesh` (secciones superior, lateral e inferior). Sirve a los `JointFill` y a los
  suelos.
- **Habitaciones** (`Rooms.h`): `detectRooms(project)` → caras acotadas del grafo plano de ejes (partidos en T y cruces).
  Cada `Room` trae el contorno del suelo (caras interiores de los muros), el área neta, un punto de etiqueta interior, sus
  muros y una clave determinista. `buildFloorMesh(room)` → losa de 10 mm (`floor`, `floor_edge`, `floor_bottom`).
  `buildSlabMesh(room, bottomZ, topZ)` → forjado sobre el mismo contorno (`slab_top`, `slab_edge`, `ceiling`; ADR-0011).
  `makeRoomLocator()` da al `EditController` la habitación cuyo suelo contiene un punto (seleccionar y pintar suelos).
- **Carpintería** (`CarpentryBuilder.h`, ADR-0013, ADR-0014): `buildOpeningMesh(wall, footprint, opening)` → en el hueco
  recortado, marco centrado en el grueso (50 mm de perfil, 70 mm de fondo o el grueso), jambas y cabecero (ventanas,
  también travesaño inferior), y según la operación: puerta abatible, hoja de 40 mm (3 mm de holgura, 10 mm sobre el
  suelo) con una manilla por cara, girada alrededor de su bisagra hacia `opensTo` (100 % = 90°); de dos hojas, dos medias
  hojas; corredera, la hoja desplazada dentro del muro hacia `hinge` con un tirador por cara; ventana practicable (una o
  dos hojas de 50 mm con perfiles y vidrio, giradas igual, manilla en la cara hacia la que abre), corredera (dos hojas
  solapadas 50 mm en dos vías; la de `opensTo` corre sobre la otra) o fija (vidrio de 6 mm y montante si mide más de
  1300 mm). Las piezas que giran se construyen en un marco local (bisagra, dirección de la hoja, grueso). Secciones
  `frame`, `leaf`, `hardware`, `glass`; cajas cerradas de sombreado plano.
  `buildOpeningSymbolMesh` → el símbolo de las puertas abatibles y de dos hojas: por hoja, el arco de 90° y la hoja
  abierta, líneas planas de 20 mm a 3 mm sobre el suelo, desde la cara hacia la que abre (sección `swing`).
- **Muebles** (`FurnitureBuilder.h`, ADR-0016): `furnitureShape(furniture)` → las cajas de su tipo en el marco del mueble
  (x a lo ancho desde el centro, y del frente a la espalda, z desde el suelo), dimensionadas por su ancho, fondo y alto:
  camas (cabecero, patas, somier, colchón, almohadas, edredón), sofá (patas, base, brazos, respaldo y cojines), mesa, silla,
  armario (puertas de unos 55 cm con tiradores), cocina (zócalo, módulos abiertos por arriba, frentes con tiradores,
  encimera con el hueco del fregadero, fregadero de acero con grifo y placa negra si mide 1,20 m o más), frigorífico,
  inodoro, lavabo suspendido (mueble, encimera cerámica con cubeta y grifo), bañera (cubeta abierta y grifo) y plato de
  ducha (con desagüe); más los contornos que el plano dibuja dentro (almohadas, edredón, cubetas, fuegos...). Ranuras:
  `body` (el acabado del mueble), `linen`, `fabric`, `ceramic`, `metal`, `stone`, `dark`, con `furnitureSlotFinish`.
  `buildFurnitureMesh` gira y coloca las cajas con su marco sobre el suelo de la habitación (z = `FloorThicknessMm`).

Tests: `WallGeometryTests.cpp` (bounds exactos de 3550 × 120 × 2400 mm, normales hacia fuera, orden de triángulos, lado
izquierdo, espesor en diagonal, regeneración sin escalar; puerta: 36 triángulos, áreas por sección y volumen encerrado
exactos; puerta y ventana juntas), `WallJoinTests.cpp`, `PolygonGeometryTests.cpp`, `RoomTests.cpp` (casos en
ADR-0009), `CarpentryTests.cpp` (mallas de carpintería, tramos de unión, recorte en un inglete) y `OpeningTypeTests.cpp`
(hojas giradas y desplazadas por tipo, símbolos) y `FurnitureTests.cpp` (cajas de los 12 tipos a medidas mínimas, por
defecto y máximas; malla girada; acabados por ranura).

Todo es local a una planta: la escena resuelve uniones y habitaciones planta a planta y sube la geometría a su cota.

Cada sección de una malla recibe en la escena el acabado de su cara (ADR-0012).

Futuro: puertas plegables y oscilobatientes, escaleras y huecos en forjados.
