# ADR-0015: Planos 2D con cotas

Estado: aceptado y verificado (MVP 8, 2026-09-29)

## Contexto
El MVP 8 (elegido por el usuario: "Planos 2D con cotas") pide una vista en planta tipo plano (sin la perspectiva de la
vista cenital, que tapa el suelo junto a los muros): muros rellenos, símbolos de puertas y ventanas, cotas automáticas
de muros y huecos, nombre y área de las habitaciones, exportable a PNG y SVG. La arquitectura ya preveía una "Vista 2D"
como consumidor más del modelo. Hay que decidir dónde vive el dibujo, cómo se muestra y se edita en Unreal, qué se acota
y cómo se exporta sin pasos manuales.

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Dónde se dibuja | En Unreal (mallas y textos 3D vistos desde arriba) | reutiliza la escena | texto 3D, grosores de línea y rellenos difíciles; sin SVG |
| Dónde se dibuja | **Módulo puro `Floor101Drawing`: del modelo a primitivas 2D (rellenos, trazos, rótulos) en mm** | testeable sin Unreal; un solo dibujo para pantalla, SVG y PNG | otro módulo que mantener |
| Mostrar y editar | Widget 2D con su propia entrada de ratón | independiente de la cámara | duplica la entrada; dos caminos para editar |
| Mostrar y editar | **Cámara ortográfica cenital + capa Slate que pinta el plano; render 3D desactivado** | la entrada del viewport sigue igual: rayos verticales al mismo `EditController` | depende de la proyección de la cámara (sin giro) |
| Cotas | A caras exteriores | como un plano de obra | no coinciden con las longitudes que se editan (ejes) |
| Cotas | **A ejes, por el lado sin habitación: cadena por los huecos y total** | coinciden con el modelo y con lo que el usuario escribe | un plano de obra también querría caras |
| PNG | Rasterizador propio en C++ | puro | texto y antialiasing: mucho trabajo |
| PNG | **El mismo widget pintado fuera de pantalla (`FWidgetRenderer`) y guardado con `FImageUtils`** | idéntico a la pantalla | necesita renderer (no con `-nullrhi`) |

## Decisión
- **`Floor101Drawing`** (C++20 puro, como CadCore, Geometry y SceneModel; en la prueba de arquitectura):
  `buildPlanDrawing(project, level, overlay, texts)` → `PlanDrawing` de la planta: `Fill` (polígono triangulado),
  `Stroke` (polilínea, cerrada o no, discontinua o no, ancho en mm) y `Label` (texto centrado y girado, altura en mm), cada
  uno con su tinta (`Ink`: corte, carpintería, apertura, oculto, cota, rótulo, seleccionado, vista previa, asa) y los
  límites del dibujo. `inkColor` da la paleta (sRGB) común a todos los consumidores.
  - Corte a 1,20 m: muros (y rellenos de nudos) macizos; puertas siempre cortadas; ventanas si el corte las cruza.
  - Ventanas: líneas de las dos caras, marco y vidrio (dos líneas si es corredera). Puertas abatibles y de dos hojas: cada
    hoja abierta a 90° y su arco, hacia la cara que abre. Corredera empotrada: la hoja en el hueco y el bolsillo
    discontinuo por el eje.
  - Cotas a ejes, en metros con dos decimales: por el lado sin habitación (o el izquierdo), una cadena por los bordes de
    los huecos a 60 cm de la cara y la longitud total a 110 cm; los muros entre dos habitaciones, solo su longitud.
  - Habitaciones: nombre ("Habitación 1"...) y área neta en su punto de rótulo. Textos y separador decimal los pone la
    aplicación (`DrawingTexts`).
  - `PlanOverlay` (selección, vistas previas, asas) solo en la vista de edición, como `ViewOverlay` en 3D.
  - `writeSvg`: hoja blanca a escala (1:100 por defecto; unidades de usuario en mm del plano, norte arriba), rellenos,
    trazos, discontinuos y textos. Determinista.
- **Unreal**: "Plano 2D (Tab)": el pawn pasa a una cámara ortográfica recta hacia abajo, norte arriba, a 1 km sobre el
  foco y tan ancha como la vista; el viewport deja de renderizar el mundo; `SFloor101PlanView` pinta el plano sobre toda
  la vista con la proyección de la cámara (rellenos con vértices propios, líneas antialiasing, textos). La entrada no
  cambia: los rayos del cursor bajan en vertical al `EditController`, así que seleccionar, dibujar, arrastrar y los
  paneles funcionan igual, y el plano se recalcula en cada refresco. Zoom y desplazamiento funcionan; la órbita no. T
  ("Cenital") y Tab vuelven a 3D.
- **Exportación**: botones "Plano: SVG | PNG", `f101.Plan.Export [ruta]` y MCP `ExportPlan(Path, LevelId)`; sin marcas
  de edición; por defecto junto al proyecto (`<proyecto>-<planta>.svg|png`). PNG: `SFloor101PlanView` encajado en la
  hoja y pintado con `FWidgetRenderer` (corrección gamma de Slate sobre un destino lineal, para que las tintas salgan con
  su color exacto), 5 mm por píxel, 4096 px como máximo.
- **Teclas**: Tab alterna el plano. Arreglado de paso: mientras se escribe una longitud, M y C son texto ("3.55 m" ya no
  cambia a Pintar, un fallo desde el MVP 5).

## Consecuencias
- Un dibujo, tres salidas (pantalla, SVG, PNG) idénticas; añadir un símbolo es tocar solo el módulo puro.
- Las cotas son a ejes; cotas a caras, interiores o de huecos entre sí quedarán para un modo de acotado.
- El plano solo muestra la planta editada (sin la de abajo en gris) y no tiene cajetín ni escala gráfica.
- La exportación PNG necesita renderer: los tests sin gráficos comprueban el error y el editor real, el PNG.
- La vista 3D no se renderiza en el plano: el coste de GPU baja mientras se trabaja en 2D.

## Verificación
- Unitarios (`PlanDrawingTests.cpp`): formato de cifras; muros cortados con huecos, ingletes y ventanas no cortadas;
  símbolos de ventanas, hojas y arcos, bolsillo discontinuo; cadenas y totales por el lado exterior con sus posiciones y
  ángulos; tabiques entre habitaciones; nombres y áreas; tintas de edición y asas; planta pedida y límites; SVG (hoja,
  escala, norte arriba, escapado, recuento de elementos, determinismo). 183 casos.
- Unreal Automation: `Floor101.Interaction.PlanView` (dibujo en el subsistema, selección y asas, cámara ortográfica,
  encuadre, sin órbita, exportación SVG y ruta por defecto; PNG con su color de corte donde hay renderer, error claro con
  `-nullrhi`), Tab y M en el mapa de teclas, spec de `ExportPlan`. 39 pruebas; `PlanView` también dentro del editor real
  por MCP (PNG pintado).
- Prueba real y MCP: ver `docs/mvp8-report.md`.
