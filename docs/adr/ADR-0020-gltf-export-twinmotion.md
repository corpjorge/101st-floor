# ADR-0020: Exportación glTF (.glb) para Twinmotion

Estado: aceptado y verificado (MVP 12, 2026-09-30); la importación en Twinmotion la confirma el usuario

## Contexto
El usuario quiere "que la casa se pueda enviar a Twinmotion". CLAUDE.md fija que Twinmotion solo presenta: el modelo de
101st Floor sigue siendo la fuente de verdad y Twinmotion recibe una copia. La arquitectura ya reservaba la capa "Scene
Exporters" (ADR-0003), que escribe contra la escena neutral, en el orden glTF → USD → Datasmith. Twinmotion importa
`.glb`/`.gltf`, `.fbx`, `.obj`, `.udatasmith` y otros, y al reimportar un `.glb` o un `.fbx` puede conservar los
materiales que el usuario puso en Twinmotion ("Use scene materials"). El usuario eligió `.glb` antes que Datasmith
Direct Link (comparación en la conversación del MVP 12).

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Formato | **glTF 2.0 binario (`.glb`)** | estándar abierto; un solo archivo con geometría, materiales e imágenes; lo leen Twinmotion, Blender y visores web; se prueba sin Unreal | no sincroniza en vivo (exportar y reimportar) |
| Formato | FBX | muy extendido | formato cerrado (SDK de Autodesk); materiales pobres |
| Formato | Datasmith + Direct Link | sincronización en vivo | depende de módulos de Epic que no entran en un build empaquetado (o del SDK compilado desde el código fuente); difícil de probar; se deja para después, reutilizando esta conversión |
| Escritura | Exportador glTF de Unreal (`GLTFExporter`) | ya existe | solo en el Editor; exporta actores, no el modelo; nuestras mallas son dinámicas |
| Escritura | **Módulo puro `Floor101Exporters` contra `scene::Scene`** | la misma escena que ve Unreal; sin Unreal; mismo resultado en juego, editor, MCP y tests | hay que escribir el glTF y un codificador PNG |
| Repetición de las texturas | Extensión `KHR_texture_transform` (UV en metros) | UV reales | soporte incierto en Twinmotion; si no se lee, las texturas salen a otra escala |
| Repetición de las texturas | **Horneada en los UV de cada primitiva (`uv · 1000 / repetición`)** | se ve igual en cualquier lector | al cambiar un material en Twinmotion, su escala es por repetición de la textura original |
| Imágenes | PNG con zlib de terceros | compresión óptima | dependencia nueva |
| Imágenes | **Codificador PNG propio (deflate con LZ77 y códigos Huffman fijos)** | sin dependencias; determinista; probado con un decodificador independiente y con el de Unreal | archivos algo mayores que con códigos dinámicos |

## Decisión
- **Módulo `Floor101Exporters`** (C++ puro, depende de CadCore, Geometry, SceneModel y Assets; nlohmann/json interno):
  `writeGlb(scene, project, materiales, textos)`, `projectMaterials(...)`, `exportProjectGlb(proyecto, carpeta,
  ModelCache, textos)` y `saveGlbFile` (escritura a un temporal y renombrado). `encodePng`/`zlibCompress`/`crc32`/`adler32`
  en `Png.h`.
- **Qué se exporta:** `compileScene(project, modelos)` sin overlay: todas las plantas con sus forjados, muros, uniones,
  suelos, carpintería con su vidrio y muebles (paramétricos, de la biblioteca e importados). Nunca selección, vistas
  previas, tiradores ni arcos de apertura.
- **Unidades y ejes:** metros, Y arriba: un punto `(x, y, z)` en mm pasa a `(x, z, −y) / 1000`. Es una rotación, así que
  los triángulos conservan su sentido; las normales giran igual.
- **Estructura:** un nodo raíz ("101st Floor"), un nodo por planta de abajo arriba ("Planta baja", "Planta 1"…), dentro un
  grupo por clase (Muros, Suelos, Forjados, Puertas y ventanas, Muebles; con el nombre de la planta si hay varias) y un
  objeto por nodo de la escena con una malla de una primitiva por sección. Nombres estables y únicos ("Muro 3f2a9c1e",
  "Sofá de tres plazas 7117ade1"); en `extras`, el id del nodo (`f101NodeId`), su clase (`f101Source`), el de la planta
  (`f101LevelId`) y el del acabado de cada material (`f101FinishId`), para reimportar y para herramientas.
- **Materiales:** uno por acabado en orden de primer uso, con el nombre de la UI. Color base (lineal) × imagen,
  rugosidad, metálico 0, como el material de la aplicación. Los acabados del catálogo llevan su textura sintetizada
  (512 px) en PNG; uno de un solo color, solo el color; el vidrio, su tinte con `alphaMode: BLEND` y su opacidad. Las
  texturas importadas y las fotografías de la biblioteca viajan como su archivo PNG/JPEG, con su repetición y reflejadas
  en v para superficies (como `textureFromImage`). Los materiales de los modelos, con su color, rugosidad e imagen. Las
  imágenes iguales se guardan una vez; un muestreador lineal con mipmaps que repite.
- **Aplicación:** `UFloor101DocumentSubsystem::ExportGltf(ruta)` y `GetDefaultGltfPath()` (`<proyecto>.glb` junto a él),
  nombres en español (`Floor101Names`: los mismos que la UI); botón "Twinmotion (.glb)" en la barra, consola
  `f101.Export.Gltf [ruta]` y herramienta MCP `ExportGltf`. El estado dice ruta, objetos, materiales y tamaño; los avisos
  (p. ej. una imagen ilegible, que deja su material con su color) van al log y al estado.
- **Segunda opinión:** `Scripts/CheckGltf.ps1` importa el `.glb` en Blender sin interfaz, lista lo leído y renderiza una
  imagen sin forjados.

## Consecuencias
- Cada cambio se lleva a Twinmotion exportando de nuevo y refrescando la importación; Direct Link queda como paso
  posterior y reutilizará nombres, materiales y conversión.
- La escala de las texturas está en los UV: un material de Twinmotion aplicado encima usará esa escala.
- Solo color, rugosidad y opacidad: sin mapas de normales, metal ni luces (Twinmotion añade los suyos).
- Los materiales de los modelos de la biblioteca conservan los nombres de sus archivos (en inglés).

## Verificación
- Unitarios: `PngTests.cpp` (CRC-32 y Adler-32 de referencia; zlib de ida y vuelta con un inflador independiente; PNG
  RGB y RGBA exactos, sRGB, deterministas; textura de ladrillo comprimida) y `GltfExportTests.cpp` (cabecera y trozos
  GLB; raíz, planta, grupos y objetos con nombres estables y únicos e ids; determinismo; relectura con el lector glTF
  del proyecto en metros con Y arriba y todos los triángulos mirando hacia su normal; materiales con PNG, color y vidrio;
  UV con la repetición y el reflejo; texturas y materiales de modelos importados con sus archivos; imagen ilegible
  avisada; plantas nombradas; proyecto vacío; acabado desconocido en magenta; escritura del archivo). 241 casos.
- Unreal Automation: `Floor101.Interaction.Export` (exportar junto al proyecto con ruta no ASCII, estado, cabecera,
  nombres en español y el PNG del ladrillo decodificado por Unreal píxel a píxel igual que la textura de la
  aplicación; otra extensión rechazada) y la spec del toolset (dos casos). 52 pruebas.
- Blender 5.2 (`CheckGltf.ps1`) con el salón del MVP 11: 16 objetos, 32 materiales, 7 imágenes, 18 948 triángulos,
  límites de 5,12 × 4,25 × 2,8 m, render con el ladrillo, el parquet en espiga y los muebles en su sitio. Ver
  `docs/mvp12-report.md`.
