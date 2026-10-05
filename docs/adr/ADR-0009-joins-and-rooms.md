# ADR-0009: Uniones de muros y habitaciones derivadas de la geometría

Estado: aceptado y verificado (MVP 2, 2026-09-28)

## Contexto
El MVP 2 (elegido por el usuario: "Uniones y habitaciones") pide esquinas limpias donde se tocan los muros (L, T, X,
espesores distintos; prompt maestro §13) y habitaciones cerradas con suelo y área. Hasta el MVP 1 cada muro era una caja
independiente: en una esquina se solapaban y dejaban huecos o picos. Hay que decidir dónde vive esa topología, si se
guarda en `project.json` y cómo se presenta mientras se edita.

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Uniones | Conexiones guardadas en el modelo (grafo explícito muro-nodo) | editar un nodo mueve todo; habilita restricciones | enlaces que se desincronizan; migración de `project.json`; cada comando debe mantener el grafo |
| Uniones | **Derivadas en cada compilación**: extremos que coinciden (mm exactos) o que tocan el cuerpo de otro muro | siempre coherentes con los muros; sin cambio de formato; testeables | mover una esquina necesita un comando que mueva a la vez todos los extremos que la comparten |
| Habitaciones | Entidad `Room` guardada | puede llevar nombre y propiedades | se desincroniza al editar muros; hay que reasociarla |
| Habitaciones | **Derivadas**: caras acotadas del grafo plano de ejes | siempre correctas; área siempre exacta; sin estado | sin nombre persistente todavía |
| Vista durante un gesto | Muro original oculto + vista previa suelta | simple | la vista previa no se une a nada; las habitaciones no reaccionan |
| Vista durante un gesto | **Compilar el proyecto "como quedaría"** (vista previa que sustituye o añade muros) | uniones y habitaciones en vivo; un solo camino de compilación | recompila todo en cada movimiento (aceptable a esta escala) |

## Decisión
- **Uniones** (`f101::geometry::solveWallJoins`, puro): los extremos que comparten un punto se ingletan con su vecino
  angular (cada lado izquierdo se corta con el lado derecho del siguiente muro en sentido antihorario). Funciona para L,
  Y y X y para espesores distintos. Los colineales se encuentran a escuadra. Con tres o más extremos se genera un
  `JointFill` (prisma) que cierra el centro. Un extremo dentro del cuerpo de otro muro, lejos de sus extremos, es una T:
  se corta en la cara de ese muro. Límite de inglete ×4 del semiespesor (esquinas muy agudas → escuadra). Un muro cuyos
  cortes se cruzarían vuelve a su rectángulo. El resultado es un `WallFootprint` por muro; `buildWallMesh(wall,
  footprint)` mantiene las 6 secciones y, para un muro suelto, la misma malla que antes.
- **Habitaciones** (`detectRooms`, puro): grafo plano de ejes partidos en las T (el extremo se une al eje del muro
  anfitrión) y en los cruces; las caras acotadas (antihorarias) son habitaciones. El suelo es la cara desplazada hacia
  dentro por el semiespesor de cada muro; los muros que entran en la habitación se recortan del suelo. Área neta, punto
  de etiqueta interior, muros que la rodean y una clave determinista (`<muro de id menor>/room-<lado>-<tramo>`), estable
  mientras ese muro la delimite. Suelo: losa de 10 mm (`buildFloorMesh`).
  Desde 0.1.4 se inicializan todos los anfitriones antes de acumular sus T y se
  recorren una sola vez los segmentos de eje coincidentes, con muro de menor UUID
  como referencia determinista. Los solapamientos colineales no invalidan una
  habitación cerrada; el modelo conserva los muros originales y los huecos reales
  no se cierran automáticamente.
- **Nada se guarda**: `project.json` no cambia (esquema 1). Las uniones y habitaciones se recalculan en cada
  `compileScene`, que emite muros, `JointFill` (`<muro>/joint-start|end`) y suelos (`<clave>/floor`), y
  `Scene::rooms`.
- **Esquinas**: `MoveJointCommand{from, to}` mueve todos los extremos en `from` en una transacción ("Move corner").
  Arrastrar el asa de un muro seleccionado mueve la esquina entera; el snapping ignora los muros que se mueven.
- **Vista previa = proyecto como quedaría**: `ViewOverlay::previewWalls` sustituye muros por id (arrastre) o añade
  muros nuevos (el que se dibuja). Desaparece el estilo `Hidden`. Las etiquetas de área siguen al gesto en vivo.
- **Snapping a muros** (§20): un punto cerca del eje de un muro (150 mm) se ajusta a él (T); al dibujar hacia un muro,
  el rayo con el ángulo ajustado se detiene en su eje. Prioridad: extremo > muro > ángulo/rejilla.
- **Cursor en planta**: si el rayo del ratón toca un muro, el punto de planta es el que queda bajo el impacto (no el
  suelo que hay detrás), y un arrastre conserva el desfase desde donde se agarró el asa. Vista en planta con la tecla T.

## Consecuencias
- Los muros vecinos se regeneran cuando uno cambia (su inglete depende de él); el reconciliador solo toca lo que cambió.
- Recompilar todo en cada movimiento es O(n²) en muros (T, cruces). Suficiente para viviendas; con proyectos grandes
  habrá que indexar o recompilar por zonas.
- Limitaciones conocidas: un grupo de muros suelto dentro de una habitación no le hace un agujero; muros solapados
  (colineales superpuestos) no se fusionan en el modelo. Los nombres persistentes
  se implementaron posteriormente en [ADR-0029](0029-room-names.md).
- Puertas y ventanas (huecos) se apoyarán en estas huellas y en los tramos de muro.

## Verificación
- Unitarios (doctest): uniones L/T/X/Y, espesores distintos, colineales, límite de inglete, reversión, malla cerrada con
  normales hacia fuera; polígonos (triangulación cóncava, prismas); habitaciones (rectángulo, sentido de dibujo, abierto,
  T en eje/cara/cuerpo, cruces, muro que entra, espesores mixtos, forma de U, claves estables); escena (uniones en la
  vista previa, sustitución por id, fills y suelos); comando de esquina; arrastre de esquina; snapping a muros; cursor
  sobre muros; desfase de agarre. 124 casos.
- Unreal Automation: estilos y vista previa en el adaptador; `Floor101.Interaction.Rooms` (suelo 388×288×1 cm, área
  11,1744 m², arrastre de esquina con área en vivo 11,8920 m², deshacer); toolset `GetRooms`/`MoveCorner`. 21 pruebas.
- Prueba real (ratón y teclado, vista en planta): habitación de 4×3 m con longitudes escritas → "11.17 m²"; muro en T →
  dos habitaciones que se actualizan mientras se dibuja; arrastre de esquina que mueve dos muros con áreas en vivo.
- MCP: `GetRooms` → 11,1744 m²; `MoveCorner` → 2 muros, 11,8920 m², misma clave; `Undo` → "Move corner".
