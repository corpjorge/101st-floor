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
| [0018](ADR-0018-photo-mode-path-tracing.md) | Modo Foto: path tracer de Unreal en el viewport del juego, sin marcas de edición; progreso, calidad y guardado PNG; comprobado en `CheckRendering.ps1` | aceptado y verificado |
| [0019](ADR-0019-catalogue-and-library.md) | Catálogo con variantes: 80 acabados por categoría (espiga, ajedrezado), muebles por categoría, biblioteca integrada (`Library/`, 53 muebles de Blender, 14 texturas CC0) añadida al proyecto como assets; Pintar suelos; `project.json` esquema 9 | aceptado y verificado |
| [0020](ADR-0020-gltf-export-twinmotion.md) | Exportación glTF (`.glb`) para Twinmotion: módulo puro `Floor101Exporters` contra la escena neutral, metros con Y arriba, nombres estables por planta y clase, materiales con sus imágenes (PNG propio), repetición horneada en los UV | aceptado y verificado |
| [0021](ADR-0021-columns-and-building-services.md) | Pilares (rectangulares y redondos, tan altos como su planta) e instalaciones (electricidad, agua, aguas negras, gas) como polilíneas 3D por tramos con color y diámetro por red; capa con rayos X (estilo `Ghost`); `project.json` esquema 10 | aceptado y verificado |

| [0022](ADR-0022-hdr-display-output.md) | HDR opcional por pantalla, botón y preferencia local; metadatos de Windows; fotos EXR/PNG y comprobación HDR→SDR | aceptado y verificado |

Solo se crea un ADR para decisiones difíciles de revertir o que afectan a varios módulos. Formato: contexto,
alternativas con ventajas y riesgos, decisión, consecuencias, verificación.
