# Exportadores (`Source/Floor101Exporters`, `f101::exporters`)

Escriben el proyecto en formatos de intercambio para herramientas de presentación (ADR-0003: cada consumidor escribe
contra la escena neutral). C++20 puro (ADR-0002): sin Unreal; se prueban con CMake. Hoy: glTF 2.0 binario para
Twinmotion (ADR-0020).

| Pieza | Fichero | Qué es |
|---|---|---|
| glTF | `F101/Exporters/Gltf.h` | `writeGlb(scene, project, materiales, textos)` → `GltfExport` (bytes del `.glb`, objetos, materiales, imágenes, triángulos, avisos); `projectMaterials` (el aspecto de cada acabado: catálogo sintetizado, texturas importadas y fotografías como su archivo, materiales de modelos); `exportProjectGlb(proyecto, carpeta, ModelCache, textos)` (compila todo el proyecto con sus modelos y lo escribe; error si no hay nada); `saveGlbFile` (temporal y renombrado); `GltfTexts` (nombres de plantas, grupos y objetos, acabados y muebles del catálogo, en el idioma de la aplicación) |
| PNG | `F101/Exporters/Png.h` | `encodePng` (8 bits sRGB, RGB si todo es opaco, filtro por fila con la heurística habitual), `zlibCompress` (deflate con LZ77 de 32 KB y códigos Huffman fijos), `crc32`, `adler32` |

Reglas del `.glb` (ADR-0020):
- Metros, Y arriba: `(x, y, z)` mm → `(x, z, −y) / 1000`; es una rotación, los triángulos conservan su sentido.
- Raíz → plantas (de abajo arriba) → grupos (Muros, Suelos, Forjados, Puertas y ventanas, Muebles) → un objeto por nodo de
  la escena con una primitiva por sección. Nombres estables y únicos; ids en `extras` (`f101NodeId`, `f101Source`,
  `f101LevelId`, `f101FinishId`).
- Un material por acabado: color base lineal × imagen, rugosidad, metálico 0; vidrio con `alphaMode: BLEND`. Los UV de
  cada primitiva llevan la repetición de su textura (`uv · 1000 / repetición`), reflejados en v para las imágenes de
  superficie que vienen de un archivo. Imágenes iguales, una vez.
- Determinista: el mismo proyecto da los mismos bytes.

Comprobación externa: `Scripts/CheckGltf.ps1 -Path <archivo.glb>` lo importa en Blender (sin interfaz), lista lo leído y
renderiza una imagen sin forjados (`Tests/Tools/check_gltf.py`).

Tests: `Tests/Unit/PngTests.cpp` (con un inflador independiente) y `GltfExportTests.cpp` (relectura con el lector glTF de
`Floor101Assets`).
