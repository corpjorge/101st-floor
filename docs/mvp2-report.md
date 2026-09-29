# Informe MVP 2 — 2026-09-28

**Estado:** MVP 2 implementado y verificado. Alcance elegido por el usuario: **uniones de muros y habitaciones**. El
prompt maestro solo define MVP 0 y MVP 1; este alcance sale de §13 (conexiones, esquinas, intersecciones, espesores
distintos), §14 (`RoomGeometryBuilder`/`FloorGeometryBuilder`) y §20 (snapping). Entorno y versiones iguales al MVP 1.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Esquinas en L (y Y, X) | extremos que coinciden se ingletan con su vecino angular; espesores distintos se encuentran en ambas caras | doctest (puntos exactos), adaptador (muro de 426 cm con inglete), Unreal (412 cm), prueba real |
| Uniones en T | un extremo dentro del cuerpo de otro muro se corta en su cara; el anfitrión no cambia | doctest (en el eje, en la cara y a media profundidad) |
| Nudos de 3 o más muros | `JointFill` (prisma) cierra el centro | doctest (cruz: 120×120 mm; T de tres extremos: triángulo) |
| Habitaciones | caras acotadas del grafo de ejes (T y cruces incluidos); suelo por dentro de los muros; área neta; clave estable | doctest (9 casos), Unreal (suelo 388×288×1 cm, 11,1744 m²), MCP, prueba real ("11.17 m²") |
| Suelos | losa de 10 mm por habitación, nodo `<clave>/floor` | doctest, Unreal |
| Etiquetas de área | proyectadas sobre el suelo en el overlay Slate; siguen al gesto en vivo | prueba real |
| Mover una esquina | `MoveJointCommand` (todos los extremos en un punto, un solo deshacer "Move corner"); arrastre del asa | doctest, Unreal (área en vivo 11,8920 m² y deshacer), MCP `MoveCorner`, prueba real |
| Snapping a muros | a menos de 150 mm del eje de un muro (T); dibujando hacia un muro, donde lo cruza el ángulo | doctest |
| Vista previa como proyecto | la vista previa sustituye/añade muros y se compila todo: ingletes y habitaciones en vivo | doctest, adaptador, prueba real |
| Vista en planta | tecla T y botón "Planta (T)": cenital con el norte arriba | Unreal (ejes de la cámara), prueba real |
| MCP | `GetRooms`, `MoveCorner` | spec del toolset (éxito y errores), transporte MCP real |

`project.json` no cambia: uniones y habitaciones se derivan de los muros en cada compilación (ADR-0009).

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **124 casos, 3126 aserciones, todo verde** + arquitectura OK |
| Integración Unreal (`Test.ps1 -Integration`) | **21/21** (nuevas: `Floor101.Interaction.Rooms`, toolset: esquinas y habitaciones, `MoveCorner` con error) |
| `CheckRendering.ps1 -Demo` | todo PASS: DX12/SM6, RTX 5070 Ti, HWRT, Lumen GI + reflexiones (9 pases HWRT), VSM, Nanite; GPU 3,14 ms @ 1920×1080 |
| `CheckMcp.ps1` + flujo por MCP | 6/6; `GetRooms` 11,1744 m² → `MoveCorner` 2 muros, 11,8920 m², misma clave → `Undo` "Move corner" |

### Prueba real con ratón y teclado (`-game`, vista en planta)
Habitación de 4×3 m con longitudes escritas (`400`, `300`, `400`, `300`) → se cierra y aparece "11.17 m²" (3,88 × 2,88 m
netos); esquinas ingleteadas. Muro en T por el centro → mientras se dibuja, la habitación ya se ve partida en dos con
sus áreas; al crearlo, dos habitaciones. Selección del muro este y arrastre de su esquina → el muro norte la sigue, las
áreas se actualizan en vivo y al soltar queda un solo paso.

Fallos encontrados en la prueba real y corregidos (con test):
1. **Recompilación a mitad de acción:** al crear un muro, el listener del documento recompilaba antes de que el
   controlador actualizara su vista, así que el muro nuevo aparecía también como vista previa (uniones y habitación mal
   en ese paso intermedio; la pantalla final era correcta). Ahora `Edit` recompila una sola vez al terminar. Confirmado
   en el log (tamaños correctos y las habitaciones en el primer resync).
2. **Cursor sobre un muro alto:** al señalar la parte de arriba de un muro, el cursor usaba el suelo que hay detrás
   (0,6 m más allá) y el muro nuevo lo atravesaba. Ahora, si el rayo toca un muro, el punto de planta es el que queda
   bajo el impacto.
3. **Asas agarradas en lo alto:** el arrastre usaba el suelo detrás del asa y la esquina saltaba. Ahora conserva el
   desfase desde donde se agarró.
4. **Dibujar en perspectiva:** con la cámara oblicua, las direcciones de pantalla no coinciden con los ejes de la planta
   y el ajuste de ángulo daba muros a 15°. Se añadió la vista en planta (T).

Las correcciones 2 y 3 tienen test unitario con rayos oblicuos, pero no se repitieron con el ratón real: tras relanzar
el juego, una ventana del servicio de Windows `GameInputSvc` retuvo el primer plano y la herramienta de automatización
no podía hacer clic en el juego (sin tocar configuración del sistema). Conviene confirmarlas a mano.

## Limitaciones conocidas
- Un grupo de muros suelto dentro de una habitación no le hace un agujero; muros colineales superpuestos no se fusionan.
- Las habitaciones no tienen nombre ni propiedades persistentes todavía (hará falta una entidad `Room`).
- El suelo usa el mismo material blanco que los muros (materiales: MVP futuro).
- La "planta" es una perspectiva casi cenital (−89°), no una vista ortográfica 2D.
- Recompilar todo en cada movimiento es O(n²) en muros; suficiente para viviendas.
- Siguen las del MVP 1 (Esc en PIE, mensajes del núcleo en inglés, sin diálogo de archivo).

## Archivos principales
`Source/Floor101Geometry/…/{WallJoins,PolygonGeometry,Rooms,WallGeometryBuilder}` + `Private/MeshBuilding.h`,
`Source/Floor101SceneModel/…/{Scene.h,SceneCompiler}`, `Source/Floor101CadCore/…/{Document,Snapping,Picking,
EditController,Geometry2D.h}`, `Source/Floor101/…/{Floor101DocumentSubsystem,SFloor101Overlay,Floor101EditingPawn,
Floor101GameViewportClient}`, `Source/Floor101Editor/…/Floor101DocumentToolset`, `Tests/Unit/{WallJoin,PolygonGeometry,
Room}Tests.cpp`, `docs/adr/ADR-0009-joins-and-rooms.md`.
