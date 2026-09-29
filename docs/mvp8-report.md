# Informe MVP 8 — 2026-09-29

**Estado:** MVP 8 implementado y verificado. Alcance elegido por el usuario: **planos 2D con cotas**. La planta se ve
como un plano de arquitectura (muros cortados, carpintería, cotas y habitaciones), se edita igual que en 3D y se
exporta a SVG y PNG. Entorno y versiones iguales al MVP 1.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Módulo `Floor101Drawing` | C++ puro nuevo (CadCore, Geometry): del modelo a rellenos, trazos y rótulos en mm; paleta común; en la prueba de arquitectura | doctest, `Test.ps1` (arquitectura) |
| Muros cortados | corte a 1,20 m: tramos macizos entre los huecos cortados (puertas siempre, ventanas si el corte las cruza), ingletes y nudos incluidos | doctest |
| Carpintería | ventanas: caras, marco y vidrio (dos vidrios si es corredera); puertas: hoja a 90° y arco (una o dos hojas); corredera empotrada: hoja en el hueco y bolsillo discontinuo | doctest, prueba real, SVG, PNG |
| Cotas | a ejes, en metros con dos decimales: cadena por los huecos a 60 cm y total a 110 cm por el lado sin habitación; tabiques entre habitaciones, su longitud; cifras giradas | doctest (valores y posiciones), prueba real (cambian en vivo al arrastrar) |
| Habitaciones | "Habitación 1" y área neta ("18,24 m²") en su punto de rótulo | doctest, Unreal, prueba real |
| Vista 2D | Tab o "Plano 2D (Tab)": cámara ortográfica cenital, sin render 3D, capa Slate con el plano; seleccionar, dibujar, arrastrar y paneles como en 3D; zoom y desplazamiento; Tab o T vuelven a 3D | Unreal (cámara, encuadre, sin órbita), prueba real |
| Exportación | "Plano: SVG / PNG" junto a Guardar/Abrir, `f101.Plan.Export [ruta]`, MCP `ExportPlan`; sin marcas de edición; `<proyecto>-<planta>.svg|png` | Unreal, MCP, prueba real |
| SVG | hoja blanca a 1:100 (tamaño en mm de papel), norte arriba, discontinuos y textos | doctest, navegador |
| PNG | la misma capa pintada fuera de pantalla (`FWidgetRenderer`), 5 mm/px, 4096 px como máximo, colores exactos | Unreal en el editor real (MCP), prueba real |
| Arreglo | escribir "3.55 m" en modo Muro ya no cambia a Pintar (fallo desde el MVP 5): las letras de las unidades son texto mientras se escribe | Unreal (mapa de teclas), prueba real |

Decisión y alternativas: ADR-0015.

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **183 casos, 4085 aserciones, todo verde** + arquitectura OK con el módulo nuevo |
| Integración Unreal (`Test.ps1 -Integration`) | **39/39** (nuevas: `Floor101.Interaction.PlanView`, `ExportPlan` en el toolset; Tab y M en el mapa de teclas) |
| `Floor101.Interaction.PlanView` en el editor real (MCP) | Success, con el PNG pintado y el corte en #262626 |
| `CheckRendering.ps1 -Demo` | todo PASS; GPU 3,01 ms @ 1920×1080 |
| MCP | `CheckMcp.ps1` 6/6; `ExportPlan` .svg (17 KB) y .png (1994 × 1794); .pdf rechazado |

### Prueba real con ratón y teclado (`-game`)
Casa de prueba del MVP 7 → Tab: plano con los muros en negro, la ventana de dos hojas y la fija con sus líneas, la
corredera de aluminio con dos vidrios, la puerta de dos hojas y la de nogal con sus arcos, la corredera empotrada con su
bolsillo discontinuo, cadenas de cotas por fuera (0,70 · 1,60 · 1,00 · 0,90 · 0,80 y 5,00 al sur; cifras verticales
giradas) y "Habitación 1 · 18,24 m²". Clic en el hueco de la puerta sur: panel de la puerta; arrastrarla: muro azul
con sus asas, "Posición 230 cm · ancho 90 cm" y las cotas recalculándose en vivo (0,70 · 1,60 · 0,90 · 1,80). "Plano:
SVG" y "PNG": "Plano exportado: …\mvp7-types-planta-baja.svg|png". Documento nuevo en el plano: W, clic, "3.55 m",
Enter → muro de 3,55 acotado. Tab → de vuelta a la perspectiva 3D con el render activo.

Corregido en la prueba real:
- Al entrar en el plano el juego se cerraba: al cerrar una polilínea se añadía al array un elemento suyo
  (`Points.Add(Points[0])`), que `TArray` no permite. Ahora se copia antes. Los tests sin gráficos no pintan; por eso el
  test del plano se ejecuta también en el editor real por MCP, donde la exportación PNG pinta la capa.
- El PNG salía con los colores lavados y, tras un primer cambio, demasiado oscuro: doble corrección gamma y luego
  ninguna. Queda la corrección de Slate sobre un destino lineal; el test comprueba el color del corte.

Observaciones: tras el cierre, `CrashReportClientEditor` mantenía abierta la DLL y el siguiente build falló al enlazar;
se cerró ese proceso y se documentó en `docs/build.md`.

## Limitaciones conocidas
- Cotas solo a ejes (sin caras, interiores ni entre huecos); sin cajetín, escala gráfica ni norte dibujado.
- El plano muestra solo la planta editada; la de abajo no aparece en gris.
- Las ventanas que el corte no cruza (muy altas o muy bajas) no se dibujan.
- PNG necesita renderer (con `-nullrhi` devuelve un error claro); sin PDF ni DXF todavía.

## Archivos principales
`Source/Floor101Drawing/…/{PlanDrawing,SvgWriter}`, `Source/Floor101/…/{SFloor101PlanView,SFloor101Overlay,
Floor101DocumentSubsystem,Floor101GameViewportClient,Floor101EditingPawn}`, `Source/Floor101Editor/…/Floor101DocumentToolset`,
`Tests/Unit/PlanDrawingTests.cpp`, `Scripts/Test.ps1`, `Floor101.uproject`, `docs/drawing.md`,
`docs/adr/ADR-0015-plan-drawings.md`.
