# Drawing (`Source/Floor101Drawing`, `f101::drawing`)

Planos 2D del modelo (ADR-0015). C++20 puro, como CadCore, Geometry y SceneModel (ADR-0002): depende de CadCore y
Geometry, nunca de Unreal. Se deriva bajo demanda y nunca se guarda.

- `buildPlanDrawing(project, level, overlay, texts)` → `PlanDrawing` de una planta, en mm de planta (X este, Y norte):
  - `Fill`: polígono antihorario con su triangulación (índices), para pintarlo con cualquier API;
  - `Stroke`: polilínea (cerrada o no, discontinua o no) con su ancho en mm (a 1:100, 10 mm imprimen 0,1 mm);
  - `Label`: texto UTF-8 centrado en un punto, girado para leerse derecho, con la altura de sus mayúsculas en mm;
  - cada elemento con su tinta (`Ink`): `Cut`, `Carpentry`, `Swing`, `Hidden`, `Dimension`, `Label`, `Furniture`,
    `ServiceCasing`, `ServiceElectrical`, `ServiceWater`, `ServiceSewage`, `ServiceGas` (`serviceInk`),
    `Selected`, `Preview`, `Handle`; `inkColor` da su color sRGB, igual para todos los consumidores;
  - `min`/`max`: los límites de todo (los rótulos, por su punto y su altura).
- Qué dibuja:
  - **Corte** a `CutHeightMm` (1,20 m): cada muro de la planta, con sus uniones resueltas (`solveWallJoins`), en tramos
    macizos entre los huecos que el corte atraviesa (puertas siempre; ventanas si su antepecho queda debajo y su dintel
    encima); los rellenos de nudos también.
  - **Ventanas**: las líneas de ambas caras, el marco y el vidrio (dos vidrios desplazados si es corredera).
  - **Puertas**: abatibles y de dos hojas, cada hoja abierta a 90° desde su bisagra en la cara hacia la que abre, y el
    arco de su canto; corredera empotrada, la hoja en el hueco y su bolsillo discontinuo por el eje.
  - **Cotas** a ejes, en metros con dos decimales (`formatMeters`): por el lado del muro sin habitación (el izquierdo si
    tienen las dos o ninguna), una cadena por los bordes de los huecos a `ChainOffsetMm` de la cara y, si tiene huecos, la
    longitud total a `OverallOffsetMm`; los muros entre dos habitaciones, solo su longitud, a 30 cm. Líneas de referencia,
    trazos a 45° en cada punto y cifras sobre la línea.
  - **Cotas interiores** (ADR-0034), de cara a cara: cada tramo recto del suelo de una habitación (`Room::sides`, unidos
    los colineales), a `InteriorOffsetMm` (15 cm) hacia dentro, sin partir por los huecos. No a lo largo de la cara con la
    cota de partición; de dos caras opuestas con el mismo tramo, solo la de menor nombre.
  - **Habitaciones** (`detectRooms`): nombre personalizado (`Project::roomNames`) o automático (`DrawingTexts::roomName`,
    "{n}" = 1, 2... por clave) y área neta, en su punto de rótulo; si una cifra de cota automática los taparía, se
    desplazan en horizontal (después en vertical) al sitio libre más cercano del suelo. SVG y PNG usan los mismos nombres.
  - **Muebles** (ADR-0016): el contorno de la huella de cada mueble de la planta y los detalles de su tipo
    (`geometry::furnitureShape`: almohadas, edredón, cojines, cubetas, fuegos, desagüe...), en trazos finos grises.
    Un modelo importado (ADR-0017) solo dibuja su huella.
  - **Pilares** (ADR-0021): cortados como los muros, relleno sólido de su huella (rectángulo o polígono de 32 lados).
  - **Instalaciones** (con `PlanOverlay::services`, la capa, de las redes de `serviceKinds`): el recorrido en planta de
    cada tramo de la planta con una vaina oscura (`ServiceCasingMm` a cada lado) bajo un trazo del color de su tipo, tan
    ancho como su tubo y nunca menos de `MinServiceCoreMm`, para que el agua blanca se vea sobre papel blanco; donde sube
    o baja sobre el mismo punto, un anillo. Los tramos en vista previa se dibujan aunque la capa esté apagada.
  - **Marcas de edición** (`PlanOverlay`, solo en la vista 2D): muro, mueble, pilar y tubería seleccionados, vistas
    previas de muros, muebles, pilares y tuberías (sustituyen por id o se añaden) y asas. La exportación del plano lleva
    las redes que se ven si su capa está activa (con todas, alguna o ninguna), nunca las marcas.
- `writeSvg(drawing, {scale, marginMm})`: documento SVG independiente, hoja blanca con el tamaño en mm de papel a la
  escala pedida, unidades de usuario en mm de planta con el norte arriba; polígonos, polilíneas (discontinuas con
  `stroke-dasharray`), textos girados y escapados. Determinista.

Consumidores: la vista 2D y la exportación PNG (`SFloor101PlanView` en el módulo `Floor101`) y la exportación SVG
(`UFloor101DocumentSubsystem::ExportPlan`).

Tests: `Tests/Unit/PlanDrawingTests.cpp`, `FurnitureTests.cpp` (muebles en el plano) y `ServicesTests.cpp` (pilares, vainas,
anillos y tintes de las instalaciones).

## Cotas editables

`PlanDrawing::dimensions` devuelve anotaciones visibles con identidad para selección.
Las manuales y las preferencias de automáticas provienen de `Project::dimensions`.
`PlanOverlay::selectedDimension` y `previewDimension` resaltan selección y gestos;
exportar omite estas marcas. SVG y PNG respetan ocultación, separación y rótulos.
Ver [guía](dimensions.md) y [ADR-0031](adr/ADR-0031-editable-dimensions.md).
