# Rodapiés

En **Diseñar → Seleccionar**, haz clic en el suelo de una habitación cerrada.
En **Propiedades**, activa **Rodapiés**. En ventanas pequeñas abre primero el
botón **Propiedades**.

- **Altura**: 10 cm por defecto; admite de 1 mm a 1 m.
- **Espesor**: 1,2 cm por defecto; admite de 1 mm a 10 cm.
- **Acabado**: lacado blanco inicialmente; puedes elegir madera, otros lacados o
  una textura importada compatible con carpintería.

Los campos usan la unidad de entrada del proyecto; también aceptan unidades
explícitas, por ejemplo `15 cm` y `18 mm`. Confirma con Enter o sal del campo.
Cada cambio admite **Deshacer/Rehacer**. Desactivar conserva sus medidas y acabado
para volver a activarlo.

El rodapié sigue el perímetro interior, se apoya sobre el suelo y se interrumpe en
puertas y ventanas que lleguen hasta su altura. Se ve en Diseñar, Recorrer y Foto,
y se incluye en la exportación GLB para Twinmotion. Al dividir automáticamente una
habitación, las resultantes heredan los ajustes y pueden editarse por separado.

Se guarda con el proyecto sin modificar paredes, áreas ni acabados del suelo.
Los proyectos antiguos abren sin rodapiés; los archivos guardados con esta versión
requieren compatibilidad con el esquema 12. Ver [ADR-0030](adr/0030-room-baseboards.md).

## Verificación

- 291 tests C++ y 13.560 aserciones aprobadas, incluidos huecos en muros invertidos,
  esquinas cóncavas, plantas, texturas importadas y geometría GLB.
- 83 tests Unreal aprobados; `Floor101.Interaction.Baseboards` verifica selección,
  materiales, activación, deshacer y reapertura. Reporte:
  `Saved/Automation/20261004-211415`.
- Capturas reales revisadas a 1280×720 y 800×600: controles completos y legibles.

![Rodapiés y propiedades](images/baseboards.png)
![Propiedades en ventana pequeña](images/baseboards-small.png)
