# ADR-0014: Tipos de puertas y ventanas

Estado: aceptado y verificado (MVP 7, 2026-09-29)

## Contexto
El MVP 7 (elegido por el usuario: "Tipos de puertas y ventanas") continúa la carpintería del MVP 6 (ADR-0013): sentido
de apertura y lado de bisagras, puertas abatibles abiertas en ángulo, correderas, ventanas fijas y de dos hojas, con
símbolo de apertura, elegibles en el panel del hueco. Hay que decidir qué se guarda de cada tipo, cómo se representa
"cuánto está abierta" sin mezclar diseño y presentación, dónde va la hoja de una corredera, cómo se dibuja el símbolo
sin que llegue a los exportadores y cómo se orienta un hueco al colocarlo.

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Qué se guarda | Un tipo cerrado por hueco ("puerta izquierda interior"...) | una sola lista | combinatoria; difícil de ampliar |
| Qué se guarda | **Operación + bisagras (extremo) + cara hacia la que abre + apertura %** | ortogonal, validable, cubre puertas y ventanas | algunos campos no aplican a todas las operaciones |
| Apertura | Estado de vista, no guardado | diseño limpio | se pierde al guardar; no sirve para presentar |
| Apertura | **Porcentaje entero guardado en el hueco** (solo cambia cómo se muestra) | persiste; útil para renders | un dato de presentación en el modelo |
| Corredera (puerta) | De superficie, delante del muro | visible | choca con muros y huecos de la cara; sin regla clara |
| Corredera (puerta) | **Empotrada: bolsillo dentro del muro, tan largo como la puerta, al lado de `hinge`** | encaja con la validación de huecos y uniones (ADR-0013) | exige muro libre al lado |
| Símbolo | En el modelo o en la escena de exportación | lo ve todo el mundo | ensucia exportaciones y Twinmotion |
| Símbolo | **Nodo de la escena de edición (overlay) con estilo `Symbol`** | ayuda de edición; los exportadores no lo ven | solo en la vista del editor |
| Orientación al colocar | Siempre la misma, invertir después | simple | dos pasos para cada puerta |
| Orientación al colocar | **Abre hacia el lado del muro al que apunta el cursor; Espacio cambia las bisagras** | un gesto, como en los CAD de arquitectura | hay que apuntar a la mitad correcta del muro |

## Decisión
- **Modelo** (`Opening`, núcleo): `operation` (`Hinged`, `DoubleHinged`, `Sliding`, `Fixed`), `hinge` (`Start`/`End`: el
  extremo de las bisagras, o hacia donde corre la hoja de una corredera), `opensTo` (`Left`/`Right`: la cara del muro hacia
  la que giran las hojas; en ventanas correderas, la vía de la hoja móvil) y `openPercent` (0–100: 100 es 90° en las
  abatibles y todo el recorrido en las correderas). Validación: una puerta no puede ser fija; apertura entre 0 y 100; una
  fija no se abre. Las puertas correderas son **empotradas**: `occupiedSpan` (hueco + bolsillo del ancho de la puerta en el
  lado `hinge`) debe caber en el muro, no pisar otros huecos ni sus bolsillos y, con el `Document`, no pisar uniones
  ("The sliding door's pocket runs into a wall joined to this one (…)"). Colocar y arrastrar ajustan todo lo que ocupa.
- **Geometría** (`buildOpeningMesh`): las hojas y las hojas de ventana (perfiles y vidrio, sección `leaf`) se construyen en
  un marco local que gira alrededor de la bisagra hacia `opensTo`; las puertas de dos hojas y las ventanas practicables de
  dos hojas llevan dos, con bisagras en ambas jambas; la puerta corredera entra en el muro (que la oculta); la ventana
  corredera tiene dos hojas solapadas 50 mm en dos vías y la de `opensTo` corre sobre la otra; la fija es la ventana del
  MVP 6 (vidrio y montante si es ancha). Manillas y tiradores giran o se mueven con su hoja.
- **Símbolo** (`buildOpeningSymbolMesh`): en puertas abatibles y de dos hojas, el arco de 90° que barre cada hoja y la hoja
  abierta a 90°, líneas planas de 20 mm a 3 mm sobre el suelo de la habitación, desde la cara hacia la que abre. La escena
  de edición (`compileScene(project, overlay)`) añade un nodo `<muro>/opening/<hueco>/swing` con estilo `Symbol` (línea
  oscura sin sombra) en las plantas no atenuadas; `compileScene(project)`, la de los exportadores, no lo tiene.
- **Interacción**: la herramienta Puerta/Ventana orienta la vista previa hacia el lado del muro donde está el cursor;
  **Espacio** cambia las bisagras de las siguientes (y, con un hueco seleccionado, las suyas). Panel del hueco: "Tipo"
  (puertas: abatible, abatible de dos hojas, corredera empotrada; ventanas: practicable, practicable de dos hojas,
  corredera, fija), "Bisagras"/"Se abre hacia" con el lado por orientación ("Lado oeste") e "Invertir", "Abre hacia"
  ("Cara norte") e "Invertir", y "Apertura" en %. Cada cambio es un `ChangeOpening` deshacible.
- **Valores por defecto**: la herramienta coloca puertas abatibles y ventanas practicables de dos hojas, cerradas.
- **`project.json` esquema 6**: `operation`, `hinge`, `opensTo` y `openPercent` en cada hueco, siempre escritos. Los
  esquemas anteriores se leen como puertas abatibles cerradas y ventanas fijas: el aspecto que tenían.
- **MCP**: `SetOpeningOperation(WallId, OpeningId, Operation, Hinge, OpensTo, OpenPercent)`; los huecos informan esos
  campos.

## Consecuencias
- `openPercent` es presentación guardada en el modelo: cambiarlo no mueve el hueco ni se valida contra otros elementos
  (una hoja abierta puede atravesar un muro cercano o un mueble futuro).
- La corredera de superficie, las puertas plegables, oscilobatientes, basculantes y el sentido "empujar/tirar" quedan
  fuera; la operación es un enumerado ampliable sin romper el esquema.
- La "vista en planta" es en perspectiva: la coronación de los muros tapa el suelo exterior junto a ellos, así que el
  símbolo de una puerta que abre hacia fuera se ve en las vistas oblicuas y no en la cenital. Una vista 2D ortográfica lo
  resolverá.
- Los huecos del proyecto de un MVP anterior se abren tal como eran (puertas abatibles, ventanas fijas).

## Verificación
- Unitarios (doctest, `OpeningTypeTests.cpp`): validación por clase y porcentaje, bolsillos (dentro del muro, sin pisar
  huecos), bolsillo contra una T en el `Document`, esquema 6 y lectura de esquemas anteriores, giro de la hoja (lado,
  bisagras, 45° y 90°), dos hojas, hoja de ventana practicable, correderas de puerta y ventana, símbolo (lado, radio,
  altura, ninguno en ventanas y correderas), símbolos solo en la escena de edición y en la planta activa, orientación al
  colocar y Espacio, edición del hueco seleccionado, colocación y arrastre de una corredera junto a una T. 175 casos.
- Unreal Automation: `Floor101.Adapter.OpeningTypes` (hoja abierta en el mismo actor, símbolo oscuro sin sombra, sin
  símbolos para exportadores), `Floor101.Interaction.OpeningTypes` (colocar con Espacio, abrir, corredera, rechazo),
  Espacio en el mapa de teclas, spec del toolset (`SetOpeningOperation` y sus errores). 37 pruebas.
- Prueba real (ratón y teclado) y MCP: ver `docs/mvp7-report.md`.
