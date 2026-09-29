# Informe MVP 4 — 2026-09-29

**Estado:** MVP 4 implementado y verificado. Alcance elegido por el usuario: **plantas y forjados**. El prompt maestro
solo define MVP 0 y MVP 1; este alcance da sentido al nombre del producto (varias plantas) y es la base de escaleras y
cubiertas. Entorno y versiones iguales al MVP 1.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Plantas | `Level` (altura suelo a suelo 2800, forjado 250 por defecto, nombre opcional) apiladas en `Project::levels`; cota derivada de las de abajo | doctest, MCP, prueba real |
| Muros por planta | cada muro con su `LevelId`; geometría local a su planta; uniones y habitaciones planta a planta | doctest (mismo plano en dos plantas = dos habitaciones; muros de plantas distintas no se unen) |
| Forjados | derivados por habitación en la franja alta de su planta: techo de la habitación, suelo de la de arriba; cubierta plana en la última | doctest (2550–2800, 5500–5800), Unreal (255–280 cm entre muros de 388 cm), prueba real |
| Muros a altura completa | los nuevos toman la altura de su planta; al cambiarla, los que medían lo mismo la siguen en el mismo paso (rechazo si un hueco deja de caber) | doctest, MCP, prueba real |
| Planta activa | el rayo se baja a su cota: picking, asas, snapping y dibujo solo con sus muros; la planta de abajo guía el snapping para apilar muros | doctest, Unreal (muro dibujado a 280 cm), prueba real |
| Vista | plantas de arriba ocultas, forjados de la activa ocultos, plantas de abajo en gris; "Ver todas (L)" | doctest, Unreal, prueba real |
| Cámara | sigue la cota de la planta activa; "Encuadrar" solo mira las plantas visibles | prueba real |
| UI | fila de plantas (planta activa con su cota, ▲ Subir, ▼ Bajar, + Nueva planta, Ver todas); panel de la planta activa sin selección (nombre, altura, forjado, altura libre, Eliminar planta); RePág/AvPág, L | Unreal (teclas), prueba real |
| Comandos | añadir, cambiar, borrar planta (con sus muros); esquinas por planta; todo deshacible | doctest, MCP |
| `project.json` esquema 3 | `levels` + `level` por muro; esquema 1 y 2 en una planta fija tan alta como su muro más alto | doctest, archivo guardado desde la UI y abierto de nuevo |
| MCP | `GetLevels`, `AddLevel`, `SetLevel`, `RemoveLevel`, `SetLevelView`; `AddWall`/`MoveCorner` con `LevelId` | spec del toolset (éxito y errores), transporte MCP real |
| Acceso directo | `Scripts/CreateDesktopShortcut.ps1`: "101st Floor" en el escritorio → `Run.ps1` | probado (arranca el juego con la pared de referencia) |

Decisión y alternativas: ADR-0011.

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **144 casos, 3433 aserciones, todo verde** + arquitectura OK |
| Integración Unreal (`Test.ps1 -Integration`) | **27/27** (nuevas: `Floor101.Interaction.Levels`, plantas en el toolset y sus errores) |
| `CheckRendering.ps1 -Demo` | todo PASS: DX12/SM6, RTX 5070 Ti, HWRT, Lumen GI + reflexiones (9 pases HWRT), VSM, Nanite; GPU 2,96 ms @ 1920×1080 |
| `CheckMcp.ps1` + flujo por MCP | 6/6; plantas, muros por planta, 2 habitaciones de 11,1744 m² con su planta, `SetLevel` con muros que siguen, `Undo`, `RemoveLevel` con sus muros, rechazo de la última planta, `Undo` |

### Prueba real con ratón y teclado (`-game`)
Planta baja: habitación cerrada con longitudes escritas → "9.88 m²", sin forjado encima mientras se edita. "+ Nueva
planta" → "Planta 1 (+2,80 m)": la cámara sube 2,80 m, la planta baja se ve gris con su forjado. Habitación de 2 × 3 m
en la planta 1 → "5.41 m²" (solo su etiqueta). L → edificio entero con la cubierta y ambas etiquetas; AvPág → planta
baja, la de arriba oculta. Altura de la planta baja a 300 → "Planta 1 (+3,00 m)". Guardar → esquema 3 con dos plantas;
relanzar, abrir el archivo y ver en perspectiva: dos plantas apiladas con forjados. Renombrar la planta 1 "Primera" →
barra y panel lo muestran. Altura de "Primera" a 300 → sus cuatro muros pasan a 3000 en un paso (log: 4 muros + forjado
actualizados), la planta baja no cambia.

Fallos encontrados en la prueba real y corregidos (con test):
1. **Muros que no siguen a su planta:** al subir la planta baja de 2800 a 3000, los muros se quedaban en 2800 y quedaba
   una franja abierta bajo el forjado. Ahora los muros a altura completa siguen a su planta en el mismo paso.
2. **Apilar muros sin referencia:** en la planta 1 el snapping no veía los muros de abajo y el primer muro empezó en
   x = −50 en lugar de 0. Ahora los muros de la planta de abajo guían el cursor (extremos y ejes), sin unirse.

Observaciones:
- **Clics perdidos en la esquina superior derecha** (pendiente del MVP 3): se repitió con la ventana del juego en
  otra posición, siempre en esa zona de la pantalla, y desapareció al mover la ventana. La captura indica una ventana
  de `nvidia overlay.exe`; el overlay de NVIDIA ocupa esa esquina. No es del producto.
- **Aviso de VSM** "Non-Nanite Marking Job Queue overflow": una vez por sesión, en los primeros frames tras cargar una
  escena con forjados; no se repite al editar y `CheckRendering` pasa. Es un aviso de rendimiento de las sombras
  virtuales, no un error de render. Vigilar si aparece de forma continua con edificios grandes.
- En la prueba, la ventana del juego perdió el primer plano; se recuperó desde PowerShell (Alt + `SetForegroundWindow`),
  sin tocar la configuración del sistema.

## Limitaciones conocidas
- Sin sótanos (la primera planta está en cota 0), sin forjados fuera de habitaciones cerradas (voladizos, terrazas) ni
  huecos de escalera; los forjados no se editan ni tienen propiedades propias.
- Los muros apilados son independientes (mover uno no mueve el de arriba).
- La altura libre no se valida contra los huecos: una ventana puede quedar parcialmente tapada por el forjado.
- Con "Ver todas", las etiquetas de área de las plantas de abajo se dibujan aunque las tape un forjado.
- Un muro más bajo que su planta deja una rendija bajo el forjado (coherente con el modelo; se corrige con su altura).
- Siguen las de MVP 1–3.

## Archivos principales
`Source/Floor101CadCore/…/{Level.h,Project,Document,ProjectFile,WallTool,EditController}`,
`Source/Floor101Geometry/…/Rooms` (`buildSlabMesh`), `Source/Floor101SceneModel/…/{Scene.h,SceneCompiler}`,
`Source/Floor101UnrealAdapter/…/Floor101SceneNodeActor.cpp` (estilo `Context`),
`Source/Floor101/…/{Floor101DocumentSubsystem,SFloor101Overlay,Floor101GameViewportClient,Floor101EditingPawn}`,
`Source/Floor101Editor/…/Floor101DocumentToolset`, `Tests/Unit/LevelTests.cpp`, `Scripts/CreateDesktopShortcut.ps1`,
`docs/adr/ADR-0011-levels-and-slabs.md`.
