# ADR-0019: Catálogo con variantes y biblioteca integrada

Estado: aceptado y verificado (MVP 11, 2026-09-30)

## Contexto
El usuario, en el MVP 11: "cuando abro por ejemplo sillas me sale solo una silla y debería ser muchas, así con los demás
muebles y pinturas y demás assets", y "no veo la opción para pintar el suelo o colocar baldosas o piso laminado". Hasta
el MVP 10 había 12 tipos de mueble paramétricos (uno por clase), 21 acabados en una lista sin agrupar, y los suelos solo
se cambiaban seleccionando la habitación. El usuario eligió un **catálogo con variantes** (muchos modelos por categoría y
muchas pinturas y acabados) de **origen mixto**: generado aquí (scripts de Blender y texturas sintetizadas por código) más
algunas texturas CC0 descargadas con su permiso.

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Variantes de mueble | Más tipos paramétricos en el núcleo | editables por medidas, dibujados en planta | cada forma cuesta código en el Geometry Engine; decenas de tipos no escalan |
| Variantes de mueble | Assets de Unreal (`.uasset`) en `Content/` | importadores del motor | pasos en el Editor; fuera de `project.json`; Unreal dejaría de "solo representar" |
| Variantes de mueble | **Biblioteca de archivos (glTF + imagen) leída por `Floor101Assets` y añadida al proyecto como asset (ADR-0017)** | reutiliza la importación, la colocación y el snapping; el proyecto sigue autocontenido | modelos en bloque (se estiran por eje); ocupa el repositorio (LFS) |
| Origen de los modelos | Descargar modelos CC0 | realismo | estilos, escalas y calidad dispares; licencias por revisar; descargas sin permiso |
| Origen de los modelos | **Modelados por código en Blender (script sin interfaz)** | reproducible desde Git (sin pasos manuales), estilo y escala coherentes, licencia propia | menos detalle que un modelo artesanal |
| Texturas | Solo sintetizadas | sin archivos | menos realistas |
| Texturas | **Sintetizadas + fotografías CC0 (Poly Haven, con permiso)** | variedad y realismo con tamaño real | 8,5 MB en LFS |
| Uso en el proyecto | Referenciar el archivo de la biblioteca (`library:<id>`) | sin copias | el proyecto depende de la versión de la aplicación |
| Uso en el proyecto | **Copiarlo como asset que recuerda su id de biblioteca** | autocontenido; se reutiliza una vez por proyecto | una copia por proyecto |
| Pintar suelos | Herramienta aparte | explícito | otra tecla y otro modo |
| Pintar suelos | **Pintar con pestañas Paredes/Suelos; el destino lo decide el acabado** | un único modo; los suelos por fin se ven | un acabado de suelo mira a través de los muros (hacia el suelo de la habitación) |

## Decisión
- **Catálogo de acabados** (`F101/Cad/Finishes.h`): 80 acabados (antes 21) con categoría (`FinishCategory`: pinturas 26,
  baldosas 11, madera 13, piedra 9, moquetas 4, carpintería 7, tejidos 8, vidrio 2). Patrones nuevos: `Herringbone`
  (espiga: tablas en ángulo recto que avanzan en diagonal; repetición de 2n anchos) y `Checker` (ajedrezado con
  `accentColor`). Los ids existentes no cambian (`paint-sage` y `paint-slate` admiten además techos). Las texturas se
  sintetizan en el Scene Model como antes (ADR-0012).
- **Categorías de mueble** (`FurnitureCategory`: camas, sillas, sofás, mesas, almacenaje, cocina, baño, luces,
  decoración) en cada tipo paramétrico y en cada modelo de la biblioteca.
- **Biblioteca** (`Library/` en el repositorio; `F101/Assets/Library.h`): cada subcarpeta tiene un `manifest.json`
  (`"version": 1`, listas `furniture` y `textures`). Mueble: `id` (minúsculas y guiones), `name` (se muestra tal cual),
  `category`, `file` (`.glb`, `.gltf`, `.obj`), `thumbnail` (PNG), `unit`, `up`, `frontQuarterTurns`, `againstWall`,
  `credit`. Textura: `id`, `name`, `category`, `file` (PNG/JPEG), `widthMm`, `heightMm`, `roughnessPercent`, `surfaces`,
  `credit`. `loadLibrary` comprueba que existan los archivos y que los ids no se repitan; sin carpeta, biblioteca vacía.
- **Contenido**: 53 muebles modelados por `Library/Source/make_furniture.py` en Blender sin interfaz
  (`Scripts/GenerateLibrary.ps1`), exportados en GLB (metros, texturas empaquetadas) con su imagen de 256 px (Eevee), en
  las 9 categorías; 14 texturas fotográficas CC0 de Poly Haven (mapa de color 1K) con su tamaño real (laminado, tarima,
  parquet en espiga y en diagonal, mármol, baldosas, tatami, ladrillos, enlucido, piedra, friso). Binarios en Git LFS.
- **Uso**: `importLibraryFurniture`/`importLibraryTexture` = `importAsset` + nombre, ajustes y `Asset::library` (el id de
  la biblioteca); `findLibraryAsset` devuelve el asset ya añadido, así que cada elemento entra una vez por proyecto.
  **`project.json` esquema 9**: `"library"` opcional en cada asset; los esquemas 1-8 se siguen leyendo.
- **Pintar** (`EditController`): `paintTarget()` dice qué pintaría un clic: la cara del muro bajo el puntero si el acabado
  admite muros; si no, el suelo de la habitación debajo (un acabado solo de suelo mira a través de los muros); nada si no
  hay destino, con un mensaje que explica por qué.
- **Unreal**: `UFloor101DocumentSubsystem::GetLibrary()` lee `<proyecto>/Library` una vez por ejecución;
  `AddFromLibrary(id)` añade el elemento (un paso deshacible) o devuelve el ya añadido ("Añadido de la biblioteca: …").
  `UFloor101FinishLibrary::GetPicture` carga las imágenes de la biblioteca como texturas transitorias, una vez.
- **UI**: Amueblar tiene pestañas por categoría (Camas, Sillas, Sofás, Mesas, Almacenaje, Cocina, Baño, Luces, Decoración)
  y una cuadrícula de fichas con imagen y nombre: los tipos paramétricos ("medidas libres") y los modelos de la
  biblioteca, con "Elegido: …". Pintar tiene pestañas Paredes/Suelos con secciones por categoría (Pinturas; Baldosas y
  azulejos; Madera, laminado y parquet; Piedra, hormigón y ladrillo; Moquetas) que mezclan las muestras sintetizadas y
  las fotografías; la barra de estado dice qué se pintará ("suelo de la habitación (18.93 m²)"). El panel mide 330 px en
  esos modos. Los elementos de la biblioteca no aparecen entre las "importadas" ni muestran sus ajustes.
- **MCP**: `GetLibrary`, `AddFromLibrary`; `Category` en `GetFinishes` y `GetFurnitureTypes`; `Library` en los assets.

## Consecuencias
- El repositorio crece unos 16 MB en LFS (GLB 5,6 MB, imágenes 2,2 MB, fotografías 8,5 MB).
- Los modelos de la biblioteca son bloques que se estiran por eje (ADR-0017), sin partes paramétricas; los 12 tipos
  paramétricos siguen ahí.
- Actualizar la biblioteca no cambia los proyectos que ya usan un elemento: tienen su copia.
- Elegir un elemento de la biblioteca es un paso del historial (deshacerlo lo quita del proyecto).
- Un build empaquetado deberá incluir `Library/` junto al ejecutable (cuando exista `Package.ps1`).

## Verificación
- Unitarios: `LibraryTests.cpp` (manifiestos y errores; lectura de la carpeta; importación con el id de biblioteca, una
  vez; esquema 9 de ida y vuelta; la biblioteca real: ≥ 50 muebles, ≥ 3 por cada una de las 9 categorías, todos
  importables sin avisos, < 20 000 triángulos, medidas plausibles, texturas empaquetadas presentes; ≥ 14 texturas con
  autor y tamaño real), `FinishTests.cpp` (variedad por categoría y superficie, espiga y ajedrezado, destino de Pintar),
  `ProjectFileTests.cpp` y `AssetTests.cpp` (esquema 9, id de biblioteca). 230 casos.
- Unreal Automation: `Floor101.Interaction.Library` (la biblioteca real; una silla añadida una vez y colocada con sus
  materiales; un laminado fotográfico en el suelo de una habitación; esquema 9) y la spec del toolset (`GetLibrary`,
  `AddFromLibrary`, categorías, error). 49 pruebas.
- Prueba real en el juego: ver `docs/mvp11-report.md`.
