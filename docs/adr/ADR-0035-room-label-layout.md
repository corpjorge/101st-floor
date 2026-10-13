# ADR-0035: tamaño y posición de los rótulos de habitación

Estado: aceptado (2026-10-13).

## Contexto
Cada habitación escribe en el plano su nombre (200 mm de altura de mayúsculas) y su área (150 mm) en su punto de
rótulo, o apartados de las cifras de cotas automáticas si caben (ADR-0034). En habitaciones estrechas (baños, pasillos
de ~1 m) el nombre es más ancho que el suelo, no encuentra sitio y tapa las cotas interiores. El usuario necesita
encoger esos textos y llevarlos a otro sitio, cada uno por su cuenta.

## Alternativas
- **Ajuste por habitación, cada texto movido por separado** (elegida por el usuario): resuelve el baño estrecho sin
  tocar las demás habitaciones.
- **Un tamaño global del plano**: un solo control, pero encoge también las habitaciones grandes, donde sí caben.
- **Mover nombre y área como un bloque**: más simple, pero no deja poner el área en otro lado del nombre.
- **Guardar la posición absoluta del texto**: una pared movida dejaría el texto fuera de su habitación; un
  desplazamiento desde el punto de rótulo sigue a la habitación.

## Decisión
- `Project::roomLabels`, por clave de habitación como nombres y rodapiés (ADR-0029, ADR-0030): `RoomLabel` con
  `nameHeightMm` y `areaHeightMm` (milímetros enteros de altura de mayúsculas en el plano, 50–1000; por defecto 200 y
  150) y, para cada texto, un desplazamiento opcional (`nameOffset`, `areaOffset`: `Vector2D` en mm enteros, cada
  componente dentro de ±100 m) desde el punto de rótulo de la habitación (`Room::labelPoint`) hasta el centro del texto.
  Solo hay entrada mientras algo difiere de lo automático: volver a los valores por defecto la elimina.
  `SetRoomLabelCommand` es una transacción reversible.
- **`project.json` esquema 14** añade `roomLabels` (`room`, `nameHeightMm`, `areaHeightMm` y, si se movieron,
  `nameOffset`/`areaOffset` como `[x, y]`); se siguen leyendo los esquemas 1–13.
- Dividir paredes (ADR-0025) hereda el ajuste por planta y contención del punto de rótulo, como nombres, acabados y
  rodapiés; el desplazamiento se aplica desde el punto de rótulo de la habitación nueva.
- **Drawing:** cada texto usa la altura de su habitación. Un texto movido va en su sitio y no se aparta de las cifras;
  los que siguen automáticos se colocan como en ADR-0034, con sus alturas. `PlanDrawing::roomTexts` publica la caja de
  cada texto (habitación, nombre o área, centro, ancho, alto y punto de rótulo) con las mismas medidas que se pintan, y
  el `EditController` las recibe por un proveedor inyectado, igual que las cotas (ADR-0031). El `PlanOverlay` pinta
  con tinta de selección los textos de la habitación seleccionada y la vista previa del arrastre.
- **Ratón (plano 2D, Seleccionar):** pulsar dentro de la caja de un texto selecciona su habitación y empieza a
  arrastrar ese texto; soltar crea un solo cambio deshacible, o ninguno si no se movió. Las asas de la cota
  seleccionada tienen prioridad; después los textos, por delante de las líneas y cifras de cotas y de las paredes, para
  poder sacar un rótulo de encima de una cifra. Esc suelta el arrastre. En 3D los rótulos no cambian: están en pantalla
  sobre el suelo y no tienen tamaño en el plano.
- **Panel Habitación:** "Tamaño del nombre" y "Tamaño del área" se escriben como las longitudes; "Colocación
  automática" devuelve los dos textos a su sitio automático (los tamaños se conservan).
- **MCP:** `SetRoomLabelSize`, `MoveRoomText`, `ResetRoomTextPositions`; `GetRooms` informa de alturas y
  desplazamientos.

## Consecuencias
Un rótulo movido no se reajusta si después cambian las cotas o la habitación: queda a la misma distancia de su punto
de rótulo, aunque eso lo lleve fuera del suelo; "Colocación automática" lo recupera. Las cotas manuales siguen sin
apartar rótulos. Los proyectos guardados con esquema 14 no se abren en versiones anteriores, que rechazan esquemas
más nuevos en lugar de perder los ajustes.

## Verificación
Regresiones doctest: comando, deshacer, entrada eliminada al volver a lo automático, validación de rangos, lectura y
escritura del esquema 14 (y del 13), herencia al dividir una pared, dibujo con alturas propias, texto movido sin
apartarse de las cifras y texto automático que sí se aparta, cajas publicadas iguales a las pintadas, y con el
`EditController` selección por el texto, arrastre de cada texto por separado, prioridad sobre cotas, Esc, deshacer y
tamaños escritos. `Floor101.Interaction.RoomLabels` comprueba en Unreal el arrastre en el plano, el SVG exportado y
guardar y abrir; el spec del toolset, las herramientas MCP. La comprobación funcional del ejecutable (`CheckPackage.ps1`,
`room_text_*`) arrastra el nombre de la habitación estrecha, reduce sus textos, los reabre y los deja en `plan.png`.
