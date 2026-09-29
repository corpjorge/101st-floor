# ADR-0012: Acabados (materiales) de muros, suelos y techos

Estado: aceptado y verificado (MVP 5, 2026-09-29)

## Contexto
El MVP 5 (elegido por el usuario: "Materiales y acabados") pide materiales por cara: pintura, ladrillo, hormigón,
azulejo, tarima... en muros, suelos y techos, elegidos desde la UI, guardados en `project.json` y visibles con Lumen
HWRT. Hasta el MVP 4 todo usaba el material blanco de ejemplo del motor. Hay que decidir qué guarda el modelo, dónde se
guardan los acabados de las habitaciones (que son derivadas, ADR-0009), cómo se describe un material sin atar el núcleo
a Unreal y cómo se crea el material de Unreal sin pasos manuales en el editor.

## Alternativas
| Tema | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Qué se guarda | Material libre (color, rugosidad) por cara | flexible | proyectos inconsistentes; UI compleja |
| Qué se guarda | **Id de un catálogo fijo** por superficie | simple, validable, portable a exportadores | pocos acabados hasta que haya un sistema de assets |
| Habitaciones | Entidad `Room` guardada | nombre y propiedades | se desincroniza al editar muros (ADR-0009) |
| Habitaciones | **Acabados por clave de habitación** | sin entidad nueva; la clave ya es estable | se pierden si la habitación cambia de clave (al cambiar su muro de id menor) |
| Aspecto | Texturas como assets importados | calidad | binarios en Git; pasos de importación |
| Aspecto | **Patrón descrito en el catálogo y texturas sintetizadas en C++** | reproducible, determinista, sin binarios, igual para exportadores | patrones sencillos (sin relieve) |
| Material de Unreal | Crear el `.uasset` a mano en el editor | rápido | prohibido: paso manual |
| Material de Unreal | **Commandlet que construye `M_Floor101Finish` desde código** (`Scripts/GenerateAssets.ps1`) | reproducible desde Git + Scripts | el `.uasset` generado va en Git (LFS) |

## Decisión
- **Catálogo** (`F101/Cad/Finishes.h`, núcleo puro): 11 acabados con id estable (`plaster-white`, `paint-sand`,
  `paint-sage`, `paint-slate`, `brick-red`, `concrete`, `tile-white`, `screed-light`, `tile-grey`, `wood-oak`,
  `wood-walnut`), las superficies que admite cada uno (muro, suelo, techo) y su aspecto neutral: color sRGB, color de
  junta, rugosidad y patrón (liso, moteado, ladrillo a soga, baldosas, tablas) con el tamaño de pieza y junta en mm.
  Por defecto: muros y techos `plaster-white`, suelos `screed-light`. Los nombres en español viven en la UI.
- **Modelo**: `Wall::finishLeft/finishRight` (caras a izquierda y derecha del eje, vacío = por defecto) y
  `Project::roomFinishes` (`RoomFinish{room, floor, ceiling}` por clave de habitación). Validación: el id existe y admite
  la superficie. Comandos `SetWallFinishCommand` y `SetRoomFinishCommand` (deshacibles; las transacciones guardan también
  estados de acabado de habitación). Un acabado de una habitación que ya no existe se conserva sin efecto (vuelve si la
  habitación vuelve, p. ej. al deshacer).
- **Escena**: cada `SceneNode` lleva el acabado de cada sección (`finishes`, en el orden de `mesh.sections`): caras
  izquierda y derecha del muro según el modelo; parte superior, extremos, mochetas y uniones con el acabado por defecto
  de muro; suelos con el del suelo de su habitación; la cara inferior del forjado (`ceiling`) con el del techo. El
  `contentHash` incluye los acabados, así que un cambio de acabado actualiza el actor sin tocar la malla.
  `buildFinishTexture`/`buildFinishLook` sintetizan la textura de un acabado (una repetición sin costuras, con toda la
  cadena de mips) y su rugosidad; un id desconocido sale magenta.
- **Unreal**: `M_Floor101Finish` (textura × `Tint`, `Roughness`, repetición `RepeatU/RepeatV` en metros sobre las UV en
  metros de la malla) generado por el commandlet `Floor101GenerateAssets`; `UFloor101FinishLibrary` (subsistema de
  motor) crea y comparte una instancia y una textura por acabado. Los estilos de vista tiñen el mismo material; las
  plantas de abajo (`Context`) muestran sus acabados oscurecidos. Si el material falta, se registra un error y
  `CheckRendering` falla ("Finish material loaded").
- **Interacción**: herramienta **Pintar (M)** con paleta: clic en un muro pinta la cara del lado del eje donde entra el
  rayo; clic en el suelo de una habitación pinta su suelo. En Seleccionar, clic en el suelo selecciona la habitación
  (suelo naranja; panel con suelo y techo). El panel del muro nombra las caras por orientación ("Cara norte"). El núcleo
  localiza habitaciones con un `RoomLocator` inyectado (lo da el Geometry Engine, `makeRoomLocator`), sin depender de él.
- **`project.json` esquema 4**: `finishes` (`left`, `right`) en los muros que tienen alguno y `roomFinishes` si hay; los
  esquemas anteriores se leen sin acabados.
- **MCP**: `GetFinishes`, `SetWallFinish`, `SetRoomFinish`; muros y habitaciones informan sus acabados.

## Consecuencias
- Las texturas son procedurales y planas (sin mapas de normales ni variación de rugosidad): se ven bien a distancia de
  edición; un Modo Foto podrá sumar normales o assets reales sin cambiar el modelo (el catálogo es la referencia).
- Los acabados de habitación dependen de la clave: dividir una habitación con un muro nuevo deja el acabado en la parte
  que conserva la clave. Una entidad `Room` con nombre podrá absorberlos más adelante.
- Los techos solo se ven desde dentro de la habitación con "Ver todas" (la planta activa no muestra sus forjados).
- Añadir un acabado es añadir una fila al catálogo y su nombre en la UI; los proyectos existentes siguen válidos.

## Verificación
- Unitarios (doctest): catálogo (ids únicos, superficies, valores por defecto), validación y mensajes, comandos con
  deshacer, secciones de la escena y hash, herramienta Pintar (caras por lado, suelo, rechazos), selección de habitación
  y propiedades, esquema 4 (ida y vuelta, rechazos), texturas (repeticiones, mips, determinismo, magenta). 151 casos.
- Unreal Automation: `Floor101.Adapter.Finishes` (material base con sus parámetros, material por sección, textura 512 con
  10 mips sRGB, repetición 1 m, rugosidad 0,9, cambio de acabado en el mismo actor), `Floor101.Interaction.Finishes`
  (pintar y seleccionar por el subsistema), spec del toolset (acabados y errores), `M` en el mapa de teclas. 31 pruebas.
- Prueba real (ratón y teclado) y MCP: ver `docs/mvp5-report.md`.
