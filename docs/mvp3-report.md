# Informe MVP 3 — 2026-09-28

**Estado:** MVP 3 implementado y verificado. Alcance elegido por el usuario: **puertas y ventanas**. El prompt maestro
solo define MVP 0 y MVP 1; este alcance es el siguiente paso natural sobre los muros y las habitaciones del MVP 2.
Entorno y versiones iguales al MVP 1.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Modelo | `Opening` (puerta o ventana) dentro de `Wall::openings`: posición a lo largo del eje desde el inicio del muro, ancho, alto, antepecho; mm enteros, id propio | doctest |
| Invariantes | cabe en el muro, deja dintel, puertas en el suelo, sin solapes; un cambio del muro que deja un hueco fuera se rechaza con el motivo | doctest, MCP (solape rechazado), prueba real (ancho que no cabe) |
| Comandos | `AddOpening`, `ChangeOpening`, `DeleteOpening`: un paso de deshacer cada uno ("Add door/window", "Change opening", "Delete opening") | doctest, MCP, prueba real |
| Malla con huecos | caras largas en rejilla sin las celdas del hueco, cara inferior recortada en las puertas, mochetas (jambas, dintel, alféizar) en la sección `reveal`; malla cerrada | doctest (36 triángulos, áreas y volumen exactos), adaptador (7 secciones), prueba real |
| Herramientas | Puerta (P) 900×2100 y Ventana (O) 1200×1200 con antepecho 900: centradas en el cursor, borde en la rejilla de 50 mm, vista previa en el muro; el modo sigue activo | doctest, prueba real |
| Selección y panel | clic en el hueco lo selecciona (asas en sus bordes); panel "Puerta/Ventana seleccionada": ancho, alto, antepecho (ventanas), posición; Supr elimina | doctest, prueba real |
| `project.json` esquema 2 | `openings` por muro (solo si hay); los archivos de esquema 1 se siguen leyendo | doctest (ida y vuelta, v1, rechazos), archivo guardado desde la UI |
| MCP | `AddOpening`, `SetOpening`, `RemoveOpening`; `GetWalls` incluye `Openings` | spec del toolset (éxito y errores), transporte MCP real |

Decisión y alternativas: ADR-0010.

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **133 casos, 3215 aserciones, todo verde** + arquitectura OK |
| Integración Unreal (`Test.ps1 -Integration`) | **24/24** (nuevas: `Floor101.Adapter.WallOpening`, teclas P/O, huecos en el toolset y sus errores) |
| `CheckRendering.ps1 -Demo` | todo PASS: DX12/SM6, RTX 5070 Ti, HWRT, Lumen GI + reflexiones (9 pases HWRT), VSM, Nanite; GPU 2,99 ms @ 1920×1080 |
| `CheckMcp.ps1` + flujo por MCP | 6/6; `OpenProject` (esquema 2) → `AddOpening` puerta → segunda puerta solapada rechazada → `SetOpening` → `RemoveOpening` → `Undo` × 2 ("Delete opening", "Change opening") → ventana restaurada |

### Prueba real con ratón y teclado (`-game`, pared de referencia)
P + clic en el muro → hueco de puerta hasta el suelo (vista previa con asas amarillas en sus bordes mientras se apunta).
O + clic → ventana con antepecho. En Seleccionar, clic en el hueco de la ventana → panel con 120 / 120 / 90 / 195 cm.
Ancho 150 → se aplica; ancho 200 → barra de estado en rojo "The window does not fit along the wall (3550 mm)" y el
muro no cambia. Supr elimina la ventana; Ctrl+Z la devuelve. Guardar en `Saved/ManualTest/mvp3-openings.json` → archivo
de esquema 2 con la ventana (1300 × 1200, antepecho 900), que luego abrió el toolset MCP.

Investigado en la prueba: algunos clics en los campos del panel (esquina superior derecha de la pantalla) parecían no
darles el foco. Con el depurador de Slate (`SlateDebugger.Event.Start`) se vio que **todo** clic que llega al juego
enfoca el campo al primer clic; los que fallaban no llegaron al proceso (ni el movimiento del cursor ni el botón: no hay
cambio de cursor ni evento de Slate), mientras que clics en la barra y en el campo de ruta llegaban siempre. Se atribuye
a la entrega de entrada de la herramienta de automatización en esa zona de la pantalla, no al producto; conviene
confirmarlo a mano.

## Limitaciones conocidas
- La validación de un hueco mira solo la longitud del eje de su muro, no las uniones: un hueco que llegue a un inglete
  o a donde otro muro se apoya en T no se rechaza y su malla puede quedar abierta o atravesada por ese muro.
- Solo el hueco: sin marcos, hojas ni vidrio; solo rectangulares. El paso de puerta queda sin losa de suelo.
- No se arrastran huecos con el ratón: se mueven escribiendo su posición en el panel.
- Mensajes del núcleo en inglés (como en MVP 1–2); siguen las demás limitaciones del MVP 2.

## Archivos principales
`Source/Floor101CadCore/…/{Id.h,Wall,Document,ProjectFile,EditController}`,
`Source/Floor101Geometry/…/{WallGeometryBuilder}` + `Private/MeshBuilding.h`,
`Source/Floor101/…/{SFloor101Overlay,Floor101GameViewportClient}`, `Source/Floor101Editor/…/Floor101DocumentToolset`,
`Tests/Unit/{Command,ProjectFile,WallGeometry,EditController}Tests.cpp`, `docs/adr/ADR-0010-openings.md`.
