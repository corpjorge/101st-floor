# ADR-0013: Carpintería y vidrio de puertas y ventanas

Estado: aceptado y verificado (MVP 6, 2026-09-29)

## Contexto
El MVP 6 (elegido por el usuario: "Carpintería y vidrio") pide que las puertas y ventanas dejen de ser solo huecos
(ADR-0010): marcos y hojas de puerta, ventanas con vidrio por el que entra y se refleja la luz de Lumen, huecos
validados contra las uniones de muros (ingletes y T) y huecos que se arrastran con el ratón. Hay que decidir si la
carpintería se guarda o se deriva, qué se guarda de ella, cómo se representa el vidrio en Unreal sin pasos manuales, qué
pasa cuando un hueco invade la unión con otro muro y cómo se edita un hueco con el ratón sin lógica en Unreal (ADR-0008).

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Carpintería | Entidad guardada (marco, hoja) por hueco | editable pieza a pieza | duplicado del hueco; se desincroniza al moverlo |
| Carpintería | **Derivada del hueco en cada compilación** (como uniones y forjados) | sin estado nuevo; sigue al hueco | perfiles fijos hasta que haya tipos de carpintería |
| Qué se guarda | Material libre del marco | flexible | proyectos inconsistentes (ADR-0012) |
| Qué se guarda | **Id de acabado del catálogo por hueco** (`Opening::finish`) | validable, mismo sistema que muros y suelos | pocas opciones hasta ampliar el catálogo |
| Vidrio | Material opaco claro | barato | no deja pasar la luz |
| Vidrio | **Material translúcido generado por commandlet** (`M_Floor101Glass`) | la luz entra y se refleja; reproducible | coste de translucidez; sin refracción |
| Hueco en una unión | Recortarlo o desplazarlo en silencio | nunca falla | el usuario no sabe dónde queda su puerta |
| Hueco en una unión | **Rechazar al añadir o cambiar el hueco, con el tramo en mm** | explícito, deshacer intacto | hay que colocar mejor (la herramienta ya lo ajusta) |
| Arrastre | En Unreal (gizmo, actores) | rápido de hacer | lógica fuera del núcleo; no testeable sin Unreal |
| Arrastre | **En `EditController`** (asas de borde y cuerpo del hueco) | puro, testeable, igual que las esquinas | picking propio (postes) |

## Decisión
- **Carpintería derivada** (`F101/Geometry/CarpentryBuilder.h`, `buildOpeningMesh`): en el hueco recortado como el del
  muro, un marco centrado en el grueso (perfil de 50 mm visto, 70 mm de fondo o el grueso del muro si es menor): jambas y
  cabecero; las puertas, una hoja cerrada de 40 mm con 3 mm de holgura y 10 mm sobre el suelo y una manilla por cara; las
  ventanas, un travesaño inferior, un vidrio de 6 mm y un montante central si miden más de 1300 mm. Los perfiles se
  estrechan en huecos pequeños. Secciones `frame`, `leaf`, `hardware`, `glass`. Nada de esto se guarda.
- **Acabados** (catálogo de ADR-0012, ahora 14): superficies nuevas `Carpentry` y `Glazing` y `FinishInfo::opacity`.
  Nuevos `lacquer-white` (carpintería por defecto), `aluminium-anthracite` (carpintería; también las manillas) y
  `glass-clear` (vidrio, opacidad 0,18); `wood-oak` y `wood-walnut` sirven también para carpintería. `Opening::finish`
  (vacío = por defecto) se valida contra `Carpentry`; el vidrio es siempre `glass-clear`.
- **Escena**: un nodo por hueco, `<muro>/opening/<hueco>` (`SourceKind::Opening`), tras los forjados, en la planta y con
  el estilo de su muro; marco y hoja con el acabado del hueco, manillas con `aluminium-anthracite`, vidrio con
  `glass-clear`. `FinishLook::opacity` lleva la opacidad al consumidor.
- **Uniones**: el Geometry Engine calcula, para un muro, los tramos de su eje ocupados por otros muros
  (`wallObstructions`: extremos hasta donde existen ambas caras en un inglete, huella de los muros en T o en cruce,
  fusionados y redondeados hacia fuera a mm enteros) y el `Document` los recibe inyectados
  (`setWallObstructions(makeWallObstructions())`, sin depender del Geometry Engine). `AddOpening` y un
  `ChangeOpening` que mueve o ensancha el hueco se rechazan si lo pisan: "The door runs into a wall joined to this one
  (1950 to 2050 mm along it)". Cambiar solo el acabado nunca se rechaza. El hueco en la malla del muro y su carpintería
  se recortan a donde existen ambas caras.
- **Interacción** (`EditController`): la herramienta Puerta/Ventana coloca el hueco en el tramo libre (sin uniones ni
  otros huecos) más cercano al puntero, con el borde en la rejilla. Con un hueco seleccionado, sus asas (postes en los
  bordes) cambian el ancho dentro de su tramo libre, con un mínimo de 300 mm, y el hueco se arrastra para moverlo; pulsar
  bien dentro del hueco (a más de media asa de cada borde) siempre lo mueve, aunque el rayo, oblicuo, siga hasta el poste
  del borde lejano. La vista previa muestra el muro con el hueco movido; al soltar se aplica un solo `ChangeOpening`
  (nada si no se movió); Esc o clic derecho cancelan. La etiqueta del cursor dice "Posición … · ancho …".
- **Unreal**: el commandlet `Floor101GenerateAssets` genera también `M_Floor101Glass` (translúcido, iluminado por
  píxel para que refleje; `Tint`, `Opacity`, `Roughness`; no proyecta sombra, así que el sol y Lumen entran por las
  ventanas). `UFloor101FinishLibrary` usa ese material para los acabados con opacidad < 1 y el de acabados para el
  resto. Panel del hueco: desplegable "Carpintería". `CheckRendering` exige ambos materiales.
- **`project.json` esquema 5**: `finish` opcional en cada hueco, escrito solo si lo tiene; los esquemas 1 a 4 se leen
  con la carpintería por defecto.
- **MCP**: `SetOpeningFinish`; los huecos informan su `Finish`; `GetFinishes` informa `bForCarpentry` y `Opacity`;
  `AddOpening` y `SetOpening` fallan en una unión con el mismo mensaje.

## Consecuencias
- Las puertas se ven siempre cerradas, con la manilla en el lado del final del hueco; no hay sentido de apertura,
  batientes ni perfiles moldurados. Un tipo de carpintería (abatible, corredera, fija) podrá añadirse al hueco sin romper
  el esquema 5. (MVP 7: hecho en ADR-0014, esquema 6.)
- El vidrio no refracta ni se elige por hueco (un solo id de vidrio). La translucidez tiene coste, acotado a los vidrios.
- La comprobación de uniones actúa al editar el hueco: editar un muro no se rechaza por los huecos de otros. Un hueco
  que acaba en un inglete se recorta; uno que acaba bajo un muro en T se mantiene hasta que se edita.
- Cada hueco suma un actor; los cambios de acabado actualizan ese actor sin tocar el muro (hash por nodo).

## Verificación
- Unitarios (doctest): catálogo de carpintería y vidrio, esquema 5, mallas de puerta y ventana (marco, hoja, manillas,
  montante, vidrio), nodos de escena, tramos de unión (inglete, T, cruce), rechazo en el `Document`, recorte en un
  inglete, colocación en el tramo libre, arrastre y cambio de ancho, pulsación oblicua dentro del hueco. 162 casos.
- Unreal Automation: `Floor101.Adapter.Carpentry` (vidrio translúcido, marco lacado, medidas del actor),
  `Floor101.Interaction.Carpentry` (rechazo en T por el subsistema, arrastre a 3850 mm con el mismo actor, Esc a mitad de
  arrastre), spec del toolset (`SetOpeningFinish`, error en unión, catálogo). 34 pruebas.
- Prueba real (ratón y teclado) y MCP: ver `docs/mvp6-report.md`.
