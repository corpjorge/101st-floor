# Geometry Engine (`Source/Floor101Geometry`, `f101::geometry`)

Geometría derivada del modelo. C++20 puro. Nunca se edita ni se guarda: se regenera.

- `MeshData`: posiciones y normales en mm (`double`, escena dextrógira, Z arriba), UV en metros (1,0 = 1000 mm) e
  índices agrupados en `MeshSection` por slot de material. Triángulos en sentido antihorario vistos desde el lado al que
  apunta su normal.
- `buildWallMesh(wall)`: caja cerrada centrada en el eje, de z = 0 a la altura. Sombreado plano, 24 vértices,
  12 triángulos y 6 secciones (`left`, `right`, `top`, `bottom`, `start`, `end`; izquierda y derecha respecto al eje
  inicio → fin). El orden de cada cara se deduce de su normal (`addQuad`), así que es correcto por construcción.

Los tests (`Tests/Unit/WallGeometryTests.cpp`) comprueban: bounds exactos de 3550 × 120 × 2400 mm, normales
unitarias hacia fuera, orden de triángulos, que la cara izquierda está a la izquierda, espesor real en paredes
diagonales, y que al alargar la pared se regenera y no se escala (la densidad de UV se mantiene en 1 por metro).

Futuro: uniones y esquinas, huecos para puertas y ventanas, suelos y techos. Builders separados por entidad, con el
mismo `MeshData`.
