# Cotas editables

En **Diseñar**, pulsa **Cotas (H)**. Se abre el plano 2D.
Haz clic en el primer punto, después en el segundo y finalmente donde quieras
colocar la línea. La medida se calcula automáticamente, también en diagonal.
**Esc** cancela la cota que estés preparando. Se usa el snapping del proyecto.

Para editar una cota manual o automática, vuelve a **Seleccionar (V)** y pulsa
su línea o el centro de su cifra. En **Propiedades** puedes:

- Cambiar **Separación**, o arrastrar la línea para moverla. Valores negativos
  sitúan la línea al otro lado de los puntos medidos.
- Escribir un **Rótulo** opcional; déjalo vacío o pulsa **Usar medida real** para
  recuperar la cifra calculada. El panel siempre muestra la distancia real.
- En cotas manuales, cambiar **Inicio X/Y** y **Fin X/Y**. Aceptan `mm`, `cm`, `m`;
  sin unidad se usan centímetros. Enter o salir del campo confirma el cambio.
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

- 297 pruebas C++ / 13.662 aserciones aprobadas.
- 84 pruebas Unreal aprobadas, incluida `Floor101.Interaction.Dimensions`.
  Reporte: `Saved/Automation/20261004-223005`.

- Interfaz real revisada a 1280×720 y 800×600; cifras con mínimo legible
  en pantalla y propiedades desplazables.

![Cotas manuales y automáticas](images/dimensions.png)
![Propiedades en ventana pequeña](images/dimensions-small.png)
