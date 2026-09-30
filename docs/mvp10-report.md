# Informe MVP 10 — 2026-09-30

**Estado:** MVP 10 implementado y verificado. Alcance pedido por el usuario: **importar assets** ("texturas, muebles,
etc."). Imágenes PNG/JPEG que se usan como acabados y modelos 3D glTF 2.0 (`.gltf`, `.glb`) u OBJ (con su `.mtl` y sus
texturas) que se colocan como muebles; se copian junto al proyecto y se guardan en `project.json`. Entorno y versiones
iguales al MVP 1.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Modelo | `Asset` en el núcleo (textura: repetición en mm, rugosidad, superficies; modelo: unidad, eje arriba, frente, apoyo en muros, tamaño medido); referencias `asset:<uuid>` como acabado o tipo de mueble; validación contra el proyecto | doctest |
| Comandos | importar, cambiar y borrar deshacibles; borrar uno en uso se rechaza ("'…' is in use (N …)") | doctest, Unreal, MCP |
| `project.json` esquema 8 | `assets` solo si hay; rutas relativas y dentro de `assets/<uuid>/`; se leen 1-7 | doctest (JSON exacto, ida y vuelta, rutas fuera rechazadas) |
| Lectura (módulo puro `Floor101Assets`) | PNG/JPEG; glTF 2.0 con buffers externos, `data:` o GLB, jerarquía de nodos, tiras y abanicos, espejos; OBJ con MTL (`Kd`, `map_Kd`, `Ns`/`Pr`); extensiones requeridas y accesores dispersos rechazados con un mensaje claro | doctest, muestras exportadas con Blender |
| Importación | copia el archivo y sus dependencias (MTL, buffers, texturas; imágenes empaquetadas como archivos) a `assets/<uuid>/`; unidad de un OBJ adivinada; si falla no deja nada | doctest, Unreal, MCP |
| Normalización y malla | Z arriba, mm, frente hacia −y, centrado en su huella; se estira por eje a las medidas del mueble; caja si el modelo no se puede leer | doctest, capturas |
| Materiales | un acabado por material del modelo (color, rugosidad, imagen); el mueble puede tener uno propio ("Original" vuelve a los del modelo); texturas repetibles con mips desde la imagen | doctest, Unreal, capturas con Lumen |
| Pintar | "Texturas importadas" con su imagen, "Importar textura…" (diálogo nativo de Windows), nombre, ancho y alto de la imagen, rugosidad, "Eliminar textura"; los combos de acabado las incluyen | Unreal, prueba real |
| Amueblar | "Modelos importados", "Importar modelo…", nombre, unidades del archivo, "Cambiar eje arriba", "Girar frente 90°", "Se apoya en los muros", tamaño y triángulos, "Eliminar modelo"; se colocan, seleccionan, arrastran, giran y cambian de medidas como cualquier mueble | Unreal, prueba real |
| Plano 2D | la huella de cada modelo | MCP (SVG y PNG) |
| Guardar en otra carpeta | copia los assets; abrir lee los archivos de la carpeta del proyecto abierto | Unreal, MCP |
| MCP | `GetAssets`, `ImportAsset`, `SetTextureAsset`, `SetModelAsset`, `RemoveAsset`; `AddFurniture` con un modelo usa sus medidas | spec del toolset, sesión MCP |
| Consola | `f101.Asset.Import <ruta>` | — |

Decisión y alternativas: ADR-0017.

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **222 casos, 9648 aserciones, todo verde** + arquitectura OK (5 módulos puros, con `Floor101Assets`) |
| Integración Unreal (`Test.ps1 -Integration`) | **45/45** (nuevas: `Floor101.Interaction.Assets` y dos specs de assets del toolset) |
| En el editor real (MCP `AutomationTestToolset`) | `Interaction.Assets`, `Interaction.Furniture`, `Interaction.Finishes` e `Interaction.PlanView`: Success |
| `CheckRendering.ps1` | todo PASS (DX12/SM6, HWRT, Lumen HWRT, VSM, Nanite); GPU 2,90 ms @ 1920×1080 |
| MCP | salón de 5 × 4 m con ventana en `Saved/ManualTest/mvp10`: importados parquet, piedra, sillón GLB (80 × 75 × 90 cm, 224 triángulos, imagen empaquetada escrita aparte) y lámpara OBJ (50 × 50 × 160 cm, con MTL y textura); parquet de 1,20 m en el suelo, piedra de 80 cm en la cara interior del muro norte, dos sillones y la lámpara; sillón girado, ensanchado, con acabado propio y vuelta a "Original"; borrar el sillón en uso rechazado; esquema 8; guardar en otra carpeta copió los 7 archivos y la copia se abrió (4 assets, 3 muebles); plano .svg (7 KB) y .png; capturas con Lumen |

### Prueba real con ratón y teclado (`-game`)
Proyecto de la sesión MCP copiado a `Saved/ManualTest/mvp10-game` y abierto desde el cuadro Archivo: el salón con el
suelo de parquet, la pared de piedra, los sillones y la lámpara. M → paleta con "Texturas importadas" (Parquet roble,
Piedra) → "Importar textura…" abre el diálogo "Importar textura" de Windows filtrado a "Imágenes (PNG, JPEG)"; ruta
escrita + Enter → "lamp_shade 128 × 128 px" queda elegida para pintar; nombre "Rayas", ancho y alto "40" → clic en la
cara exterior del muro sur: rayas cada 40 cm. A → "Importar modelo…" abre el diálogo en la última carpeta, filtrado a
"Modelos 3D (glTF, GLB, OBJ)"; doble clic en `armchair.glb` → "armchair 80 × 75 cm", 224 triángulos; nombre "Sillón",
"Girar frente 90°" → 75 × 80 × 90 cm; clic en el suelo lo coloca con su tapicería y sus patas. V, clic en él → "Mueble
seleccionado: Sillón", acabado "Original"; ancho "100" y arrastrarlo lo mueven y ensanchan; acabado "Rayas" (el combo
lista las texturas importadas con su imagen) → Deshacer vuelve a sus materiales; Guardar → `project.json` con 6 assets
y sus carpetas (`lamp_shade.png`, `armchair.glb` + `embedded_0.png`).

Corregido en la prueba real:
- **Abrir un proyecto de otra carpeta leía sus archivos de la carpeta del proyecto anterior** (el log decía "cannot read
  the image …/Documentos/101st Floor/assets/…"): la escena se reconstruía al sustituir el documento y la ruta del
  proyecto cambiaba después. Ahora la ruta nueva rige antes (y se restaura si falla). La prueba de integración abre la
  copia desde un proyecto de otra carpeta sin esos archivos; sin el arreglo falla (imagen no encontrada, muestra vacía,
  caja en lugar del modelo) y con él pasa.
- **Las muestras de las texturas importadas salían blancas** en la paleta: se quedaban con la textura de la imagen que no
  se pudo leer. Ahora cada muestra consulta la imagen vigente de la textura al pintarse (`GetAssetFinishImage`) y sigue
  sus cambios.
- **La lista de modelos no cambiaba las medidas al girar el frente** (el panel sí): la lista se reconstruye también cuando
  cambian las medidas o los píxeles que muestra.

Durante la validación también se completaron dos pruebas unitarias de casos que el código ya resolvía pero nadie
probaba: un nodo glTF con espejo (escala negativa) conserva sus caras hacia fuera (orden invertido y normales
corregidas), y un OBJ/MTL con finales de línea de Windows (como los deja Git con `autocrlf`) se lee igual.

Observaciones: la primera vez, el diálogo de Windows abre en Documentos; después recuerda la última carpeta.

## Limitaciones conocidas
- Solo glTF 2.0 y OBJ: FBX, USD, `.blend` y extensiones glTF (Draco, KTX2, WebP…) no se leen; de los materiales solo
  se usan color, rugosidad e imagen de color (sin mapas de normales, metal ni emisión).
- Un modelo es un bloque que se estira por eje; en el plano solo se dibuja su huella.
- Un proyecto con assets es una carpeta (`project.json` + `assets/`); guardar en otra carpeta copia los assets.
- Cambiar unidad, eje o frente de un modelo lo vuelve a medir, pero los muebles colocados conservan sus medidas.
- Reemplazar a mano el archivo de una imagen con el juego abierto no se ve hasta reabrir el proyecto.
- El diálogo nativo solo existe en Windows (la plataforma del proyecto, ADR-0004).

## Archivos principales
`Source/Floor101CadCore/…/{Asset,Project,Document,ProjectFile,EditController,Furniture,Finishes,Id}`,
`Source/Floor101Assets/` (nuevo: `Files`, `ImageInfo`, `ObjReader`, `GltfReader`, `ModelLoader`, `AssetImport`),
`Source/Floor101Geometry/…/ModelMesh`, `Source/Floor101SceneModel/…/{SceneCompiler,FinishTextures}`,
`Source/Floor101UnrealAdapter/…/Floor101FinishLibrary`, `Source/Floor101/…/{Floor101DocumentSubsystem,SFloor101Overlay,
Floor101FileDialog}`, `Source/Floor101Editor/…/Floor101DocumentToolset`, `Tests/Unit/{AssetTests,AssetImportTests}.cpp`,
`Tests/Data/Assets/` (muestras y el script de Blender que las genera), `docs/adr/ADR-0017-imported-assets.md`.
