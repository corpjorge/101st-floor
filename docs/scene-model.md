# Scene Model (`Source/Floor101SceneModel`, `f101::scene`)

Escena neutral compilada desde el modelo (ADR-0003). C++20 puro. Consumidores: Unreal hoy; exportadores y vista 2D en
el futuro.

- `compileScene(project)`: primero un nodo por muro en el orden del modelo, con sus uniones resueltas
  (`<wall uuid>/body`); luego un nodo por `JointFill` (`<wall uuid>/joint-start|end`) y uno por suelo de habitación
  (`<room key>/floor`). `Scene::rooms` lista esas habitaciones (área, punto de etiqueta, muros) para las vistas. Es
  determinista: el mismo proyecto da la misma escena (ADR-0009).
- `SceneNodeId`: derivado, nunca aleatorio, de modo que el mismo muro conserva su nodo al editarse.
- `contentHash`: FNV-1a 64 sobre posiciones, normales, UV y secciones. Los consumidores lo usan para no reconstruir lo que
  no cambió. Al mover un muro cambian también sus vecinos (sus ingletes dependen de él).
- Metadatos de origen: `sourceKind` (`Wall`, `Joint`, `Room`, `Marker`), `sourceId` (UUID del muro; en los suelos, la
  clave de la habitación) y `name` legible (`Wall 1a2b3c4d`, `Room 11.17 m2`).
- Espacio: mm, dextrógiro, X este, Y norte, Z arriba. Transform de nodo identidad (geometría en coordenadas de escena).
- Vista: `compileScene(project, overlay)` compila el proyecto **como quedaría** tras el gesto en curso.
  `ViewOverlay::previewWalls` sustituye muros por id (arrastre de una esquina) o añade muros sin id (el que se dibuja,
  nodo `preview/wall/<i>`); ambos llevan estilo `Preview`, y las uniones y habitaciones siguen a la vista previa. El
  muro seleccionado lleva `Selected` y las asas son `marker/<i>` (`Handle`). Los muros inválidos se omiten.
  `compileScene(project)` no lleva overlay: es lo que usan los exportadores.

La escena nunca se guarda como proyecto ni se edita. Un exportador escribe contra ella con su única conversión de
ejes (glTF: `(x, z, −y) / 1000`).

Tests: `Tests/Unit/SceneTests.cpp` (ID esperado, determinismo, el hash sigue a la geometría, borrado, vista previa unida
al proyecto, sustitución por id, orden muros → uniones → suelos).
