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

Solo se crea un ADR para decisiones difíciles de revertir o que afectan a varios módulos. Formato: contexto,
alternativas con ventajas y riesgos, decisión, consecuencias, verificación.
