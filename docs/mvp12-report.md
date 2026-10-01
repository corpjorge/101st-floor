# Informe MVP 12 — 2026-09-30

**Estado:** MVP 12 implementado y verificado en 101st Floor y en Blender; la importación en Twinmotion la hace el
usuario (la ventana de Twinmotion no se puede controlar desde las herramientas de automatización de esta sesión).
Alcance pedido por el usuario: "que la casa se pueda enviar a Twinmotion"; tras comparar `.glb` y Datasmith Direct
Link, eligió la exportación `.glb`. Entorno y versiones iguales al MVP 1; Twinmotion 2026.1 HF2 y Blender 5.2 instalados.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Módulo puro `Floor101Exporters` | glTF 2.0 binario escrito contra la escena neutral: todo el proyecto sin estado de vista, metros con Y arriba, raíz → plantas → grupos (Muros, Suelos, Forjados, Puertas y ventanas, Muebles) → objetos con nombres estables e ids en `extras` | doctest, relectura con el lector glTF propio, Blender |
| Materiales | uno por acabado con su nombre de la UI: textura sintetizada en PNG (512 px), color liso sin imagen, vidrio con transparencia, texturas importadas y fotografías de la biblioteca como su archivo, materiales de los modelos con su color, rugosidad e imagen; la repetición va en los UV | doctest, Unreal (PNG decodificado), Blender |
| PNG propio | deflate con LZ77 y códigos Huffman fijos, filtro por fila, CRC y Adler; sin dependencias | doctest con un inflador independiente, decodificador de Unreal |
| Aplicación | botón "Twinmotion (.glb)" en la barra: `<proyecto>.glb` junto al proyecto, con el estado "Exportado para Twinmotion: … (N objetos, M materiales, X MB)"; consola `f101.Export.Gltf [ruta]` | Unreal, prueba real |
| MCP | `ExportGltf(ruta)` con objetos, materiales, imágenes, triángulos, bytes y avisos | spec del toolset |
| Segunda opinión | `Scripts/CheckGltf.ps1`: importa el `.glb` en Blender sin interfaz, lista lo leído y renderiza una imagen sin forjados | ejecutado |

Decisión y alternativas: ADR-0020.

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **241 casos, 11 756 aserciones, todo verde** + arquitectura OK (6 módulos puros, con `Floor101Exporters`) |
| Integración Unreal (`Test.ps1 -Integration`) | **52/52** (nuevas: `Floor101.Interaction.Export` y dos specs del toolset) |
| Exportación del salón del MVP 11 | 16 objetos, 32 materiales, 7 imágenes, 18 948 triángulos, 3,7 MB, en 0,2 s |
| Blender 5.2 (`CheckGltf.ps1`) | lo importa sin errores: 16 objetos, 32 materiales con los nombres de la UI ("Enlucido blanco", "Ladrillo encalado (foto)", "Parquet en espiga (foto)"…), 7 imágenes, límites 5,12 × 4,25 × 2,8 m con Z arriba (norte en +y, como el plano); el render muestra el ladrillo en el muro norte, el parquet en espiga a su escala, la ventana al sur, la puerta al oeste y los muebles en su sitio con sus materiales |

### Prueba real con ratón (`-game`)
El salón de la sesión MCP del MVP 11 copiado a `Saved/ManualTest/mvp12/salon.json` y abierto en el juego. El botón
"Twinmotion (.glb)" de la barra escribió `salon.glb` junto al proyecto y la barra de estado dijo "Exportado para
Twinmotion: C:/Dev/101st-floor/Saved/ManualTest/mvp12/salon.glb (16 objetos, 32 materiales, 3.7 MB)". Ese archivo, en
Blender, da lo mismo que arriba.

### Twinmotion
Twinmotion 2026.1 HF2 se abrió, pero su ventana no aparece en la lista de aplicaciones que la automatización puede
controlar, así que la importación la confirma el usuario: en Twinmotion, Importar → elegir `salon.glb` (opciones por
defecto). Después de cambiar la casa en 101st Floor: exportar otra vez y refrescar la importación eligiendo "Use scene
materials" para conservar los materiales puestos en Twinmotion.

Corregido durante la validación: dos pruebas (una unitaria y una de integración) guardaban una referencia a un
temporal; el exportador no cambió.

## Limitaciones conocidas
- No hay sincronización en vivo: cada cambio es exportar y refrescar (Datasmith Direct Link queda para un MVP posterior).
- La repetición de las texturas va en los UV: un material de Twinmotion puesto encima toma esa escala.
- Solo color, rugosidad y opacidad; sin mapas de normales, metal ni luces.
- Los materiales de los muebles de la biblioteca conservan los nombres de sus archivos (en inglés).

## Archivos principales
`Source/Floor101Exporters/` (nuevo: `Gltf`, `Png`), `Source/Floor101/…/{Floor101DocumentSubsystem,SFloor101Overlay,
Floor101Names (nuevo)}`, `Source/Floor101Editor/…/Floor101DocumentToolset`, `Tests/Unit/{PngTests,GltfExportTests}.cpp`,
`Tests/Tools/check_gltf.py`, `Scripts/CheckGltf.ps1`, `docs/exporters.md`, `docs/adr/ADR-0020-gltf-export-twinmotion.md`.
