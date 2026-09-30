# ADR-0017: Assets importados (texturas y modelos 3D)

Estado: aceptado y verificado (MVP 10, 2026-09-30)

## Contexto
El MVP 10 (pedido por el usuario: "que se puedan importar assets, como texturas, muebles, etc.") añade al proyecto
imágenes y modelos 3D del usuario: una textura se pinta en muros, suelos, techos, carpintería y muebles, y un modelo se
coloca como un mueble más. Hay que decidir qué formatos se leen, dónde viven los archivos, qué guarda `project.json`,
quién lee y normaliza los modelos (sin romper "Unreal solo representa") y cómo se usan en la edición.

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Lectura | Importadores de Unreal (Interchange, `UTexture2D`/`UStaticMesh` como assets) | muchos formatos | assets `.uasset` fuera de Git y de `project.json`; no existen en `-game` sin editor; lógica de producto en Unreal |
| Lectura | **Lectores propios en C++ puro (glTF 2.0 y OBJ/MTL) en un módulo nuevo `Floor101Assets`** | testeables sin Unreal; mismo resultado en editor, juego y tests | solo lo que se implementa (sin FBX, sin extensiones glTF) |
| Archivos | Rutas absolutas al archivo original | sin copias | el proyecto se rompe al mover o compartir la carpeta |
| Archivos | **Copia en `<carpeta del proyecto>/assets/<uuid>/` con sus dependencias; rutas relativas** | el proyecto es autocontenido; mover la carpeta funciona | ocupa disco; guardar en otra carpeta copia los assets |
| Referencia | Tabla aparte de acabados/tipos importados | explícito | dos caminos en cada validación y en la UI |
| Referencia | **`asset:<uuid>` como id de acabado (textura) o tipo de mueble (modelo)** | todo lo que ya acepta un acabado o un tipo acepta un asset; los muebles reutilizan colocación, snapping, selección y plano | la validación necesita el proyecto (`validateFinishIn`, `furnitureSpec`) |
| Materiales del modelo | Un acabado elegido por el usuario para todo el modelo | simple | pierde el aspecto del modelo |
| Materiales del modelo | **Un acabado derivado por material (`asset:<uuid>#<i>`: color, rugosidad, imagen del modelo); el mueble puede sustituirlos todos por uno** | el modelo se ve como se exportó; se puede pintar | los materiales PBR avanzados (normales, metal, emisión) se ignoran |

## Decisión
- **Modelo** (`F101/Cad/Asset.h`): `Project::assets` en orden de importación. `Asset { id, kind (Texture | Model), name,
  file }` con `file` relativo a la carpeta del proyecto y dentro de su propia carpeta `assets/<uuid>/` (sin `..`, sin
  unidades de disco, con `/`). Textura: `widthMm × heightMm` que cubre una repetición de la imagen (por defecto 1 m de
  ancho y alto proporcional), rugosidad 0-100 %, superficies admitidas (nunca vidrio) y los píxeles, informativos. Modelo:
  unidad de sus coordenadas (m, cm, mm, in), eje arriba (Y o Z), frente en cuartos de vuelta (0-3), si se apoya en los
  muros, y el tamaño y los triángulos medidos con esos ajustes.
- **Referencias**: `asset:<uuid>` (textura como acabado, modelo como tipo de mueble); `asset:<uuid>#<i>` es el acabado del
  material `i` de un modelo, nunca elegido por el usuario. Sin proyecto, `validateFinish` solo acepta la forma; con él,
  `validateFinishIn` exige una textura existente que admita la superficie y `furnitureSpec` da el tipo de un modelo (sus
  medidas por defecto, rango de 10 mm a 100 m por eje y si va contra un muro). `validateProject` comprueba cada
  referencia.
- **Comandos**: `AddAssetCommand` (conserva su id: los archivos ya están en su carpeta), `ChangeAssetCommand` (sin
  cambiar tipo ni archivo) y `DeleteAssetCommand`, que se rechaza mientras algo lo use ("'…' is in use (N …)"). Transacción
  y `ChangeSet` con altas, cambios y bajas; etiquetas "Import/Change/Delete asset". Borrar un asset no borra sus archivos
  (deshacer lo devuelve).
- **`project.json` esquema 8**: `"assets": [{ id, kind, name, file, texture: { widthMm, heightMm, roughnessPercent,
  surfaces, pixelWidth, pixelHeight } | model: { unit, up, frontQuarterTurns, againstWall, widthMm, depthMm, heightMm,
  triangles } }]`, solo si hay assets. Los esquemas 1-7 se siguen leyendo.
- **Módulo `Floor101Assets`** (C++ puro, depende de CadCore y Geometry): lee PNG/JPEG (cabecera: tamaño, hasta 16384 px),
  glTF 2.0 (`.gltf` con buffers externos o `data:`, `.glb`; jerarquía de nodos con matrices o TRS; triángulos, tiras y
  abanicos; color base, rugosidad e imagen de color; rechaza `extensionsRequired` y accesores dispersos) y OBJ con MTL
  (`Kd`, `map_Kd`, `Ns`/`Pr`; polígonos en abanico). Límites: 512 MB por archivo, 2 millones de triángulos. `importAsset`
  copia el archivo y lo que necesita (MTL, buffers, texturas; las imágenes empaquetadas se escriben como archivos),
  adivina la unidad de un OBJ (la primera de m, cm que da un tamaño plausible ≤ 30 m; si no, mm), lo mide y lo valida; si
  falla no deja nada. `normalizeModel` lo lleva al marco del mueble: Z arriba, mm, frente hacia −y, centrado en su huella
  y apoyado en z = 0, con una sección `m<i>` por material. `copyAssetFiles` copia las carpetas al guardar en otra
  carpeta. `ModelCache` lee cada modelo una vez por archivo, ajustes y fecha.
- **Geometría y escena**: `buildModelFurnitureMesh` estira el modelo por eje a las medidas del mueble (normales con la
  inversa traspuesta), lo gira y lo apoya en el suelo; si el modelo no se puede leer, una caja de su tamaño
  (`buildPlaceholderFurnitureMesh`) con el acabado de la carpintería. El compilador recibe un `ModelProvider` inyectado;
  cada sección lleva el acabado del mueble o el de su material. `textureFromImage` convierte una imagen en textura
  repetible (potencia de dos, con mips; volteada para superficies, tal cual para modelos).
- **Plano**: la huella del modelo como contorno.
- **Unreal**: el subsistema registra en `UFloor101FinishLibrary` un acabado por textura y por material de modelo (imagen,
  tinte, rugosidad, repetición) antes de compilar; las imágenes se decodifican con `FImageUtils` una vez por archivo y los
  materiales se actualizan en su sitio. Al abrir un proyecto, su ruta pasa a ser la del proyecto antes de reconstruir la
  escena, así que los archivos se leen de su carpeta.
- **UI** (Slate, en español): en Pintar, "Texturas importadas" (muestra con la imagen, píxeles), "Importar textura…" y
  nombre, ancho y alto de la imagen, rugosidad, "Eliminar textura"; en Amueblar, "Modelos importados", "Importar
  modelo…" y nombre, unidades del archivo, "Cambiar eje arriba", "Girar frente 90°", "Se apoya en los muros", tamaño y
  triángulos, "Eliminar modelo". El diálogo de archivo es el nativo de Windows (`GetOpenFileNameW`), filtrado por tipo.
  Los combos de acabado incluyen las texturas importadas (y "Original" para un mueble importado). Consola:
  `f101.Asset.Import <ruta>`.
- **MCP**: `GetAssets`, `ImportAsset`, `SetTextureAsset`, `SetModelAsset`, `RemoveAsset`; `AddFurniture` con un modelo
  usa sus medidas por defecto.

## Consecuencias
- Un proyecto con assets es una carpeta (`project.json` + `assets/`), no un único archivo; guardar en otra carpeta copia
  los assets.
- Solo glTF 2.0 y OBJ: FBX, USD, `.blend` y las extensiones glTF (Draco, KTX2, texturas WebP…) no se leen; de los
  materiales solo se usan color, rugosidad e imagen de color.
- Un modelo es un bloque que se estira por eje: no tiene partes paramétricas ni detalle en planta (solo su huella).
- Cambiar unidad, eje o frente de un modelo vuelve a medirlo, pero los muebles ya colocados conservan sus medidas.
- Las imágenes se leen de nuevo solo si cambia su ruta; reemplazar a mano un archivo con el juego abierto no se ve hasta
  reabrir.

## Verificación
- Unitarios (`AssetTests.cpp`, `AssetImportTests.cpp`): referencias y rutas; validación; comandos con deshacer, rechazo
  de borrar en uso; `validateProject`; JSON esquema 8 exacto, ida y vuelta y errores; PNG/JPEG; glTF (GLB, `data:`,
  jerarquía, espejo, tiras, extensiones rechazadas) y OBJ/MTL; normalización (ejes, unidades, frente); importación y
  copia; muestras reales exportadas con Blender (`Tests/Data/Assets`). 222 casos.
- Unreal Automation: `Floor101.Interaction.Assets` (importar parquet, sillón GLB y lámpara OBJ; pintar, colocar,
  materiales e imágenes; guardar en otra carpeta y reabrir, también desde un proyecto sin esos archivos) y la spec del
  toolset (importar, usar, cambiar, borrar, errores). 45 pruebas; `Assets`, `Furniture`, `Finishes` y `PlanView` también
  en el editor real por MCP.
- Prueba real y MCP: ver `docs/mvp10-report.md`.
