# ADR-0031: Cotas manuales y automáticas editables

Estado: aceptado (2026-10-04).

## Contexto
Las cotas del plano se generaban al dibujar: no existían como selección ni podían
ocultarse individualmente. Se necesitan mediciones entre puntos libres y cambios
de presentación conservando la geometría CAD como fuente de verdad.

## Decisión
- `Project::dimensions` guarda anotaciones por planta: extremos y separación en
  milímetros enteros, identidad, rótulo opcional y flags automática/oculta.
  `project.json` pasa al esquema 13 y conserva lectura de los esquemas 1–12.
- Una cota manual obtiene UUID al crearla. Las automáticas tienen claves derivadas
  del UUID del muro, rol y extremos de huecos por UUID; desplazar un hueco mantiene
  su identidad. Los puntos de una automática se recalculan desde el modelo y sus
  entradas guardadas solo modifican separación, rótulo y visibilidad.
- `SetDimensionCommand`, `DeleteDimensionCommand` y `ResetDimensionsCommand`
  producen transacciones reversibles y notificaciones. Eliminar una planta incluye
  sus anotaciones en la misma transacción.
- Drawing devuelve las cotas resueltas para picking mediante un proveedor inyectado
  en EditController; CadCore sigue sin depender de Drawing ni Unreal.
- Herramienta en plano 2D: inicio, fin y colocación. Desde 0.1.4, extremos
  libres cuantizados a 1 mm, sin snapping de construcción: medir una cara o un
  punto cercano a una esquina no debe desplazarlo al eje ni a la rejilla.
  Selección por línea o centro del rótulo con tolerancia de ocho píxeles; arrastre
  perpendicular para mover la línea. Propiedades permite editar rótulo y separación;
  las manuales también sus coordenadas. Medidas manuales se muestran a resolución
  de milímetro; automáticas conservan el formato de plano existente.
- Desde 0.1.5, cuatro controles visibles en la selección: dos puntos medidos
  para mover extremos libremente y dos esquinas para ajustar longitud sobre el
  eje original. El arrastre preserva el extremo opuesto y la separación; admite
  vista previa, cancelación y una sola transacción al soltar. Cambiar extremos de
  una automática ejecuta `MakeDimensionManualCommand`: oculta la original y crea
  una manual con UUID, rótulo y separación conservados. Un deshacer restaura ambas
  entradas; no modifica paredes. Restaurar automáticas mantiene la manual creada.
- Borrar una automática guarda una exclusión. Vista → Restaurar cotas automáticas
  restablece la planta activa en un solo paso, preservando cotas manuales.
  Vista, SVG y PNG consumen el mismo dibujo; selección y borradores no se exportan.

## Consecuencias
Las cotas no cambian paredes, huecos, áreas ni escena 3D. Son distancias en planta,
no mediciones verticales. Las manuales fijan puntos: no siguen entidades movidas.
Dividir muros crea cotas para los nuevos tramos; las preferencias quedan ligadas
al tramo que conserva el UUID. Las referencias obsoletas no se dibujan y pueden
limpiarse con Restaurar. Un rótulo personalizado es texto de presentación;
Propiedades siempre muestra la distancia real y permite recuperar su cifra.

## Verificación
Regresiones doctest e integración Unreal: clics exactos, diagonales, precisión,
arrastre, huecos, identidad, plantas, validación JSON, compatibilidad, SVG,
guardado/reapertura, borrado y deshacer/rehacer. Capturas reales y ejecutable
Shipping se documentan en [cotas](../dimensions.md).
