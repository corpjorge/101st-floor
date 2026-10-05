# Cotas editables

En **Diseñar**, pulsa **Cotas (H)**. Se abre el plano 2D.
Haz clic en el primer punto, después en el segundo y finalmente donde quieras
colocar la línea. La medida se calcula automáticamente, también en diagonal.
**Esc** cancela la cota que estés preparando. Los puntos no se ajustan a la
rejilla, ejes ni esquinas: se conserva el clic, redondeado al milímetro del modelo.

Para editar una cota manual o automática, vuelve a **Seleccionar (V)** y pulsa
su línea o el centro de su cifra. En **Propiedades** puedes:

- Cambiar **Separación**, o arrastrar la línea para moverla. Valores negativos
  sitúan la línea al otro lado de los puntos medidos.
- Escribir un **Rótulo** opcional; déjalo vacío o pulsa **Usar medida real** para
  recuperar la cifra calculada. El panel siempre muestra la distancia real.
- En cotas manuales, cambiar **Inicio X/Y** y **Fin X/Y**. Aceptan `mm`, `cm`, `m`;
  sin unidad se usan centímetros. Enter o salir del campo confirma el cambio.
- Arrastrar cualquiera de los cuatro cuadrados: los puntos medidos cambian
  libremente; las esquinas de la línea alargan o acortan sobre su dirección.
  El extremo opuesto queda fijo y la cifra se actualiza mientras arrastras.
  **Esc** cancela. En una automática, cambiar un extremo la convierte en manual
  y oculta la original sin modificar paredes; **Ctrl+Z** revierte todo en un paso.
- Pulsar **Eliminar (Supr)**. Las automáticas se ocultan individualmente.

**Vista → Restaurar cotas automáticas de esta planta** recupera las ocultas y
restablece sus posiciones y rótulos; mantiene las manuales.
Cada acción admite **Ctrl+Z/Ctrl+Y** y se guarda con el proyecto. SVG y PNG incluyen
las cotas y sus cambios. En ventanas pequeñas usa **Propiedades** y desplázate
por el panel para acceder a todos los campos.

Las cotas miden en planta. Editarlas no modifica paredes, áreas, acabados ni
rodapiés. Las manuales conservan puntos fijos; las automáticas siguen la geometría.
Los archivos guardados requieren un lector del esquema 13; esta versión abre
proyectos anteriores. Decisiones: [ADR-0031](adr/0031-editable-dimensions.md).

## Verificación

- 303 pruebas C++ / 13.848 aserciones aprobadas, incluidas mediciones libres
  junto a caras, esquinas y fuera de la rejilla; arrastre de ambos extremos y
  conversión reversible de automáticas.
- 85 pruebas Unreal aprobadas, incluidas `Floor101.Interaction.Dimensions` y
  `Floor101.Interaction.OverlappingRoomNames`.
  Reporte: `Saved/Automation/20261005-151148`.

- Interfaz real revisada a 1280×720 y 800×600; cifras con mínimo legible
  en pantalla y propiedades desplazables.
- Portable Windows **0.1.4 Shipping**, generado desde `93cb3df` con el árbol
  limpio. Integridad, dependencias y ejecución independiente aprobadas;
  `dimension_exact_points` verifica puntos fuera de rejilla. También pasan
  selección y nombre de habitaciones con solapamientos, guardado, reapertura,
  exportaciones y deshacer/rehacer.
  Reporte: `Saved/PackageChecks/20261005-130105-functional`.
- Entrega anterior **0.1.3 Shipping**, generada desde `6bca576` con el árbol
  limpio. `Scripts/CheckPackage.ps1` aprobó las comprobaciones de integridad,
  dependencias y ejecución del paquete, incluida creación de cotas, edición y
  ocultación de automáticas, deshacer/rehacer, reapertura y exportación SVG.
  Reporte: `Saved/PackageChecks/20261004-223753-functional`.

## Entrega portable

Extrae `Packaged/0.1.5/101st-Floor-portable.zip` y abre `Floor101.exe`.
Conserva todos los archivos de la carpeta junto al ejecutable. La carpeta
ya extraída está en `Packaged/0.1.5/Windows/`.
La validación se ejecutó en este equipo; queda pendiente probar la entrega
en otro equipo sin Unreal instalado.

![Cotas manuales y automáticas](images/dimensions.png)
![Propiedades en ventana pequeña](images/dimensions-small.png)

![Cota libre con coordenadas exactas](images/dimensions-exact.png)

![Extremos y esquinas para ajustar una cota](images/dimension-grips.png)
![Controles de cota en ventana pequeña](images/dimension-grips-small.png)
