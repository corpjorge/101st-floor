# Informe MVP 7 — 2026-09-29

**Estado:** MVP 7 implementado y verificado. Alcance elegido por el usuario: **tipos de puertas y ventanas**. Continúa la
carpintería del MVP 6: cada hueco guarda cómo abre (abatible, de dos hojas, corredera o fija), hacia dónde y cuánto se
muestra abierto; las correderas de puerta son empotradas y su bolsillo se valida; las puertas dibujan su arco de
apertura en la vista de edición. Entorno y versiones iguales al MVP 1.

## Qué hay

| Funcionalidad | Cómo | Verificación |
|---|---|---|
| Tipos | puertas: abatible, abatible de dos hojas, corredera empotrada; ventanas: practicable, practicable de dos hojas, corredera, fija | doctest, Unreal, MCP, prueba real |
| Bisagras y sentido | extremo de las bisagras (o hacia dónde corre la hoja) y cara del muro hacia la que abre, cada uno con "Invertir" en el panel | doctest, prueba real |
| Apertura | 0–100 % (100 % = 90° en abatibles, todo el recorrido en correderas); solo cambia cómo se muestra | doctest, Unreal (hoja abierta en el mismo actor), prueba real |
| Geometría | hojas y hojas de ventana (perfiles y vidrio) que giran alrededor de su bisagra o corren por sus vías; manillas y tiradores con ellas; la corredera de puerta entra en el muro; la fija es la del MVP 6 | doctest (medidas exactas por tipo), prueba real |
| Bolsillo de las correderas | tan largo como la puerta, en el lado hacia el que corre: dentro del muro, sin pisar otros huecos ni uniones ("The sliding door needs 900 mm of wall beside it for its pocket", "…pocket runs into a wall joined to this one…"); colocar y arrastrar lo respetan | doctest, Unreal, MCP, prueba real |
| Símbolo de apertura | arco de 90° y hoja abierta de cada puerta abatible, en el suelo, estilo `Symbol` (línea oscura sin sombra); solo en la escena de edición, nunca en la de exportación | doctest, Unreal, MCP (captura), prueba real |
| Colocar | la puerta o ventana abre hacia el lado del muro al que apunta el cursor; **Espacio** pone las bisagras en la otra jamba | doctest, Unreal, prueba real |
| Panel del hueco | Tipo, Bisagras / Se abre hacia ("Lado oeste"), Abre hacia ("Cara norte"), Apertura (%); filas que no aplican ocultas | prueba real |
| Por defecto | puertas abatibles y ventanas practicables de dos hojas, cerradas | doctest |
| `project.json` esquema 6 | `operation`, `hinge`, `opensTo`, `openPercent` en cada hueco; los esquemas 1–5 se leen como puertas abatibles y ventanas fijas (su aspecto de antes) | doctest, archivo guardado desde la UI |
| MCP | `SetOpeningOperation`; los huecos informan su operación, bisagras, cara y apertura | spec del toolset, transporte MCP real |

Decisión y alternativas: ADR-0014.

## Resultados

| Validación | Resultado |
|---|---|
| Build `Floor101Editor` Win64 Development | correcto, sin avisos |
| Tests unitarios (sin Unreal) | **175 casos, 3997 aserciones, todo verde** + arquitectura OK |
| Integración Unreal (`Test.ps1 -Integration`) | **37/37** (nuevas: `Floor101.Adapter.OpeningTypes`, `Floor101.Interaction.OpeningTypes`, `SetOpeningOperation` en el toolset; Espacio en el mapa de teclas) |
| `CheckRendering.ps1 -Demo` | todo PASS; GPU 3,01 ms @ 1920×1080 |
| Flujo por MCP (`RunEditorMcp.ps1` + `Scripts/McpClient.ps1`) | abrir un esquema 6 con los 6 tipos, 2 símbolos, puerta fija y bolsillo sobre una T rechazados, corredera hacia el otro lado, deshacer, guardado en esquema 6 |

### Prueba real con ratón y teclado (`-game`)
Proyecto de prueba (`Saved/ManualTest/mvp7-types.json`): habitación de 5 × 4 m con ventana de dos hojas entreabierta
(30 %) y puerta de nogal abatible (60 %) al sur, ventana corredera de aluminio al este (50 %), corredera empotrada de
roble (50 %) y ventana fija al norte, y puerta de dos hojas (40 %) al oeste, todas abriendo hacia dentro. Se ven las
hojas giradas, la corredera medio metida en el muro, las dos hojas de la corredera de ventana solapadas y, en la vista
en planta (T), los arcos de las puertas abatibles en el suelo. Seleccionar la puerta de nogal → panel con Tipo
"Abatible", "Bisagras: Lado este", "Abre hacia: Cara norte", "Apertura: 60 %" → **Espacio**: bisagras al oeste, hoja y
arco reflejados → "Abre hacia" Invertir y apertura 100: la hoja sale al exterior → Tipo "Corredera empotrada": la hoja
entra en el muro hacia el oeste y "Abre hacia" se oculta → Invertir hacia el este: rechazado en la barra de estado
("The sliding door needs 900 mm of wall beside it for its pocket") y la puerta no cambia. Herramienta Puerta sobre el
muro este: la vista previa abre hacia el lado del muro donde está el cursor, con su arco; Espacio mueve las bisagras al
otro extremo. Guardar → esquema 6 con la puerta abatible hacia el sur al 100 %, bisagras al oeste.

Aclarado en la prueba: el arco de una puerta que abre hacia fuera no se veía en la vista en planta. El nodo existe (MCP:
actor a 1,3 cm sobre el suelo, al sur del muro) y una captura oblicua del visor lo muestra; la vista en planta es en
perspectiva y la coronación de los muros tapa el suelo exterior pegado a ellos. Documentado como limitación.

Observaciones: Esc no se puede enviar con el control del escritorio (lo cubren los tests, como en el MVP 6). La
primera captura del editor salió con el material de rejilla del motor porque aún compilaba shaders tras arrancar; la
segunda, con los acabados.

## Limitaciones conocidas
- La apertura es presentación: una hoja abierta no se valida contra muros cercanos ni se evita al mover otros huecos.
- Sin correderas de superficie, puertas plegables, oscilobatientes ni basculantes; una sola forma de manilla.
- En la vista en planta (perspectiva) no se ve el símbolo de las puertas que abren hacia fuera; en las vistas oblicuas sí.
- Los huecos de proyectos anteriores se abren como puertas abatibles cerradas y ventanas fijas; hay que elegir su tipo.

## Archivos principales
`Source/Floor101CadCore/…/{Wall,Document,ProjectFile,EditController}`, `Source/Floor101Geometry/…/CarpentryBuilder`,
`Source/Floor101SceneModel/…/{Scene.h,SceneCompiler}`, `Source/Floor101UnrealAdapter/…/Floor101SceneNodeActor`,
`Source/Floor101/…/{SFloor101Overlay,Floor101GameViewportClient}`, `Source/Floor101Editor/…/Floor101DocumentToolset`,
`Tests/Unit/OpeningTypeTests.cpp`, `docs/adr/ADR-0014-opening-types.md`.
