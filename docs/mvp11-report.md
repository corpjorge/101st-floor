# Informe MVP 11 — 2026-09-30

**Estado:** MVP 11 implementado y verificado. Alcance pedido por el usuario: **path tracing**; **muchas opciones por
categoría** ("cuando abro por ejemplo sillas me sale solo una silla y debería ser muchas, así con los demás muebles y
pinturas y demás assets"); y **pintar el suelo** ("no veo la opción para pintar el suelo o colocar baldosas o piso
laminado"). Decisiones del usuario: catálogo con variantes; origen mixto (modelos generados aquí con Blender, texturas
sintetizadas y 14 texturas CC0 descargadas con su permiso). Incluye además el selector del tipo de puerta o ventana antes
de colocarla (pedido entre el MVP 10 y el 11). Entorno y versiones iguales al MVP 1.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Modo Foto | R o "Foto (R)": el viewport pasa al path tracer de Unreal, la escena sin marcas de edición; panel con muestras y barra, calidad (Borrador 64, Media 256, Alta 1024), "Guardar imagen" (PNG sin UI junto al proyecto), Encuadrar, Cenital, "Volver a editar (R)"; la cámara sigue orbitando y la imagen se reinicia | Unreal, `CheckRendering.ps1`, prueba real |
| Acabados con variantes | 80 acabados (antes 21) por categoría: pinturas 26, baldosas 11, madera 13, piedra 9, moquetas 4, carpintería 7, tejidos 8, vidrio 2; patrones nuevos espiga y ajedrezado | doctest, prueba real |
| Pintar suelos | pestañas Paredes/Suelos con secciones (Pinturas; Baldosas y azulejos; Madera, laminado y parquet; Piedra, hormigón y ladrillo; Moquetas); un acabado de suelo pinta el suelo de la habitación aunque el cursor pase por un muro; la barra de estado dice qué se pintará | doctest, Unreal, prueba real |
| Biblioteca integrada | `Library/`: 53 muebles modelados por código en Blender (camas 5, sillas 8, sofás 7, mesas 7, almacenaje 6, cocina 4, baño 4, luces 4, decoración 8) con imagen, y 14 texturas fotográficas CC0 con su tamaño real (laminado, tarima, parquet en espiga y diagonal, mármol, baldosas, tatami, ladrillos, enlucido, piedra, friso) | doctest (la biblioteca real), Unreal, MCP |
| Uso de la biblioteca | al elegir un elemento se copia al proyecto como asset que recuerda su id (una vez por proyecto; el proyecto no depende de la biblioteca); `project.json` esquema 9 | doctest, Unreal, MCP |
| Amueblar | pestañas Camas, Sillas, Sofás, Mesas, Almacenaje, Cocina, Baño, Luces, Decoración; fichas con imagen y nombre (tipos paramétricos "medidas libres" + modelos de la biblioteca); "Elegido: …" | prueba real |
| Tipo de hueco antes de colocarlo | en Puerta y Ventana, "elige el tipo" (abatible, dos hojas, corredera empotrada / practicable, dos hojas, corredera, fija); cada clase recuerda el suyo | doctest, Unreal |
| MCP | `GetLibrary`, `AddFromLibrary`; `Category` en `GetFinishes` y `GetFurnitureTypes`; `Library` en los assets | spec del toolset, sesión MCP |

Decisiones y alternativas: ADR-0018 (Modo Foto) y ADR-0019 (catálogo con variantes y biblioteca).

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **230 casos, 11 499 aserciones, todo verde** + arquitectura OK (5 módulos puros) |
| Integración Unreal (`Test.ps1 -Integration`) | **49/49** (nuevas: `Floor101.Interaction.PhotoMode`, `Floor101.Interaction.Library` y dos specs de la biblioteca del toolset) |
| En el editor real (MCP `AutomationTestToolset`) | `Interaction.Library`, `Interaction.PhotoMode`, `Interaction.Assets`, `Interaction.Finishes` e `Interaction.OpeningTypes`: Success |
| `CheckRendering.ps1 -Demo` | todo PASS; **"Path tracing (photo mode)": 64/64 muestras en 1,5 s, brillo 0,62**, imagen sin barra de progreso; GPU 2,19 ms de media @ 853×640 |
| MCP | salón de 5 × 4 m con ventana y puerta en `Saved/ManualTest/mvp11-mcp`: `GetLibrary` = 67 elementos (53 muebles en 9 categorías, 14 texturas), 80 acabados; añadidos de la biblioteca parquet en espiga (3,40 m por imagen) en el suelo, ladrillo encalado en la cara interior del muro norte, sofá de tres plazas (230 × 92 × 80 cm, 1256 triángulos), mesa de centro, alfombra, butaca, lámpara de arco, monstera, estantería y aparador; pedir el sofá otra vez devolvió el mismo asset (10 assets), un id desconocido dio "The library has no item 'sofa-flying'"; esquema 9 con 10 ids de biblioteca, 19 archivos (3,3 MB) copiados junto al proyecto; plano .svg; capturas con Lumen |

### Prueba real con ratón y teclado (`-game`)
Proyecto del MVP 10 copiado a `Saved/ManualTest/mvp11`. A → pestañas por categoría; se abre en Sofás (el tipo elegido):
8 fichas con imagen. Sillas → 9 fichas (Silla paramétrica, clásica de roble, Windsor, tapizada de terciopelo, nórdica,
de oficina, taburete alto, taburete bajo, banco de roble). Clic en "Silla Windsor" → "Añadido de la biblioteca: Silla
Windsor" y se copia a `assets/<uuid>/`; clic en el suelo coloca taburetes y sofás de la biblioteca con sus materiales. M
→ Paredes: 27 pinturas, baldosas, madera y piedra con las fotografías al final de su sección; "Muro de piedra (foto)" en
una cara del muro. Suelos → baldosas (11), madera, laminado y parquet (16), piedra (7) y moquetas (5); con "Laminado de
roble (foto)" la barra dice "apunta al suelo de una habitación cerrada por muros" y un clic en el suelo lo pone (también
"Madera de roble" sintetizada). R → "Foto (path tracing)", "Lista: 256 muestras por píxel" en unos segundos, sin UI de
edición; "Guardar imagen" → `project-foto-<fecha>-<hora>.png` (1006 × 679) sin UI. El salón de la sesión MCP abierto en
el juego y fotografiado desde arriba: parquet en espiga, alfombra, mesa, sofá, lámpara, estantería, planta, butaca y
aparador con sombras suaves.

Corregido en la prueba real:
- **La fila de calidad del panel de la foto se salía del panel** ("Alta" cortado): "Calidad" va ahora en su línea y los
  tres botones se reparten el ancho.

Durante la validación la prueba unitaria de la biblioteca real detectó que leía los GLB directamente (sus texturas
empaquetadas solo existen tras importarlos): ahora importa cada mueble a un proyecto temporal, como hace la aplicación,
y lo vuelve a leer sin avisos. No era un fallo del producto.

Observaciones: el arnés de automatización de escritorio pierde a veces el primer clic de una secuencia y no puede
arrastrar con el botón derecho; el log de teclas (`-LogCmds="LogFloor101 VeryVerbose"`) confirmó que esos clics no
llegaban al juego, y los que llegan se aplican. Con la ventana a 1024 px de ancho, el panel de 330 px de Amueblar y
Pintar tapa el final de la barra ("Plano 2D"); a 1920 px no.

## Limitaciones conocidas
- La foto tiene la resolución del viewport; mover la cámara la reinicia; sin ray tracing por hardware no hay Modo Foto.
- Los modelos de la biblioteca son bloques que se estiran por eje (ADR-0017), sin partes paramétricas; en el plano, su
  huella.
- Las texturas fotográficas son solo mapa de color (sin normales ni rugosidad por píxel).
- Un build empaquetado deberá incluir `Library/` junto al ejecutable (cuando exista `Package.ps1`).

## Archivos principales
`Source/Floor101CadCore/…/{Finishes,Furniture,Asset,ProjectFile,EditController}`, `Source/Floor101SceneModel/…/{FinishTextures,
SceneCompiler}`, `Source/Floor101Assets/…/Library` (nuevo), `Source/Floor101UnrealAdapter/…/Floor101FinishLibrary`,
`Source/Floor101/…/{Floor101PathTracing (nuevo),Floor101GameViewportClient,Floor101DocumentSubsystem,SFloor101Overlay,
RenderCheck}`, `Source/Floor101Editor/…/Floor101DocumentToolset`, `Library/` (manifiestos, 53 GLB + PNG, 14 JPG,
`Source/make_furniture.py`), `Scripts/{GenerateLibrary,CheckRendering}.ps1`, `Tests/Unit/{LibraryTests,FinishTests,
OpeningTypeTests,AssetTests,ProjectFileTests}.cpp`, `docs/adr/ADR-0018-photo-mode-path-tracing.md`,
`docs/adr/ADR-0019-catalogue-and-library.md`.
