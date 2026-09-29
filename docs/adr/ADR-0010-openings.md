# ADR-0010: Puertas y ventanas como huecos del muro

Estado: aceptado y verificado (MVP 3, 2026-09-28)

## Contexto
El MVP 3 (elegido por el usuario: "Puertas y ventanas") pide colocar puertas y ventanas en los muros, verlas como
huecos reales (con Lumen HWRT la luz tiene que pasar) y editarlas con deshacer, guardado y MCP. Hay que decidir dónde
vive un hueco en el modelo, cómo se mide, cómo se valida, cómo se genera la malla y qué cambia en `project.json`.

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Dónde vive | Entidad propia en el documento con referencia a su muro | se puede listar sin recorrer muros | referencia colgante al borrar el muro; transacciones con dos entidades |
| Dónde vive | **Dentro del muro** (`Wall::openings`) | borrar/deshacer el muro arrastra sus huecos; las transacciones por entidad (ADR-0006) no cambian | para listarlas hay que recorrer los muros |
| Medida | Punto en planta | independiente del muro | se desincroniza si el muro cambia; hay que proyectar |
| Medida | **Distancia a lo largo del eje desde el inicio del muro** (mm enteros) | el hueco sigue al muro al moverlo; validación trivial | alargar el muro por su inicio desplaza el hueco respecto al mundo |
| Malla | Operación booleana (CSG) sobre la caja | general | frágil, lenta, dependencia pesada |
| Malla | **Construcción directa**: caras largas en rejilla sin las celdas del hueco + mochetas | exacta, cerrada, testeable por volumen | solo huecos rectangulares |
| Carpintería | Marcos y hojas en este MVP | más realista | multiplica el alcance (mallas, materiales) |
| Carpintería | **Solo el hueco** | alcance acotado | todavía no hay marco ni vidrio |

## Decisión
- **Modelo**: `Opening{id, kind (Door|Window), offsetMm, widthMm, heightMm, sillMm}` en `Wall::openings`, ordenadas por
  `offsetMm`. `offsetMm` va del inicio del muro al borde cercano del hueco, a lo largo del eje. Id propio (`OpeningId`,
  UUID v4) único en todo el proyecto.
- **Invariantes** (`validateWallGeometry`, así que también los cumple una vista previa): el hueco cabe en la longitud
  (`offset ≥ 0`, `offset + ancho ≤ floor(longitud)`), queda al menos 1 mm de dintel (`antepecho + alto < altura`), una
  puerta empieza en el suelo (antepecho 0), anchos y altos positivos, sin solapes. Un comando sobre el muro que deje un
  hueco fuera (acortarlo, bajarlo) se rechaza con el motivo; no se recolocan huecos en silencio.
- **Comandos**: `AddOpeningCommand` (asigna id), `ChangeOpeningCommand` (mueve/redimensiona; el tipo no cambia),
  `DeleteOpeningCommand`. Los tres pasan por la transacción de estado del muro anfitrión: un paso de deshacer cada uno.
- **Geometría** (`buildWallMesh`): las caras largas se parten en una rejilla con los bordes de los huecos y se omiten
  las celdas interiores; la cara inferior se recorta (Sutherland–Hodgman a lo largo del eje) donde hay una puerta;
  mochetas (jambas, dintel y alféizar de ventana) en una sección nueva `reveal`. La malla sigue cerrada (lo comprueban
  los tests con el volumen encerrado) y un muro sin huecos da exactamente la misma malla que antes.
- **Interacción** (`EditController`): modos Puerta (P) y Ventana (O). El hueco se centra en el puntero y su borde se
  ajusta a la rejilla de 50 mm; el muro anfitrión es el eje más cercano a menos de medio espesor + 150 mm. La vista
  previa es el muro anfitrión con el hueco (solo si es válido), es decir, el proyecto "como quedaría" (ADR-0009). Un clic
  sobre un hueco en modo Seleccionar lo selecciona; el panel edita ancho, alto, antepecho (ventanas) y posición;
  Supr lo borra. Medidas por defecto: puerta 900×2100, ventana 1200×1200 con antepecho 900.
- **`project.json` esquema 2**: cada muro puede llevar `openings` (`id`, `kind` "door"/"window", `offsetMm`,
  `widthMm`, `heightMm`, `sillMm`), escrito solo si no está vacío. Los archivos de esquema 1 se siguen leyendo (un muro
  sin `openings` no tiene huecos); se rechaza un esquema mayor que 2 (ADR-0007).
- **MCP**: `AddOpening`, `SetOpening`, `RemoveOpening` y `FFloor101WallInfo.Openings` en `GetWalls`.

## Consecuencias
- Un hueco se mueve con su muro sin lógica extra; al alargar el muro por su inicio (mover la esquina de inicio), el
  hueco mantiene su distancia a esa esquina.
- Las habitaciones y las uniones no cambian: el suelo de una habitación no entra por la puerta (el paso de puerta queda
  bajo el hueco, sin losa).
- Limitaciones conocidas: la validación mira solo la longitud del eje del muro, no sus uniones; un hueco que llegue a la
  zona de un inglete o a donde otro muro se apoya en T no se rechaza y su malla puede quedar abierta o atravesada por
  ese muro. Sin marcos, hojas ni vidrio (la luz atraviesa la ventana como un hueco). Solo huecos rectangulares.
- Una carpintería futura (marcos, vidrio, materiales) puede colgarse de `Opening` y generarse como nodos de escena
  propios (`<muro>/opening/<id>/…`) sin tocar este modelo.

## Verificación
- Unitarios (doctest): invariantes y mensajes; comandos con deshacer/rehacer; muros que rechazan cambios que dejan un
  hueco fuera; `project.json` (ida y vuelta, lectura de esquema 1, rechazos); malla con puerta (36 triángulos, áreas y
  volumen encerrado exactos) y con puerta y ventana; herramientas Puerta/Ventana, selección y propiedades del hueco.
  133 casos.
- Unreal Automation: `Floor101.Adapter.WallOpening` (36 triángulos, 7 secciones de material); mapa de teclas P/O (Ctrl+O
  sigue siendo Abrir); spec del toolset con huecos (éxito y errores). 24 pruebas.
- Prueba real con ratón y teclado y MCP: ver `docs/mvp3-report.md`.
