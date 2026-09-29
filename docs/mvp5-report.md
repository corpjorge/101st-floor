# Informe MVP 5 — 2026-09-29

**Estado:** MVP 5 implementado y verificado. Alcance elegido por el usuario: **materiales y acabados**. Hasta ahora todo
se veía con el material blanco de ejemplo del motor; ahora cada cara de muro, cada suelo y cada techo tiene un acabado
de un catálogo, con texturas y rugosidad, visible con Lumen HWRT. Entorno y versiones iguales al MVP 1.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Catálogo | 11 acabados en el núcleo (enlucido blanco, pinturas arena, verde salvia y gris azulado, ladrillo visto, hormigón visto, azulejo blanco, solado gris claro, baldosa gris, tarimas de roble y nogal), con las superficies que admiten y su aspecto neutral | doctest, MCP `GetFinishes` |
| Acabados de muro | uno por cara (izquierda y derecha del eje); por defecto enlucido blanco; extremos, coronación y mochetas con el de por defecto | doctest, Unreal (material por sección), prueba real |
| Acabados de habitación | suelo y techo (cara inferior del forjado) por clave de habitación; por defecto solado gris claro y enlucido | doctest, Unreal, MCP, prueba real |
| Validación | el acabado existe y admite la superficie; un rechazo explica por qué ("The finish 'wood-oak' is not for walls") | doctest, MCP |
| Texturas | sintetizadas en C++ desde el catálogo: ladrillo a soga, baldosas, tablas con veta, moteado; sin costuras, con todos los mips, a escala real | doctest (repeticiones, mips, determinismo), Unreal (512 px, 10 mips, sRGB), prueba real |
| Material de Unreal | `M_Floor101Finish` generado desde código por un commandlet (`Scripts/GenerateAssets.ps1`); una instancia por acabado compartida | Unreal (parámetros), `CheckRendering` ("Finish material loaded") |
| Pintar (M) | paleta de acabados; clic en una cara de muro (el lado del eje donde entra el rayo) o en el suelo de una habitación | doctest, Unreal, prueba real |
| Selección de habitación | clic en el suelo en Seleccionar: suelo naranja, panel con área, suelo y techo (desplegables filtrados por superficie) | doctest, Unreal, prueba real |
| Panel del muro | caras nombradas por su orientación ("Cara norte", "Cara sur"...) con su desplegable | prueba real |
| Plantas de abajo | muestran sus acabados oscurecidos en vez de gris plano | Unreal |
| `project.json` esquema 4 | `finishes` por muro y `roomFinishes`, solo si hay | doctest, archivo guardado desde la UI y abierto de nuevo |
| MCP | `GetFinishes`, `SetWallFinish`, `SetRoomFinish`; acabados en `GetWalls` y `GetRooms` | spec del toolset, transporte MCP real |

Decisión y alternativas: ADR-0012.

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **151 casos, 3703 aserciones, todo verde** + arquitectura OK |
| Integración Unreal (`Test.ps1 -Integration`) | **31/31** (nuevas: `Floor101.Adapter.Finishes`, `Floor101.Interaction.Finishes`, acabados en el toolset y sus errores) |
| `CheckRendering.ps1 -Demo` | todo PASS, incluido el nuevo "Finish material loaded"; GPU 3,00 ms @ 1920×1080 |
| `GenerateAssets.ps1` | `Content/Floor101/Materials/M_Floor101Finish.uasset` (9 KB) generado desde código |
| `CheckMcp.ps1` + flujo por MCP | 6/6; catálogo, cara de ladrillo, rechazo de tarima en un muro, suelo y techo de una habitación, dos deshacer, guardado con acabados |

### Prueba real con ratón y teclado (`-game`)
Habitación de 3,55 × 3 m ("9.88 m²") con el suelo por defecto (solado moteado). Pintar (M) → paleta con muestras de
color → Ladrillo visto → clic en la cara interior del muro norte: ladrillo con juntas a escala real. Tarima de roble en
el suelo: tablas con juntas escalonadas. Seleccionar el suelo → panel de habitación → Techo: Tarima de roble (el
desplegable solo ofrece acabados de techo). Seleccionar el muro sur → "Cara norte" y "Cara sur" → Cara norte: Pintura
gris azulado. Guardar → esquema 4 con los acabados; relanzar y abrir en perspectiva: ladrillo dentro y fuera, verde
salvia en otra cara, todo con Lumen.

Corregido en la prueba real: en la paleta, "Hormigón visto" quedaba cortado por la lista de superficies; las
superficies pasaron al tooltip del botón. En los tests de integración apareció un aviso de Unreal (una instancia
dinámica no puede ser padre de otra) al oscurecer los acabados de las plantas de abajo: esas variantes se crean ahora
sobre el material base copiando los parámetros; sin avisos.

Observaciones: la ventana del juego volvió a perder el primer plano (se recupera desde PowerShell) y algún clic suelto
no llegó al juego, como en MVP 3–4; ninguno fue del producto (cuando el clic llega, actúa). Un clic en un elemento del
desplegable falló porque apunté fuera de la lista.

## Limitaciones conocidas
- Texturas planas: sin mapas de normales ni variación de rugosidad (el ladrillo no tiene relieve). Catálogo fijo de 11.
- Los acabados de habitación van con su clave: al dividir una habitación, el acabado queda en la parte que conserva la
  clave. Sin nombres de habitación todavía (entidad `Room`).
- El techo de la planta que se edita no se ve (sus forjados se ocultan); con "Ver todas", desde dentro de la habitación.
- Coronación, extremos, mochetas y bordes de forjado llevan siempre el acabado por defecto.
- El material se genera con un commandlet: tras cambiar el commandlet hay que regenerarlo y versionar el `.uasset`.

## Archivos principales
`Source/Floor101CadCore/…/{Finishes,Wall,Project.h,Document,ProjectFile,EditController}`,
`Source/Floor101Geometry/…/Rooms` (`makeRoomLocator`), `Source/Floor101SceneModel/…/{Scene.h,SceneCompiler,FinishTextures}`,
`Source/Floor101UnrealAdapter/…/{Floor101FinishLibrary,Floor101SceneNodeActor}`,
`Source/Floor101/…/{SFloor101Overlay,Floor101DocumentSubsystem,Floor101GameViewportClient}`,
`Source/Floor101Editor/…/{Floor101GenerateAssetsCommandlet,Floor101DocumentToolset}`,
`Content/Floor101/Materials/M_Floor101Finish.uasset`, `Scripts/GenerateAssets.ps1`, `Tests/Unit/FinishTests.cpp`,
`docs/adr/ADR-0012-finishes.md`.
