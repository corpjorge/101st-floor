# Informe MVP 13 — 2026-09-30

**Estado:** MVP 13 implementado y verificado con tests, en el juego con el ratón y en Blender.
Alcance pedido por el usuario: los pilares "que sostienen la casa" y las líneas de las instalaciones: cableado eléctrico
(tubo verde delgado), agua limpia (tubo blanco delgado), aguas negras (tubo amarillo grueso) y gas (tubo dorado
delgado). Opciones elegidas: pilares y las cuatro redes, trazado a mano por tramos, capa con rayos X, pilares
rectangulares y redondos. Entorno y versiones iguales al MVP 12.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Pilares | `Column` en el núcleo: rectangular o redondo, 10–300 cm, giro, acabado de muro (hormigón visto por defecto), tan alto como su planta; caja o cilindro de 32 caras; en el plano, cortado como un muro | doctest, Unreal, ratón |
| Instalaciones | `ServiceRun`: polilínea 3D con z sobre el suelo de su planta; electricidad verde 25 mm, agua blanca 25 mm, aguas negras amarilla 110 mm, gas dorada 22 mm; tubo liso con codos esféricos y extremos planos | doctest, Unreal, ratón, Blender |
| Capa con rayos X | X o "Rayos X (X)": tuberías visibles y muros, suelos, forjados, carpintería y el suelo del mapa translúcidos (`Ghost`); los pilares y los muebles siguen sólidos | Unreal, ratón |
| Modo Pilar (C) | el pilar sigue al cursor y se ajusta a esquinas, ejes de muro y rejilla; Espacio gira; paleta con forma, ancho, fondo o diámetro | doctest, ratón |
| Modo Instalación (I) | tipo con su color y diámetro y altura (Bajo suelo −10 cm, Pared 30 cm, Techo 10 cm bajo el forjado, o escrita); cada clic un punto ajustado como el extremo de un muro; cambiar la altura a mitad sube o baja en vertical; doble clic o Enter terminan; Retroceso quita el último; Esc descarta; la etiqueta del cursor da el tramo y la altura | doctest, ratón |
| Edición | seleccionar un pilar (antes que el muro donde está), arrastrarlo, forma, medidas, giro, acabado; seleccionar una tubería a través de los muros, arrastrarla entera o por un codo (con su subida), cambiar su tipo; Supr; deshacer | doctest, Unreal, ratón |
| Plano 2D | pilares rellenos; tuberías con vaina oscura bajo su color y un anillo donde suben; también en el SVG/PNG si la capa está activa | doctest, Unreal, ratón |
| `project.json` esquema 10 | `columns` y `services` (solo si hay) | doctest, archivo guardado en la prueba real |
| Twinmotion (`.glb`) | grupos "Pilares" e "Instalaciones", objetos "Pilar …" y "Gas …", materiales "Hormigón visto", "Tubo de agua blanco"… | doctest, Blender |
| MCP | `GetColumns`, `AddColumn`, `SetColumn`, `SetColumnFinish`, `RemoveColumn`, `GetServiceRuns`, `AddServiceRun`, `SetServiceRun`, `RemoveServiceRun`, `SetServicesView` | specs del toolset |

Decisión y alternativas: ADR-0021.

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **262 casos, 12 854 aserciones, todo verde** (21 nuevos: 20 en `ServicesTests.cpp` y uno de exportación) + arquitectura OK |
| Integración Unreal (`Test.ps1 -Integration`) | **56/56** (nuevas: `Floor101.Interaction.Services` y tres specs del toolset; teclas C, I y X en `KeyMap`) |
| `CheckRendering.ps1` | todo PASS: DX12/SM6, RTX 5070 Ti, HWRT, Lumen, VSM, Nanite, path tracing 64/64 muestras, 2,9 ms de GPU a 1920×1080 |
| Blender 5.2 (`CheckGltf.ps1`) con la casa de la prueba real | 22 objetos, 37 materiales, 8 imágenes, 19 436 triángulos; límites de −0,155 m (la tubería de aguas negras bajo el suelo) a 2,8 m; render con los pilares de hormigón en las esquinas y las tuberías de gas, agua y electricidad en su sitio |

### Prueba real con ratón (`-game`, 1800 × 1000)
El salón del MVP 12 copiado a `Saved/ManualTest/mvp13/casa.json`. Con la vista cenital: Pilar (C) y dos clics cerca de
dos esquinas → dos pilares de 30 × 30 cm guardados exactamente en las esquinas de los ejes, `(0, 4000)` y `(5000, 0)`.
Instalación (I): Agua, dos puntos a 30 cm, "Techo" (245 cm) y un punto más → la tubería sube en vertical en el segundo
punto; la etiqueta del cursor decía "235 cm · altura 245 cm". X ocultó y volvió a mostrar las tuberías, con los muros y el
suelo sólidos o translúcidos. Clic en la tubería → seleccionada (naranja, asas en sus codos, "Longitud 795 cm · 4
puntos"); arrastrar el asa del codo movió el codo con su subida (847,2 cm). El plano 2D mostró los pilares y la tubería
blanca dentro de su vaina. Guardado con Ctrl+S: esquema 10 con los dos pilares y el tramo de agua con su subida de 300 a
2450 mm. Después, en 3D: aguas negras bajo el suelo, electricidad a 30 cm y gas bajo el techo, cada una con un clic y un
doble clic; con la capa se ven las cuatro redes y los pilares; el botón "Twinmotion (.glb)" escribió `casa.glb` (22
objetos, 37 materiales, 3,9 MB).

Corregido durante la prueba real:
- **El doble clic no terminaba la tubería** (añadía un punto de más): el núcleo comparaba el punto ajustado con el último,
  pero tras el primer clic el ajuste se mide desde el punto nuevo y, si el ángulo o la rejilla lo habían separado unos
  centímetros del cursor, el segundo clic caía en otro punto. Ahora el viewport pasa el doble clic como evento propio
  (`pointerDoubleClicked`) y termina la tubería; hay un test con el puntero fuera del punto ajustado.
- **Las tuberías bajo la planta baja no se veían**: el suelo del mapa de Unreal es opaco y la capa no lo tocaba. Con la
  capa, el subsistema le pone el material fantasma y le devuelve el suyo al apagarla (test de integración con un suelo de
  prueba).
- El botón "Bajo suelo" quedaba cortado (el panel se ensancha en Instalación) y la línea de datos de la tubería no
  pasaba a la línea siguiente.

## Limitaciones conocidas
- Los pilares no cortan habitaciones (el área no los descuenta) ni se unen a los muros: se dibujan encima.
- Las tuberías no se validan contra muros ni entre sí y no llevan pendiente, accesorios ni cálculo de diámetros.
- El agua blanca sobre el suelo claro se ve poco en 3D (en el plano lleva vaina); el color de cada red es fijo.
- Con la capa, el suelo del mapa también es translúcido: la casa queda sobre un fondo casi vacío.

## Archivos principales
`Source/Floor101CadCore/{Public/F101/Cad/Column.h, Services.h, Private/Column.cpp, Services.cpp}` (nuevos), `Document`,
`Project`, `ProjectFile`, `Finishes`, `Picking`, `EditController`; `Source/Floor101Geometry/…/ServicesBuilder` (nuevo) y
`MeshBuilding.h`; `Source/Floor101SceneModel/…/SceneCompiler`; `Source/Floor101Drawing/…/PlanDrawing`;
`Source/Floor101Exporters/…/Gltf`; `Source/Floor101UnrealAdapter/…/{Floor101FinishLibrary,Floor101SceneNodeActor}`;
`Source/Floor101/…/{Floor101DocumentSubsystem,Floor101GameViewportClient,SFloor101Overlay,Floor101Names}`;
`Source/Floor101Editor/…/Floor101DocumentToolset`; `Tests/Unit/ServicesTests.cpp`;
`docs/adr/ADR-0021-columns-and-building-services.md`.
