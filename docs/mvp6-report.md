# Informe MVP 6 — 2026-09-29

**Estado:** MVP 6 implementado y verificado. Alcance elegido por el usuario: **carpintería y vidrio**. Hasta ahora las
puertas y ventanas eran huecos vacíos; ahora tienen marco, hoja con manillas o vidrio, con su acabado, no pueden pisar
las uniones con otros muros y se mueven y ensanchan con el ratón. Entorno y versiones iguales al MVP 1.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Carpintería | derivada de cada hueco en cada compilación: marco centrado en el grueso (perfil 50 mm, fondo 70 mm); puerta con hoja cerrada de 40 mm y una manilla por cara; ventana con travesaño, vidrio de 6 mm y montante si mide más de 1,30 m | doctest (mallas), Unreal (medidas del actor), prueba real |
| Acabado de carpintería | por hueco: lacado blanco (por defecto), madera de roble, madera de nogal, aluminio antracita; manillas de aluminio antracita | doctest, Unreal, MCP, prueba real |
| Vidrio | acabado translúcido del catálogo (opacidad 0,18) con el material `M_Floor101Glass`, generado desde código: se ve a través, refleja y deja pasar la luz | Unreal (material translúcido), `CheckRendering`, prueba real |
| Uniones | añadir, mover o ensanchar un hueco sobre un inglete, una T o un cruce se rechaza con el tramo ("The door runs into a wall joined to this one (1950 to 2050 mm along it)"); un hueco que llega a un inglete se recorta, en el muro y en su carpintería | doctest, Unreal (por el subsistema), MCP |
| Colocación | Puerta (P) / Ventana (O) colocan el hueco en el tramo libre más cercano al cursor, sin uniones ni otros huecos | doctest, prueba real |
| Arrastre | con un hueco seleccionado: arrastrar el hueco lo mueve; arrastrar sus asas cambia el ancho (mínimo 30 cm); rejilla de 50 mm; vista previa; un solo paso deshacible; Esc o clic derecho cancelan | doctest, Unreal, prueba real |
| Etiqueta del cursor | durante el arrastre, "Posición 265 cm · ancho 90 cm" | prueba real |
| Panel del hueco | desplegable "Carpintería" | prueba real |
| `project.json` esquema 5 | `finish` en cada hueco que lo tenga; los esquemas 1 a 4 se leen con la carpintería por defecto | doctest, archivo guardado desde la UI |
| MCP | `SetOpeningFinish`; `Finish` en los huecos; `GetFinishes` con `bForCarpentry` y `Opacity`; `AddOpening`/`SetOpening` fallan en una unión | spec del toolset, transporte MCP real |

Decisión y alternativas: ADR-0013.

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **162 casos, 3839 aserciones, todo verde** + arquitectura OK |
| Integración Unreal (`Test.ps1 -Integration`) | **34/34** (nuevas: `Floor101.Adapter.Carpentry`, `Floor101.Interaction.Carpentry`, carpintería en el toolset) |
| `CheckRendering.ps1 -Demo` | todo PASS; "Finish material loaded" exige ahora `M_Floor101Finish` y `M_Floor101Glass`; GPU 3,01 ms @ 1920×1080 |
| `GenerateAssets.ps1` | `M_Floor101Finish.uasset` (9 KB) y `M_Floor101Glass.uasset` (6 KB) generados desde código |
| `CheckMcp.ps1` + flujo por MCP | 6/6; catálogo de carpintería y vidrio, puerta rechazada en una T, acabado de roble, rechazo de ladrillo en una puerta, deshacer, guardado con esquema 5, 3 actores (2 muros y la carpintería) |

### Prueba real con ratón y teclado (`-game`)
Proyecto de prueba: habitación de 5 × 4 m con muros de 20 cm, caras exteriores de ladrillo, una ventana de 1,60 m (con
montante) y una puerta de nogal en el muro sur, y una ventana de aluminio de 1,20 m en el muro este. En perspectiva:
marcos lacados, hoja de nogal con manilla, vidrios por los que se ve el exterior; desde dentro, el marco antracita y el
vidrio de la ventana este. Seleccionar la puerta → panel con "Carpintería: Madera de nogal" → Lacado blanco → puerta
blanca con manillas antracita. Guardar → esquema 5 con `"finish": "lacquer-white"`. Puerta (P) junto a la esquina: la
vista previa aparece en el único tramo libre donde cabe (entre la ventana y la puerta), porque los tramos junto a los
ingletes son más estrechos que la puerta. Arrastrar la puerta desde su centro: vista previa azul con la etiqueta
"Posición 265 cm · ancho 90 cm"; al soltar, posición 235 cm con el mismo ancho.

Corregido en la prueba real:
- En perspectiva oblicua, pulsar en el centro de la puerta agarraba el asa del borde lejano (el rayo seguía dentro del
  muro hasta su poste) y la puerta se estrechaba a 30 cm. Ahora pulsar bien dentro del hueco siempre lo mueve; las asas
  cuentan cerca de los bordes (test nuevo con ese rayo oblicuo).
- La etiqueta del cursor mostraba la longitud del muro ("500 cm") al arrastrar un hueco; ahora da posición y ancho.
- El desplegable de carpintería decía "Tarima de nogal"; las maderas se llaman ahora "Madera de roble/nogal".
- `GetFinishes` (MCP) no decía qué acabados valen para carpintería ni cuáles son translúcidos: añadidos
  `bForCarpentry` y `Opacity`.

Observaciones: Esc no se pudo probar con el control del escritorio porque la herramienta usa esa tecla para detenerse
(no llega al juego). Lo cubren el mapa de teclas (`Floor101.Interaction.KeyMap`) y la cancelación a mitad de arrastre
por el subsistema (`Floor101.Interaction.Carpentry`); el clic derecho sin arrastrar llama a la misma cancelación. La ventana del juego
volvió a perder el primer plano algunas veces (se recupera desde PowerShell), como en MVP 3–5.

## Limitaciones conocidas
- Puertas siempre cerradas, manilla en el lado del final del hueco; sin sentido de apertura, correderas ni fijos.
- Un solo vidrio (`glass-clear`), sin refracción; la carpintería son cajas sin molduras.
- La comprobación de uniones actúa al editar el hueco: mover un muro no se rechaza por los huecos de otro. Un hueco que
  queda en un inglete se recorta; uno que queda bajo un muro en T se mantiene hasta que se edita.
- Las asas de un hueco son postes de 24 cm: en huecos muy estrechos el centro libre para moverlo es pequeño.

## Archivos principales
`Source/Floor101CadCore/…/{Finishes,Wall,Project.h,Document,ProjectFile,EditController}`,
`Source/Floor101Geometry/…/{CarpentryBuilder,WallJoins,WallGeometryBuilder,MeshBuilding.h}`,
`Source/Floor101SceneModel/…/{Scene.h,SceneCompiler,FinishTextures}`,
`Source/Floor101UnrealAdapter/…/Floor101FinishLibrary`,
`Source/Floor101/…/{SFloor101Overlay,Floor101DocumentSubsystem}`,
`Source/Floor101Editor/…/{Floor101GenerateAssetsCommandlet,Floor101DocumentToolset}`,
`Content/Floor101/Materials/M_Floor101Glass.uasset`, `Scripts/CheckRendering.ps1`, `Tests/Unit/CarpentryTests.cpp`,
`docs/adr/ADR-0013-carpentry-and-glass.md`.
