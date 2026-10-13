# ADR-0034: cotas interiores automáticas de cara a cara

Estado: aceptado (2026-10-13).

## Contexto
Las cotas automáticas (ADR-0031) miden los ejes de las paredes por fuera de las habitaciones. La luz libre de una
habitación (de cara a cara) solo se podía conocer midiéndola a mano. Una cota manual fija los puntos donde se hizo
clic y no sigue a las paredes, así que es fácil leer como "la misma medida" dos cifras de cosas distintas: eje a eje
`1,07` frente a una manual `1,079`.

## Alternativas
- **Una cota por tramo recto del suelo** (elegida): bien definida en cualquier polígono (L, U, paredes que entran en la
  habitación). En un rectángulo repetiría la medida de las caras opuestas.
- **Una cruz entre caras opuestas**: limpia en rectángulos, pero no está definida en habitaciones no rectangulares.
- **Ocultar la cota de partición y medir solo por dentro**: cambia un comportamiento existente y deja huérfanas sus
  preferencias guardadas.

## Decisión
- Geometry expone `Room::sides`: el borde del suelo pared a pared (`RoomSide`: muro, cara, tramo, inicio y fin), en el
  orden del contorno antihorario, con la habitación a su izquierda. El remate libre de un muro que entra en la habitación
  no es un lado; los lados sin longitud por muros gruesos se descartan.
- Drawing une los lados consecutivos colineales y sin escalón en una cara, que se nombra con su menor
  `(muro, cara izquierda primero, tramo)`, como las claves de habitación. Cada cara da una cota automática
  `auto/<muro>/interior-left-<tramo>` (o `interior-right-`), a `InteriorOffsetMm` = 150 mm hacia dentro de la habitación.
  Los huecos no la parten: mide de cara a cara.
- No se dibuja una cota interior a lo largo de la cara izquierda de una pared entre dos habitaciones: ahí va su cota de
  partición. De dos caras opuestas con el mismo tramo (los lados de un rectángulo) solo se cota la de menor nombre. Ocultar
  esa cota no hace aparecer la opuesta.
- Mismo formato que las demás automáticas: metros con dos decimales. Separación, rótulo, ocultación, conversión a manual
  y Restaurar funcionan igual que en ADR-0031, guardados en `Project::dimensions` por su id. **Sin cambio de esquema.**
- El nombre y el área de una habitación se apartan de las cifras de cotas automáticas que los taparían: primero en
  horizontal, a la posición libre más cercana, con 75 mm de holgura y 50 mm dentro del suelo; después en vertical; si no
  hay sitio, se quedan en su punto. Solo cambia el plano (vista, SVG, PNG); `Room::labelPoint` y la selección de la
  habitación por su suelo no cambian. Cuenta cualquier cifra automática, también la de una partición que ya caía dentro
  de una habitación; las cotas manuales no desplazan rótulos.

- **Interruptor en Vista → Cotas interiores.** Es estado de vista del `EditController`
  (`setShowInteriorDimensions`, `EditView::showInteriorDimensions`), visible por defecto, como la capa de instalaciones:
  llega al dibujo como `PlanOverlay::interiorDimensions` en la vista, en el SVG/PNG exportado y en el proveedor de cotas,
  así que ocultas no se pueden seleccionar; ocultarlas suelta la que estuviera seleccionada. No es un cambio del modelo:
  no se guarda en `project.json` ni se deshace. Se recuerda en el equipo como preferencia local
  (`Floor101.Interface/ShowInteriorDimensions`, igual que la cuadrícula); la aplica el controlador del jugador al
  arrancar la aplicación (`UFloor101DocumentSubsystem::SetInteriorDimensionsShown`, sin tocar el mensaje de estado), de
  modo que las pruebas de integración y el toolset MCP del editor parten siempre de visibles.

## Consecuencias
Cada habitación muestra al menos su luz libre en ambas direcciones. Los planos existentes ganan cotas sin tocar el
proyecto; las que sobren se ocultan una a una. Los 150 mm dejan la línea por debajo del rótulo en habitaciones estrechas
(baños, pasillos de ~1 m). Las cotas interiores pueden cruzar símbolos de puertas o muebles junto a las paredes; se
separan arrastrando su línea. Un nombre más ancho que la habitación no encuentra sitio y se queda en su punto.

## Verificación
Regresiones doctest: lados de habitación (sentido, cara, tramos, muro que entra), cotas interiores del rectángulo (caras
opuestas, huecos), planta en L, muros colineales y con escalón, partición con nombre menor, baño estrecho con los rótulos
desplazados, y selección, arrastre, seguimiento de paredes, ocultación persistente, Restaurar y deshacer con el
`EditController`; con el interruptor apagado, ni dibujo, ni rótulos desplazados, ni selección, y el proyecto y su
historial intactos. `Floor101.Interaction.PlanView` comprueba la cota interior en el plano, su desaparición al ocultarlas,
el SVG exportado sin ella y su vuelta. Las pruebas de integración de Unreal y la comprobación Shipping se ejecutan en Windows con
`Test.ps1 -Integration` y `CheckPackage.ps1`.
