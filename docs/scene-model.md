# Scene Model (`Source/Floor101SceneModel`, `f101::scene`)

Escena neutral compilada desde el modelo (ADR-0003). C++20 puro. Consumidores: Unreal y el exportador glTF para
Twinmotion (`docs/exporters.md`, ADR-0020).

- `compileScene(project)`: primero un nodo por muro en el orden del modelo, con sus uniones resueltas
  (`<wall uuid>/body`); luego un nodo por `JointFill` (`<wall uuid>/joint-start|end`), uno por suelo de habitación
  (`<room key>/floor`), uno por forjado (`<room key>/slab`, en la franja alta de su planta) y uno por hueco con su
  carpintería (`<wall uuid>/opening/<opening uuid>`, ADR-0013; en la planta y con el estilo de su muro) y uno por mueble en el
  orden del modelo (`furniture/<furniture uuid>`, ADR-0016; `furnitureNodeId`), uno por pilar (`column/<uuid>`,
  `columnNodeId`, tan alto como su planta y `ColumnProudMm` (1 mm) más alto y más ancho por cada lado para quedar encima de muros y forjados sin parpadeo, con su acabado o hormigón visto) y uno por tramo de instalación
  (`service/<uuid>`, `serviceRunNodeId`, con el acabado de su tipo) (ADR-0021). Uniones y habitaciones se
  resuelven planta a planta y cada planta se sube a su cota (ADR-0011). `Scene::rooms` lista esas habitaciones
  (`SceneRoom`: habitación, planta y cota) para las vistas; cada nodo lleva su `level`. Es
  determinista: el mismo proyecto da la misma escena (ADR-0009).
- `SceneNodeId`: derivado, nunca aleatorio, de modo que el mismo muro conserva su nodo al editarse.
- `finishes` (ADR-0012): el acabado de cada sección, en el orden de `mesh.sections` (caras del muro según el modelo;
  parte superior, extremos, mochetas y uniones con el acabado de muro por defecto; suelos con el de su habitación; la
  cara `ceiling` del forjado con el techo de su habitación; marco y hoja con la carpintería del hueco, manillas con
  `aluminium-anthracite` y vidrio con `glass-clear`; el cuerpo de un mueble con su acabado o el de su tipo y sus demás
  partes con los fijos de cada ranura; un modelo importado, ADR-0017, con el acabado del mueble o el de cada material,
  `asset:<uuid>#<i>`). Siempre resuelto: nunca vacío.
- Modelos importados (ADR-0017): `compileScene` recibe un `ModelProvider` opcional (`Asset` → `geometry::ModelMesh`
  leída y normalizada, o nulo) que inyecta quien sabe leer archivos (en Unreal, el `ModelCache` de `Floor101Assets`). Un
  mueble cuyo tipo es un modelo usa `buildModelFurnitureMesh`; sin modelo, una caja de sus medidas.
- `FinishTextures.h`: `buildFinishTexture` sintetiza una repetición sin costuras de un acabado (ladrillo a soga,
  baldosas, tablas, espiga —tablas en ángulo recto que avanzan en diagonal, repetición de 2n anchos—, ajedrezado con el
  color de acento, moteado o liso; ADR-0019) con todos sus mips y el tamaño en mm que cubre; `buildFinishLook` añade la
  rugosidad y la opacidad (menor que 1: translúcido, vidrio) y pinta de magenta un id desconocido. Deterministas: el
  mismo acabado da los mismos píxeles. `textureFromImage` convierte una imagen decodificada (ADR-0017) en textura
  repetible: lados en potencia de dos (máx. 2048), filtrado con vuelta, todos los mips y volteada para superficies (la
  fila de arriba de la imagen va hacia arriba del muro o hacia el norte del suelo).
- `contentHash`: FNV-1a 64 sobre posiciones, normales, UV, secciones y acabados (`hashNode`). Los consumidores lo usan para no reconstruir lo que
  no cambió. Al mover un muro cambian también sus vecinos (sus ingletes dependen de él).
- Metadatos de origen: `sourceKind` (`Wall`, `Joint`, `Room`, `Slab`, `Opening`, `Furniture`, `Column`, `Service`, `Marker`), `sourceId` (UUID del muro;
  en suelos y forjados, la clave de la habitación; en la carpintería, el UUID del hueco; en un mueble, un pilar o un tramo, el suyo) y `name` legible (`Wall 1a2b3c4d`, `Room 11.17 m2`).
- Espacio: mm, dextrógiro, X este, Y norte, Z arriba. Transform de nodo identidad (geometría en coordenadas de escena).
- Vista: `compileScene(project, overlay)` compila el proyecto **como quedaría** tras el gesto en curso.
  `ViewOverlay::previewWalls` sustituye muros por id (arrastre de una esquina) o añade muros sin id (el que se dibuja,
  nodo `preview/wall/<i>`); ambos llevan estilo `Preview`, y las uniones y habitaciones siguen a la vista previa. El
  muro seleccionado (o el anfitrión del hueco seleccionado) lleva `Selected` y las asas son `marker/<i>` (`Handle`;
  en un hueco, en sus bordes). La colocación de una puerta o ventana es una vista previa del muro anfitrión con el hueco
  nuevo; el arrastre de un hueco, una vista previa del anfitrión con el hueco movido (y su carpintería). Los muros
  inválidos se omiten.
  `ViewOverlay::selectedRoom`: el suelo de esa habitación lleva `Selected`.
  `ViewOverlay::selectedFurniture` y `previewFurniture` (ADR-0016): el mueble seleccionado lleva `Selected`; los previstos
  sustituyen por id (arrastre) o se añaden sin id (el que se coloca, `preview/furniture/<i>`), con estilo `Preview`; los
  inválidos se omiten. Los muebles de las plantas de abajo llevan `Context`.
  Pilares e instalaciones (ADR-0021): `selectedColumn`/`previewColumns` y `selectedServiceRun`/`previewServiceRuns` como
  los muebles (`preview/column/<i>`, `preview/service/<i>`). Los tramos solo se compilan con `ViewOverlay::services` (la
  capa de instalaciones, de las redes de `serviceKinds`) o mientras están en vista previa; con la capa, muros, uniones, suelos, forjados y carpintería
  con estilo `Normal` pasan a `Ghost` (translúcidos); los pilares no. `compileScene(project)` lleva siempre los tramos.
  Símbolos (ADR-0014): la escena de edición añade, al final, el arco de cada puerta abatible o de dos hojas de las plantas
  no atenuadas (`<wall uuid>/opening/<opening uuid>/swing`, `sourceKind` `Opening`, estilo `Symbol`); la de exportación
  no los tiene, ni la de una foto (`ViewOverlay::symbols = false`, Modo Foto, ADR-0018).
  `ViewOverlay::activeLevel` (ADR-0011): sin `showAllLevels`, las plantas de encima no se compilan, la activa no
  muestra sus forjados y las de abajo llevan estilo `Context`; las asas van en la cota de la planta activa.
  `compileScene(project)` no lleva overlay: es lo que usan los exportadores.

La escena nunca se guarda como proyecto ni se edita. Un exportador escribe contra ella con su única conversión de
ejes (glTF: `(x, z, −y) / 1000`, en `Floor101Exporters`).

Tests: `Tests/Unit/SceneTests.cpp` (ID esperado, determinismo, el hash sigue a la geometría, borrado, vista previa unida
al proyecto, sustitución por id, orden muros → uniones → suelos → forjados) y `LevelTests.cpp` (cotas, forjados, vista
por planta), `FinishTests.cpp` (acabados por sección, hash, texturas), `CarpentryTests.cpp` (nodos de carpintería,
acabados, planta y estilo) y `OpeningTypeTests.cpp` (símbolos solo en la escena de edición y en la planta activa), `FurnitureTests.cpp` (nodos de muebles, elevación,
estilos), `ServicesTests.cpp` (pilares siempre, tramos con la capa, `Ghost`, vistas previas, planta de arriba) y `AssetTests.cpp` (modelo con sus materiales, con un acabado propio o caja sin modelo; texturas de imágenes).
