# Architecture Decision Records

| ADR | Decisión | Estado |
|---|---|---|
| [0001](ADR-0001-units-mm.md) | Milímetros enteros como unidad persistente; parser decimal exacto | aceptado |
| [0002](ADR-0002-cad-core-independent-from-unreal.md) | CAD Core, Geometry y Scene Model en C++ estándar; tests con CMake + doctest | aceptado |
| [0003](ADR-0003-scene-model-neutral.md) | Scene Model neutral, derivado, IDs deterministas, conversión de ejes por consumidor | aceptado |
| [0004](ADR-0004-windows-rtx-primary-platform.md) | Solo Windows + DX12/SM6 + RTX, sin fallback a DX11 | aceptado y verificado |
| [0005](ADR-0005-unreal-dynamic-mesh-representation.md) | Paredes en Unreal con `UDynamicMeshComponent` (cards de Lumen en runtime) | aceptado y verificado |
| [0006](ADR-0006-undo-redo-transactions.md) | Comandos como datos; deshacer/rehacer con transacciones de estado por entidad | aceptado |
| [0007](ADR-0007-project-json.md) | `project.json`: nlohmann/json, validación estricta, guardado atómico | aceptado |
| [0008](ADR-0008-interaction.md) | Interacción: `EditController` puro + cliente de viewport y overlay Slate en Unreal | aceptado y verificado |
| [0009](ADR-0009-joins-and-rooms.md) | Uniones de muros y habitaciones derivadas de la geometría; vista previa como proyecto | aceptado y verificado |
| [0010](ADR-0010-openings.md) | Puertas y ventanas como huecos del muro; malla con mochetas; `project.json` esquema 2 | aceptado y verificado |
| [0011](ADR-0011-levels-and-slabs.md) | Plantas apiladas con cota derivada; forjados derivados por habitación; edición por planta; `project.json` esquema 3 | aceptado y verificado |
| [0012](ADR-0012-finishes.md) | Acabados de un catálogo por cara de muro y por habitación; texturas sintetizadas; material generado por commandlet; `project.json` esquema 4 | aceptado y verificado |
| [0013](ADR-0013-carpentry-and-glass.md) | Carpintería derivada de cada hueco; vidrio translúcido generado; huecos rechazados en uniones; arrastre de huecos; `project.json` esquema 5 | aceptado y verificado |
| [0014](ADR-0014-opening-types.md) | Tipos de puertas y ventanas: operación, bisagras, sentido y apertura; correderas empotradas con bolsillo validado; símbolo de apertura solo en la vista de edición; `project.json` esquema 6 | aceptado y verificado |
| [0015](ADR-0015-plan-drawings.md) | Planos 2D: módulo puro `Floor101Drawing` (plano con cotas y SVG); vista 2D con cámara ortográfica y capa Slate, editable; exportación PNG/SVG | aceptado y verificado |
| [0016](ADR-0016-furniture.md) | Mobiliario paramétrico: catálogo de 12 tipos en el núcleo, forma derivada en el Geometry Engine, snapping contra muros al colocar y arrastrar, en 3D y en el plano; `project.json` esquema 7 | aceptado y verificado |
| [0017](ADR-0017-imported-assets.md) | Assets importados: texturas PNG/JPEG y modelos glTF 2.0/OBJ leídos en el módulo puro `Floor101Assets`, copiados en `assets/<uuid>/` junto al proyecto; referencias `asset:<uuid>` como acabado o tipo de mueble; `project.json` esquema 8 | aceptado y verificado |

Solo se crea un ADR para decisiones difíciles de revertir o que afectan a varios módulos. Formato: contexto,
alternativas con ventajas y riesgos, decisión, consecuencias, verificación.
