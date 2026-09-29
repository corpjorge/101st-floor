# Scene Model (`Source/Floor101SceneModel`, `f101::scene`)

Escena neutral compilada desde el modelo (ADR-0003). C++20 puro. Consumidores: Unreal hoy; exportadores y vista 2D en
el futuro.

- `compileScene(project)`: primero un nodo por muro en el orden del modelo, con sus uniones resueltas
  (`<wall uuid>/body`); luego un nodo por `JointFill` (`<wall uuid>/joint-start|end`), uno por suelo de habitación
  (`<room key>/floor`), uno por forjado (`<room key>/slab`, en la franja alta de su planta) y uno por hueco con su
  carpintería (`<wall uuid>/opening/<opening uuid>`, ADR-0013; en la planta y con el estilo de su muro). Uniones y habitaciones se
  resuelven planta a planta y cada planta se sube a su cota (ADR-0011). `Scene::rooms` lista esas habitaciones
  (`SceneRoom`: habitación, planta y cota) para las vistas; cada nodo lleva su `level`. Es
  determinista: el mismo proyecto da la misma escena (ADR-0009).
- `SceneNodeId`: derivado, nunca aleatorio, de modo que el mismo muro conserva su nodo al editarse.
- `finishes` (ADR-0012): el acabado de cada sección, en el orden de `mesh.sections` (caras del muro según el modelo;
  parte superior, extremos, mochetas y uniones con el acabado de muro por defecto; suelos con el de su habitación; la
  cara `ceiling` del forjado con el techo de su habitación; marco y hoja con la carpintería del hueco, manillas con
  `aluminium-anthracite` y vidrio con `glass-clear`). Siempre resuelto: nunca vacío.
- `FinishTextures.h`: `buildFinishTexture` sintetiza una repetición sin costuras de un acabado (ladrillo a soga,
  baldosas, tablas, moteado o liso) con todos sus mips y el tamaño en mm que cubre; `buildFinishLook` añade la
  rugosidad y la opacidad (menor que 1: translúcido, vidrio) y pinta de magenta un id desconocido. Deterministas: el
  mismo acabado da los mismos píxeles.
- `contentHash`: FNV-1a 64 sobre posiciones, normales, UV, secciones y acabados (`hashNode`). Los consumidores lo usan para no reconstruir lo que
  no cambió. Al mover un muro cambian también sus vecinos (sus ingletes dependen de él).
- Metadatos de origen: `sourceKind` (`Wall`, `Joint`, `Room`, `Slab`, `Opening`, `Marker`), `sourceId` (UUID del muro;
  en suelos y forjados, la clave de la habitación; en la carpintería, el UUID del hueco) y `name` legible (`Wall 1a2b3c4d`, `Room 11.17 m2`).
- Espacio: mm, dextrógiro, X este, Y norte, Z arriba. Transform de nodo identidad (geometría en coordenadas de escena).
- Vista: `compileScene(project, overlay)` compila el proyecto **como quedaría** tras el gesto en curso.
  `ViewOverlay::previewWalls` sustituye muros por id (arrastre de una esquina) o añade muros sin id (el que se dibuja,
  nodo `preview/wall/<i>`); ambos llevan estilo `Preview`, y las uniones y habitaciones siguen a la vista previa. El
  muro seleccionado (o el anfitrión del hueco seleccionado) lleva `Selected` y las asas son `marker/<i>` (`Handle`;
  en un hueco, en sus bordes). La colocación de una puerta o ventana es una vista previa del muro anfitrión con el hueco
  nuevo; el arrastre de un hueco, una vista previa del anfitrión con el hueco movido (y su carpintería). Los muros
  inválidos se omiten.
  `ViewOverlay::selectedRoom`: el suelo de esa habitación lleva `Selected`.
  `ViewOverlay::activeLevel` (ADR-0011): sin `showAllLevels`, las plantas de encima no se compilan, la activa no
  muestra sus forjados y las de abajo llevan estilo `Context`; las asas van en la cota de la planta activa.
  `compileScene(project)` no lleva overlay: es lo que usan los exportadores.

La escena nunca se guarda como proyecto ni se edita. Un exportador escribe contra ella con su única conversión de
ejes (glTF: `(x, z, −y) / 1000`).

Tests: `Tests/Unit/SceneTests.cpp` (ID esperado, determinismo, el hash sigue a la geometría, borrado, vista previa unida
al proyecto, sustitución por id, orden muros → uniones → suelos → forjados) y `LevelTests.cpp` (cotas, forjados, vista
por planta), `FinishTests.cpp` (acabados por sección, hash, texturas) y `CarpentryTests.cpp` (nodos de carpintería,
acabados, planta y estilo).
