# ADR-0006: Comandos y deshacer/rehacer con transacciones de estado por entidad

Estado: aceptado (MVP 1, 2026-09-28)

## Contexto
El prompt maestro (§22) pide comandos (`CreateWall`, `DeleteWall`, `MoveWallEndpoint`, `ChangeWallThickness`,
`ChangeWallHeight`) que habiliten deshacer, rehacer y automatización futura por IA. Deshacer debe conservar la
identidad: una pared borrada y recuperada tiene que volver con el mismo `WallId`, porque de él dependen el nodo de
escena, el actor de Unreal y, en el futuro, la sincronización con Twinmotion.

## Alternativas
| Opción | Ventajas | Riesgos |
|---|---|---|
| Snapshot del proyecto completo por paso | trivial y siempre correcto | memoria O(tamaño × pasos); cambios difíciles de describir a los consumidores |
| Comandos con `undo()` propio | clásico | cada comando reimplementa su inverso: fuente de errores sutiles |
| **Transacciones de estado por entidad** | un único mecanismo genérico; validación antes de aplicar; conserva los ID; el `ChangeSet` sale gratis | hay que registrar la posición para restaurar el orden |

## Decisión
- Los comandos son datos (`std::variant` de structs, fáciles de serializar para IA o macros). `Document::execute`
  construye una `Transaction` con `{id, index, before, after}` por pared tocada, valida los estados `after` y solo
  entonces aplica. Un comando rechazado no toca el proyecto ni el historial; uno que no cambia nada no se registra.
- `undo()` aplica los `before`, `redo()` los `after`, y ambos notifican el `ChangeSet` correspondiente. Un comando
  nuevo vacía la pila de rehacer. `replaceProject` (cargar un archivo) limpia el historial.
- `ChangeWallLengthCommand` conserva el inicio y la dirección y cuantiza el final a milímetros enteros (ADR-0001). En
  diagonal, de los cuatro milímetros alrededor del punto exacto elige el de longitud más cercana a la pedida
  (`pointAtDistance`): el error queda por debajo de 0,5 mm y la longitud se lee tal cual se escribió.

## Consecuencias
- La `WallTool` y la UI solo emiten comandos. Cada pared dibujada es un paso deshacible.
- El historial no tiene límite por ahora (proyectos pequeños); se acotará cuando haga falta.
- Arrastres continuos (mover un extremo) generan un solo comando al soltar, no uno por frame.

## Verificación
`Tests/Unit/CommandTests.cpp`: deshacer y rehacer cada comando, borrado restaurado con el mismo ID y posición,
comandos rechazados sin efecto, no-op sin historial, rehacer vaciado por un comando nuevo, `replaceProject`.
