# ADR-0029: Nombres editables de habitaciones

Estado: aceptado (2026-10-04).

## Contexto

Las habitaciones se detectan desde los muros; los rótulos automáticos «Habitación 1»
no permiten identificar Cocina, Sala o Dormitorio. El nombre debe persistir sin
convertir la geometría derivada en una segunda fuente de verdad.

## Decisión

- `Project::roomNames` guarda pares de clave de habitación y nombre UTF-8, separados
  de los acabados. `SetRoomNameCommand` aplica una transacción reversible; un nombre
  vacío elimina la personalización. Se admiten nombres repetidos, una sola línea y
  hasta 256 bytes UTF-8. No se guardan superficies ni áreas.
- El esquema 11 añade `roomNames`; se siguen leyendo los esquemas 1–10 sin cambios
  en el modelo geométrico. Los proyectos antiguos usan nombres automáticos.
- Seleccionar el suelo muestra Nombre en propiedades. Los rótulos 3D, planos 2D,
  SVG/PNG y nombres de nodos de la escena derivan del mismo dato.
- Los nombres siguen la clave determinista de la habitación, como sus acabados.
  La división automática de muros reasocia nombres por planta y contención del
  punto interior, dentro de la misma transacción. Las habitaciones nuevas heredan
  el nombre y pueden renombrarse independientemente; deshacer restaura los datos.

## Consecuencias

La precisión, las áreas y los acabados no cambian. Una edición que elimina el muro
que determina la identidad puede cambiar la clave; no se promete reasociación
general para eliminar o fusionar habitaciones. Se conservan entradas huérfanas
para recuperar el nombre al reconstruir esa clave. Versiones anteriores al esquema
11 rechazan los nuevos archivos, en lugar de descartar los nombres silenciosamente.
