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

Solo se crea un ADR para decisiones difíciles de revertir o que afectan a varios módulos. Formato: contexto,
alternativas con ventajas y riesgos, decisión, consecuencias, verificación.
