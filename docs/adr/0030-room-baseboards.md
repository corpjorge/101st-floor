# ADR-0030: Rodapiés por habitación

Estado: aceptado (2026-10-04).

## Contexto

Las habitaciones necesitan rodapiés con medidas y acabados propios. El perímetro
debe seguir derivándose de los muros, sin duplicar el modelo CAD ni cubrir puertas.

## Decisión

- `Project::roomBaseboards` guarda clave de habitación, activación, altura,
  espesor y acabado. `SetRoomBaseboardCommand` es una transacción reversible.
  Desactivar conserva la configuración. Ausencia significa sin rodapié.
- Dimensiones en milímetros enteros: altura 1–1000, espesor 1–100; por defecto
  100 y 12. Se usan acabados de carpintería y texturas compatibles; vacío resuelve
  a lacado blanco. Los assets referenciados no pueden borrarse mientras se usen.
- El esquema 12 añade esta colección opcional; se siguen leyendo esquemas 1–11.
  Los archivos guardados requieren una versión compatible con el esquema 12.
- Geometry genera prismas sobre el contorno interior, desde la cara superior del
  suelo, con esquinas a inglete. Huecos intersectados recortan solo su volumen.
  Scene Model aplica la cota de planta, el acabado y el estilo de rayos X.
  El nodo `Baseboard` se exporta a GLB bajo Suelos con identidad propia.
- Seleccionar el suelo permite activar y editar el rodapié en Propiedades.
  La división automática hereda los ajustes por planta y contención, dentro de la
  misma transacción, como los nombres y acabados de habitaciones.

## Consecuencias

No cambian paredes, áreas, snapping ni precisión. No se añaden perfiles curvos ni
piezas individuales editables. Las claves siguen la identidad derivada de los muros;
eliminar el muro que define una clave puede perder su asociación (ADR-0029).
En esquinas muy agudas se limita el inglete para evitar picos desproporcionados.
