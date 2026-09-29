# ADR-0011: Plantas (niveles) y forjados derivados

Estado: aceptado y verificado (MVP 4, 2026-09-29)

## Contexto
El MVP 4 (elegido por el usuario: "Plantas y forjados") pide edificios de varias plantas: niveles con altura de planta,
forjados entre ellas, cambiar de planta al editar y ver las de abajo como referencia. Hasta el MVP 3 todo estaba en
z = 0 y una habitación no tenía techo. Hay que decidir cómo se modela una planta, dónde vive su cota, qué se guarda, cómo
se generan los forjados, cómo se edita una planta sin que las otras estorben y qué pasa con los archivos anteriores.

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Cota | Cota absoluta guardada por planta | niveles con huecos o sótanos libres | cambiar una altura obliga a mover a mano todas las de encima; incoherencias |
| Cota | **Derivada: suma de las alturas de las plantas de abajo** | apilar es automático; cambiar una altura sube todo lo de encima | sin sótanos ni plantas separadas por ahora (la primera está en 0) |
| Muros | Los muros dentro de su planta (`Level::walls`) | agrupación natural | reescribir todo el núcleo que recorre `Project::walls` |
| Muros | **Cada muro con su `LevelId`** | cambio pequeño; comandos, deshacer y ficheros siguen igual | hay que filtrar por planta en picking, snapping, uniones y habitaciones |
| Forjado | Entidad `Slab` guardada | forma libre, huecos de escalera | se desincroniza al editar muros (como las habitaciones, ADR-0009) |
| Forjado | **Derivado: uno por habitación, en la franja alta de su planta** | siempre coherente; sin estado | solo sobre habitaciones cerradas; sin voladizos ni huecos de escalera |
| Altura del muro | Hasta la cara inferior del forjado | techo = muro | la fachada queda cortada por la franja del forjado |
| Altura del muro | **Altura de planta completa (suelo a suelo)** | fachada continua; el forjado encaja entre muros | la altura libre es altura de planta − forjado |
| Vista | Todas las plantas siempre | simple | el forjado tapa la planta que se edita; la de arriba estorba |
| Vista | **Planta activa**: las de arriba ocultas, la activa sin forjados, las de abajo atenuadas; "Ver todas" | se ve dentro de lo que se edita y se entiende el contexto | una opción de vista más |

## Decisión
- **Modelo**: `Level{id, name, heightMm (suelo a suelo), slabThicknessMm}` en `Project::levels`, de abajo arriba. La
  cota de una planta se deriva (`levelElevationMm`). Por defecto 2800 mm con forjado de 250 mm (altura libre 2550). Cada
  `Wall` tiene `level`; su geometría es local a su planta (z = 0 en su suelo). Un proyecto válido tiene al menos una
  planta, todos los muros en una planta suya y todo el edificio dentro del rango del modelo. Un documento nuevo nace con
  la planta baja.
- **Comandos**: `AddLevelCommand` (encima de todas), `ChangeLevelCommand` (nombre, altura, forjado; las de encima suben o
  bajan solas), `DeleteLevelCommand` (con sus muros, en un paso; la última no se borra). `CreateWallCommand` y
  `MoveJointCommand` llevan planta (nula = la primera); una esquina solo mueve muros de su planta aunque haya muros
  apilados en el mismo punto. Las transacciones guardan también estados de planta (ADR-0006 ampliado).
- **Muros a altura completa**: los muros nuevos toman la altura de su planta (suelo a suelo). Al cambiar la altura de
  una planta, sus muros que medían lo que ella pasan a medir la nueva altura en el mismo paso; los demás conservan la
  suya. Si un hueco dejara de caber, se rechaza con el motivo.
- **Uniones y habitaciones por planta**: `compileScene` resuelve cada planta por separado (muros de plantas distintas no
  se unen ni cierran habitaciones juntos) y sube su geometría a su cota. **Forjado derivado** por habitación
  (`<habitación>/slab`, `buildSlabMesh`): prisma sobre el contorno del suelo, en la franja alta de la planta
  (`[cota + altura − forjado, cota + altura]`), así que encaja entre los muros; es el techo de la habitación y el suelo de
  la planta de arriba. La planta más alta tiene su forjado como cubierta plana. Sin nada guardado: igual que ADR-0009.
- **Edición en la planta activa** (`EditController`): el rayo del puntero se baja a la cota de la planta activa, así que
  picking, asas, suelo y snapping funcionan igual que en una sola planta, solo con sus muros. Además, **los muros de la
  planta de abajo guían el cursor** (extremos y ejes, `snapReference`) para apilar muros exactos, sin unirse. Cambiar de
  planta (RePág/AvPág, botones, seleccionar un muro de otra planta) suelta la selección y el gesto. Añadir una planta la
  crea como la de arriba y la activa.
- **Vista** (`ViewOverlay::activeLevel/showAllLevels`): sin "Ver todas", las plantas de encima no se compilan, la activa
  no muestra sus forjados (se ve dentro desde arriba) y las de abajo llevan estilo `Context` (gris). Con "Ver todas" (L)
  se ve el edificio entero con forjados. Los exportadores compilan sin overlay: todo. La cámara sigue a la planta activa
  (su foco sube o baja con la cota) y "Encuadrar" solo mira las plantas visibles.
- **`project.json` esquema 3**: `levels` (`id`, `name` si no está vacío, `heightMm`, `slabThicknessMm`) y `level` en
  cada muro. Los archivos de esquema 1 y 2 se leen en una sola planta con id fijo
  (`f1010000-0000-4000-8000-000000000001`) tan alta como su muro más alto, para que los forjados derivados queden dentro
  de los muros; al guardar pasan a esquema 3.
- **MCP**: `GetLevels`, `AddLevel`, `SetLevel`, `RemoveLevel`, `SetLevelView`; `AddWall` y `MoveCorner` reciben
  `LevelId` (vacío = la primera); muros y habitaciones informan su planta.

## Consecuencias
- Los muros apilados son independientes: mover uno no mueve el de arriba (se ajusta gracias al snapping a la planta de
  abajo). Una futura restricción "alineado con la planta de abajo" puede apoyarse en esto.
- Un muro que no llega a la altura de su planta deja ver una rendija bajo el forjado (p. ej. el muro de referencia de
  2400 mm en una planta de 2800). Es coherente con el modelo; se corrige cambiando su altura.
- Limitaciones: sin sótanos (la primera planta está en cota 0), sin forjados fuera de habitaciones cerradas (voladizos,
  terrazas) ni huecos de escalera; los forjados no se editan; la altura libre no se valida contra los huecos (una ventana
  puede quedar tapada por el forjado); la etiqueta de área no se oculta tras un forjado con "Ver todas".
- Escaleras, cubiertas inclinadas y forjados editables se apoyarán en `Level` y en estas cotas.

## Verificación
- Unitarios (doctest): planta baja por defecto; cotas derivadas; cambio de altura (muros que siguen, muros que no,
  rechazo por hueco); borrado con sus muros y deshacer en orden; esquinas por planta; escena por planta (cotas, dos
  habitaciones con el mismo plano, forjados 2550–2800 y cubierta 5500–5800); vista activa/contexto/todas; editor en la
  planta activa (picking, muro nuevo a 3000 mm de altura, subir/bajar, añadir/borrar, propiedades); snapping a la planta
  de abajo; esquema 3, migración de 1 y 2, rechazos. 144 casos.
- Unreal Automation: `Floor101.Interaction.Levels` (forjado 255–280 cm entre muros de 388 cm, planta de abajo gris,
  muro dibujado a 280 cm, ocultar y "Ver todas"), mapa de teclas (RePág, AvPág, L), spec del toolset (plantas y
  errores). 27 pruebas.
- Prueba real (ratón y teclado) y MCP: ver `docs/mvp4-report.md`.
