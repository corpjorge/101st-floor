# ADR-0016: Mobiliario paramétrico

Estado: aceptado y verificado (MVP 9, 2026-09-29)

## Contexto
El MVP 9 (elegido por el usuario: "Mobiliario") pide un catálogo de muebles paramétricos generados desde código (cama,
mesa, sillas, sofá, armario, cocina, baño básico) que se colocan, mueven, giran y borran con el ratón, con snapping a
muros, que salen en 3D con Lumen y en el plano 2D, y que se guardan en `project.json`. Hay que decidir qué guarda el
modelo, dónde se genera la forma, cómo se orienta un mueble contra un muro y cómo se edita.

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Qué se guarda | Mallas o assets de Unreal por mueble | detalle libre | rompe "Unreal solo representa"; binarios en Git |
| Qué se guarda | **Tipo del catálogo + planta + centro + giro + ancho/fondo/alto + acabado** | pequeño, versionable, validable; la forma se deriva | los muebles son tan ricos como su generador |
| Giro | `double` en grados | directo | no es entero persistente (ADR-0001) |
| Giro | **Centésimas de grado enteras en [0, 36000)** | entero; cualquier muro se sigue con error < 0,1 mm en 2 m | cuartos de vuelta tratados aparte para ser exactos |
| Forma | Mallas de Unreal (Modeling Tools) | booleanas | lógica de producto en Unreal |
| Forma | **Cajas en el marco del mueble, por tipo, en el Geometry Engine (C++ puro)** | testeable; misma forma en 3D y en planta | aspecto de maqueta (sin curvas) |
| Muros | Ninguna relación (colocación libre) | simple | colocar contra un muro es tedioso |
| Muros | **Snapping al colocar y arrastrar: la espalda contra la cara más cercana, deslizando por el muro hasta los muros con que choca** | natural; sin relaciones guardadas | un muro movido después no arrastra sus muebles |

## Decisión
- **Modelo** (`F101/Cad/Furniture.h`): `Furniture { id, type, level, position, rotationCentidegrees, widthMm, depthMm,
  heightMm, finish }`. Huella de ancho × fondo centrada en `position`; en su marco, x a lo ancho, y hacia la espalda
  (+y), así que con giro 0 mira al sur. `Project::furniture` en orden de creación. Catálogo fijo de 12 tipos
  (`furnitureCatalogue`): medidas por defecto y rangos, si va contra un muro y el acabado de su cuerpo. `validateFurniture`
  comprueba tipo, rangos, giro, posición y acabado (`Surface::Furniture`); `validateProject`, ids únicos y planta
  existente.
- **Comandos** `AddFurnitureCommand` (id nuevo; planta nula = la primera), `ChangeFurnitureCommand`,
  `DeleteFurnitureCommand`; `FurnitureChange` en la transacción, `ChangeSet` con altas, cambios y bajas; borrar una
  planta borra sus muebles en el mismo paso. Etiquetas "Add/Change/Delete furniture".
- **`project.json` esquema 7**: `"furniture": [{ id, type, level, position: [x, y], rotationCentidegrees, widthMm,
  depthMm, heightMm, finish? }]`, solo si hay muebles. Los esquemas 1-6 se siguen leyendo.
- **Acabados**: nuevos del catálogo solo para muebles (tela gris y arena, lino blanco, cerámica blanca, piedra oscura,
  acero cepillado, vidrio negro); roble, nogal, lacado blanco y aluminio también valen para muebles.
- **Forma** (`F101/Geometry/FurnitureBuilder.h`): `furnitureShape` da las cajas de cada tipo (con huecos abiertos por
  arriba para lavabos, fregadero y bañera) y los contornos de detalle para el plano (almohadas, colchón, cubeta, fuegos);
  todo se ajusta a las medidas del mueble. Ranuras de material: `body` (el acabado del mueble o de su tipo) y fijas
  `linen`, `fabric`, `ceramic`, `metal`, `stone`, `dark`. `buildFurnitureMesh` las gira y coloca sobre el suelo de la
  habitación (z = 10 mm).
- **Escena**: un nodo `furniture/<id>` por mueble (`SourceKind::Furniture`), elevado a su planta, con estilos de vista:
  seleccionado, vista previa (`preview/furniture/<n>` para el que se coloca), contexto en las plantas de abajo.
- **Plano**: contorno de cada mueble y sus detalles con la tinta `Furniture` (gris), o seleccionado / vista previa.
- **Interacción** (`EditController`, ADR-0008): modo `Furniture` (tecla A) con el tipo de `EditSettings::furnitureType`.
  El mueble sigue al cursor en la rejilla; si su tipo va contra un muro y su centro está a su medio fondo + 40 cm de la
  cara de un muro, gira de espaldas a esa cara (la del lado del cursor), la toca y se desliza por el muro en la rejilla,
  apartándose de los muros con que choca (en una esquina se para en la cara del otro muro). Espacio gira 90° los
  siguientes; clic coloca. En Seleccionar, un clic en un mueble (más cerca que cualquier muro) lo selecciona y arrastrarlo
  lo mueve con el mismo snapping (un `ChangeFurnitureCommand` al soltar); Espacio lo gira 90°, Supr lo borra; el panel
  edita ancho, fondo, alto, giro en grados y acabado.
- **MCP**: `GetFurnitureTypes`, `GetFurniture`, `AddFurniture` (sin snapping; medidas 0 = las del tipo),
  `SetFurniture`, `SetFurnitureFinish`, `RemoveFurniture`; `GetFinishes` indica `bForFurniture`.

## Consecuencias
- Los muebles son cajas: se leen bien en planta y en 3D, pero no tienen curvas ni detalle fino.
- No hay colisión entre muebles ni con puertas; el snapping solo mira muros de la planta.
- Un muro que se mueve o se borra no arrastra ni borra los muebles apoyados en él (no se guarda la relación).
- El giro con Espacio es sobre el centro y no vuelve a ajustar contra el muro; arrastrarlo sí.

## Verificación
- Unitarios (`FurnitureTests.cpp`): catálogo y rangos; validación; centésimas de grado y marco del mueble; comandos con
  deshacer/rehacer y notificaciones; borrar planta; `validateProject`; JSON esquema 7 exacto, ida y vuelta y errores;
  cajas de los 12 tipos dentro de su huella a medidas mínimas, por defecto y máximas; malla girada y acabados por ranura;
  escena (nodos, elevación, estilos); picking; modo Amueblar (rejilla, giro, snapping contra un muro, esquina, muro
  diagonal); selección, arrastre (también con el agarre al otro lado del muro), edición, giro, borrado, plantas; plano.
  205 casos.
- Unreal Automation: `Floor101.Interaction.Furniture` (colocar, actor y medidas, seleccionar, arrastrar, girar, borrar,
  deshacer, plano, guardar y abrir), tecla A, spec del toolset (catálogo, alta con medidas del tipo, mover/girar/acabado en
  el mismo actor, errores). 42 pruebas; `Furniture` y `PlanView` también en el editor real por MCP.
- Prueba real y MCP: ver `docs/mvp9-report.md`.
