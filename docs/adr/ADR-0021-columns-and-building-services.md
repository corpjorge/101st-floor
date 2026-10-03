# ADR-0021: Pilares e instalaciones (electricidad, agua, aguas negras, gas)

Estado: aceptado y verificado (MVP 13, 2026-09-30)

## Contexto
El usuario pidió los pilares "que sostienen la casa" y las líneas de las instalaciones con un color y un grosor por
red: cableado eléctrico (tubo verde delgado), agua limpia (tubo blanco delgado), aguas negras (tubo amarillo grueso) y
gas (tubo dorado delgado). Eligió, entre las opciones propuestas: pilares y las cuatro redes, trazado a mano por tramos,
una capa con rayos X para verlas y pilares rectangulares y redondos. Las instalaciones van dentro de muros, bajo el
suelo y por el techo, así que hay que poder verlas a través de la arquitectura y que bajen y suban entre alturas.

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Pilar | Muro corto | sin entidad nueva | uniones, huecos y habitaciones lo tratarían como muro; no puede ser redondo |
| Pilar | **Entidad propia (`Column`): centro, forma, ancho, fondo, giro, acabado; tan alto como su planta** | redondo o rectangular; no altera uniones ni habitaciones | no corta habitaciones ni se une a muros (se dibuja encima) |
| Trazado | Automático (de un punto de consumo a otro) | rápido | reglas de trazado, pendientes y normativa fuera de alcance |
| Trazado | **A mano por tramos: polilínea 3D (x, y, z sobre el suelo de la planta)** | lo que el usuario dibuja; sube y baja en vertical; simple de guardar y validar | sin cálculo de pendientes ni diámetros normativos |
| Ver las instalaciones | Ocultar muros y suelos | sencillo | se pierde la referencia de la casa |
| Ver las instalaciones | **Capa (tecla X) que deja la arquitectura translúcida (estilo `Ghost`)** | se ven dentro de muros y bajo el suelo sin perder la casa | translucidez y ordenación en Unreal |
| Colores | Elegidos por el usuario | flexible | se pierde el código de colores pedido |
| Colores | **Fijados por el tipo de red, como acabados del catálogo (`Surface::Services`)** | el código pedido; los exportadores y el material ya saben de acabados | no se puede pintar una tubería |

## Decisión
- **Modelo (CAD Core):** `Column` (`ColumnId`, planta, centro, `ColumnShape` Rectangle/Round, `widthMm`, `depthMm`,
  giro en centésimas de grado, acabado de muro; redondo: un diámetro y sin giro; de 100 a 3000 mm; por defecto 300 ×
  300 mm de hormigón visto). `ServiceRun` (`ServiceRunId`, planta, `ServiceKind`, puntos `ServicePoint{x, y, z}` en mm
  con z sobre el suelo de su planta, de 2 a 1000, consecutivos distintos, z de −5 m a 50 m). Cada tipo de red trae su
  diámetro y su acabado: electricidad 25 mm verde, agua 25 mm blanco, aguas negras 110 mm amarillo, gas 22 mm dorado.
  Comandos Add/Change/Delete de pilar y de tramo (deshacer/rehacer), borrar una planta borra sus pilares y tramos.
  `project.json` **esquema 10**: arrays `columns` y `services` (solo si hay).
- **Geometría:** `ServicesBuilder`: un pilar es una caja o un cilindro liso de 32 caras, del suelo a la altura de su
  planta; un tramo es un tubo liso de 12 caras por segmento, una esfera en cada codo y extremos planos.
- **Escena:** nodos `column/<uuid>` y `service/<uuid>` (`SourceKind::Column` y `Service`). Los pilares siempre se ven.
  Los tramos solo con la capa de instalaciones (`ViewOverlay::services`) o mientras se dibujan o arrastran; con la capa,
  muros, uniones, suelos, forjados y carpintería sin selección pasan a `NodeStyle::Ghost`. La escena para exportar
  (`compileScene(project)`) lleva siempre los tramos.
- **Plano 2D:** los pilares se cortan como los muros (relleno sólido); con la capa, cada tramo se dibuja con una vaina
  oscura bajo su color (el agua blanca se ve sobre papel blanco) y un anillo donde sube o baja; también en el SVG y el
  PNG exportados si la capa está activa.
- **Interacción (`EditController`):** modo **Pilar (C)**: el pilar sigue al puntero y se ajusta a las esquinas, luego a
  los ejes de los muros y luego a la rejilla (como el extremo de un muro); Espacio lo gira 90°; clic lo coloca. Modo
  **Instalación (I)**: activa la capa; el siguiente punto se toma donde el rayo corta el plano de la altura elegida y se
  ajusta como el extremo de un muro (ángulo y longitud desde el anterior); clic añade un punto, doble clic, Enter o un
  segundo clic sobre el último punto terminan (un solo comando), Retroceso quita el último y Esc descarta; cambiar la
  altura a mitad sube o baja en vertical. En Seleccionar, un pilar se selecciona antes que el muro en el que está y se
  arrastra con el mismo ajuste; con la capa, una tubería se selecciona a través de los muros (radio + 80 mm), se
  arrastra entera por la rejilla y sus asas mueven un codo (con su subida). Propiedades: forma, medidas, giro y acabado
  del pilar; tipo de la tubería. **X** muestra u oculta la capa.
- **Unreal:** estilo `Ghost` con `MI_Floor101Ghost` (el vidrio generado, gris azulado, opacidad 0,12, sin sombras); con
  la capa, el suelo propio del mapa (sus `AStaticMeshActor`) toma el mismo material para que se vean los tramos bajo la
  planta baja, y recupera los suyos al quitarla. El doble clic llega al controlador como su propio evento
  (`pointerDoubleClicked`). Barra: "Pilar (C)", "Instalación (I)" y "Rayos X (X)"; paletas con la forma y medidas del
  pilar y con el tipo (con su color y diámetro) y la altura de la tubería (Bajo suelo −10 cm, Pared 30 cm, Techo 10 cm
  bajo el forjado, o escrita). La foto conserva la capa.
- **Exportación glTF:** grupos "Pilares" e "Instalaciones" por planta; objetos "Pilar 3f2a9c1e", "Gas 0c4e81d2";
  materiales con los nombres de la UI ("Hormigón visto", "Tubo de gas dorado"…).
- **MCP:** `GetColumns`, `AddColumn`, `SetColumn`, `SetColumnFinish`, `RemoveColumn`, `GetServiceRuns`,
  `AddServiceRun`, `SetServiceRun`, `RemoveServiceRun`, `SetServicesView`; `GetFinishes` indica `bForServices`.

## Consecuencias
- Los pilares no cortan habitaciones ni se unen a muros: se dibujan encima; el área de la habitación no los descuenta.
- Las tuberías no se validan contra muros ni entre sí, ni llevan pendiente, accesorios o diámetros normativos.
- El color de cada red es fijo; un tramo cambia de tipo, no de color.
- Con la capa, el suelo del mapa también se ve a través: la casa queda sobre un fondo casi vacío.
- Archivos de esquema 10 no se abren en versiones anteriores (como en cada cambio de esquema).

## Verificación
- Unitarios (`ServicesTests.cpp`, 20 casos, y uno de exportación en `GltfExportTests.cpp`): pilares y redes válidos e
  inválidos; huella girada exacta; comandos con notificaciones, deshacer y rechazos; borrado con la planta; proyecto
  completo; `project.json` esquema 10 exacto, ida y vuelta y rechazos; mallas (caja, cilindro, tubos con codos y
  extremos); escena con y sin capa, `Ghost`, vistas previas; selección con rayos; plano con vainas, anillos y tintes;
  modos Pilar e Instalación; doble clic con el punto ajustado lejos del puntero; selección a través de muros, arrastre
  entero y por codo; plantas. 262 casos en total.
- Unreal Automation: `Floor101.Interaction.Services` (pilar ajustado a la esquina y de hormigón, capa con muro y suelo
  del mapa en `Ghost`, tramo en tres clics, plano y SVG con el tubo verde, X, guardar y abrir), teclas C/I/X en
  `Floor101.Interaction.KeyMap` y tres specs del toolset. 56 pruebas.
- Prueba real con ratón y Blender (`CheckGltf.ps1`): ver `docs/mvp13-report.md`.
