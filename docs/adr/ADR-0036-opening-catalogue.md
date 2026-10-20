# ADR-0036: Catálogo paramétrico de puertas y ventanas

Estado: aceptado por el usuario, implementado y verificado el 2026-10-19.

## Contexto

El selector ofrecía tres mecanismos de puerta y cuatro de ventana, con un solo aspecto por mecanismo.
El usuario aprobó un catálogo de 24 familias (12 puertas y 12 ventanas), con miniaturas, filtros y medidas
ajustables, integrado con los muros, las hojas móviles y los planos. Importar modelos como muebles no aporta
esa integración ni permite redimensionar sin deformar perfiles.

## Decisión

- `openingCatalogue()` en CadCore define familias con id estable, nombre, clase, mecanismo, diseño
  (liso, panelado o acristalado), divisiones por hoja y medidas sugeridas en mm. Esta primera entrega utiliza
  los mecanismos validados de ADR-0014: abatible, doble, corredera empotrada y ventana fija.
- `Opening::family` guarda el id. Vacío conserva exactamente la carpintería anterior. Una familia debe coincidir
  con la clase y el mecanismo del hueco; ids desconocidos y combinaciones incompatibles se rechazan.
- `project.json` esquema 15 escribe `family` solo si existe. Se siguen leyendo los esquemas 1–14 con familia
  vacía; los lectores anteriores rechazan el esquema nuevo en vez de perder el diseño silenciosamente.
- La geometría deriva del hueco. Las hojas y sus paneles, perfiles, vidrios y herrajes comparten la transformación
  de apertura. Las divisiones definen paños de vidrio separados; los perfiles mantienen su medida física y se
  estrechan en huecos pequeños. El catálogo no importa mallas ni introduce tipos Unreal en módulos puros.
- `setNewOpeningFamily` aplica medidas sugeridas a la herramienta; ancho, alto y antepecho se ajustan antes de
  colocar. `setOpeningFamily` conserva medidas, acabado, bisagras y cara de un hueco existente y usa un solo
  `ChangeOpeningCommand`, con validación del bolsillo y deshacer/rehacer. Cambiar a otro mecanismo borra
  la familia incompatible y vuelve al aspecto anterior de ese mecanismo.
- El inspector usa tarjetas Slate con miniaturas generadas a partir de `buildOpeningMesh`, búsqueda por nombre,
  filtro por mecanismo y estado sin resultados. Los colores y medidas proceden de `Floor101Style` (ADR-0033).
- Los planos conservan la simbología por mecanismo y añaden divisiones verticales en ventanas y una línea de
  vidrio en puertas abatibles acristaladas. Los travesaños de alzado no se dibujan como montantes en planta.
- MCP informa `Family` y ofrece `GetOpeningFamilies` y `SetOpeningFamily`, con las mismas validaciones.

## Consecuencias

Se añaden diseños reales, sin implementar mecanismos nuevos ni llamar oscilobatiente, plegable o pivotante a
una hoja abatible. Cambiar una familia existente conserva su tamaño; elegir otra para colocar restaura las
medidas sugeridas de esa familia. Los materiales usan el acabado de carpintería y el vidrio transparente existentes.
Las familias de corredera de puerta necesitan el bolsillo de ADR-0014. Los modelos externos siguen importándose
como muebles; integrarlos como familias funcionales queda para una ampliación posterior.

## Verificación

`OpeningCatalogueTests.cpp`: 24 ids y nombres únicos, 12 familias por clase, validación, geometría a varios anchos
y porcentajes, vidrios por paño, vértices finitos y triángulos no degenerados, persistencia y compatibilidad,
colocación desde la vista previa, cambio de familia deshacible y rechazo sin efectos de un bolsillo sin espacio.
Comprobación visual con `CaptureUI.ps1 -Screen doors|windows|doors-search|doors-empty|doors-filter|windows-filter`.
Resultados de ejecución en `docs/opening-catalogue.md`.
