# Informe MVP 9 — 2026-09-29

**Estado:** MVP 9 implementado y verificado. Alcance elegido por el usuario: **mobiliario**. Un catálogo de muebles
paramétricos generados desde código (camas, sofá, mesa, sillas, armario, cocina, frigorífico, inodoro, lavabo, bañera,
plato de ducha) que se colocan, mueven, giran y borran con el ratón apoyándose en los muros; salen en 3D con Lumen y en
el plano 2D, y se guardan en `project.json`. Entorno y versiones iguales al MVP 1.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Modelo | `Furniture` en el núcleo: tipo, planta, centro, giro en centésimas de grado enteras, ancho/fondo/alto, acabado; catálogo de 12 tipos con medidas por defecto y rangos; validación | doctest |
| Comandos | alta, cambio y baja deshacibles con notificación; borrar una planta borra sus muebles en el mismo paso | doctest, Unreal |
| `project.json` esquema 7 | `furniture` solo si hay muebles; se leen 1-6 | doctest (JSON exacto, ida y vuelta, errores), Unreal, MCP |
| Forma | cajas por tipo en el Geometry Engine, ajustadas a sus medidas: cama con almohadas y edredón, sofá con cojines, cocina con encimera, fregadero, grifo y placa, lavabo y bañera con cubeta, inodoro, plato de ducha con desagüe... | doctest (12 tipos a medidas mínimas, por defecto y máximas), capturas |
| Acabados | 7 nuevos solo para muebles (telas, lino, cerámica, piedra, acero, vidrio negro); roble, nogal, lacado y aluminio también; partes fijas por ranura | doctest, capturas con Lumen |
| 3D | un nodo por mueble, a la cota de su planta; seleccionado en naranja, vista previa en azul, atenuado en las plantas de abajo | doctest, Unreal |
| Plano 2D | contorno de cada mueble y sus detalles (almohadas, cubetas, fuegos...) en gris; seleccionado y vista previa tintados | doctest, SVG, PNG, prueba real |
| Amueblar (A) | paleta con los 12 tipos y sus medidas; el mueble sigue al cursor en la rejilla; los que van contra un muro se apoyan de espaldas en la cara más cercana y se paran en los muros con que chocan (esquinas); Espacio gira 90°; clic coloca | doctest, Unreal, prueba real |
| Seleccionar | clic en el mueble (más cerca que los muros); arrastrar con el mismo ajuste; panel: ancho, fondo, alto, giro en grados, "Girar 90° (Espacio)", acabado, "Eliminar (Supr)" | doctest, Unreal, prueba real |
| MCP | `GetFurnitureTypes`, `GetFurniture`, `AddFurniture`, `SetFurniture`, `SetFurnitureFinish`, `RemoveFurniture`; `bForFurniture` en los acabados | spec del toolset, sesión MCP |

Decisión y alternativas: ADR-0016.

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **205 casos, 9291 aserciones, todo verde** + arquitectura OK |
| Integración Unreal (`Test.ps1 -Integration`) | **42/42** (nuevas: `Floor101.Interaction.Furniture` y dos specs de muebles del toolset; A en el mapa de teclas) |
| En el editor real (MCP `AutomationTestToolset`) | `Interaction.Furniture`, `Interaction.PlanView` y `Interaction.KeyMap`: Success |
| `CheckRendering.ps1` | todo PASS (DX12/SM6, HWRT, Lumen HWRT, VSM, Nanite); GPU 2,90 ms @ 1920×1080 |
| MCP | piso de 9 × 4 m con salón y baño: 14 muebles con actor, acabados, `brick-red` rechazado, deshacer/rehacer, guardado con esquema 7, plano .svg (14 KB) y .png; capturas con Lumen del piso amueblado |

### Prueba real con ratón y teclado (`-game`)
Piso guardado por MCP (`Saved/ManualTest/mvp9-furniture.json`) abierto desde el cuadro Archivo → T y F: salón con
cama, armario, sofá de tela arena, mesa de nogal con cuatro sillas, cocina con fregadero y placa, frigorífico; baño con
inodoro, lavabo, bañera y plato de ducha. A → paleta "Amueblar: elige el mueble" y el sofá azul siguiendo al cursor;
"Armario" junto al muro sur: se apoya de espaldas en él y se para en la cara del tabique; junto al tabique: gira de
espaldas a él (270°); clic lo coloca ("Sin guardar"). "Silla" en suelo libre: Espacio la gira. V, clic en el armario:
"Mueble seleccionado: Armario" (120 × 60 × 220 cm, giro 270°, lacado blanco); ancho "100" → aplicado; acabado "Madera de
nogal"; Espacio → 0°. Tab: plano con todos los muebles y el armario en naranja; arrastrarlo en el plano: se apoya en el
tabique y se detiene en el muro sur. Supr lo borra y Ctrl+Z lo devuelve.

Corregido en la prueba real:
- En la vista cenital en perspectiva, arrastrar el armario hacia el muro sur lo dejaba fuera del piso, apoyado en la
  cara exterior: el desfase de agarre (medido con el cursor sobre el tabique) llevaba el centro buscado al otro lado del
  muro, y el lado se decidía por ese centro. Ahora el lado es el del cursor; un test unitario reproduce el caso (falla
  sin el arreglo) y en el juego el armario queda dentro, contra el muro sur (180°), en la esquina.

Observaciones: la primera captura del editor tras arrancar sale con el material de rejilla del motor mientras se crean
los materiales de los acabados; la siguiente ya los muestra (ya estaba anotado).

## Limitaciones conocidas
- Los muebles son cajas: sin curvas ni detalle fino.
- Sin colisiones entre muebles ni con el barrido de las puertas; el ajuste solo mira muros de la planta.
- Mover o borrar un muro no mueve ni borra los muebles apoyados en él.
- Girar con Espacio es sobre el centro y no vuelve a apoyarlo en el muro (arrastrarlo sí).
- Los rótulos de las habitaciones pueden caer encima de un mueble en el plano.

## Archivos principales
`Source/Floor101CadCore/…/{Furniture,Document,EditController,Picking,Project,ProjectFile,Finishes,Id}`,
`Source/Floor101Geometry/…/FurnitureBuilder`, `Source/Floor101SceneModel/…/{Scene,SceneCompiler}`,
`Source/Floor101Drawing/…/PlanDrawing`, `Source/Floor101/…/{SFloor101Overlay,Floor101DocumentSubsystem,
Floor101GameViewportClient}`, `Source/Floor101Editor/…/Floor101DocumentToolset`, `Tests/Unit/FurnitureTests.cpp`,
`docs/adr/ADR-0016-furniture.md`.
