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
  recuperar la cifra calculada. El panel siempre muestra la distancia real, y el plano
  escribe la medida real entre paréntesis tras el rótulo (`eje (3,55)`), también en
  SVG y PNG, salvo que el rótulo ya sea esa cifra. Si el muro cambia, la cifra lo sigue.
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
proyectos anteriores. Decisiones: [ADR-0031](adr/ADR-0031-editable-dimensions.md).

## Cotas interiores

Cada habitación muestra sus medidas libres **de cara a cara**, por dentro, a 15 cm de la pared: una cota por cada
tramo recto del suelo, medida de la cara de una pared a la de la siguiente (una pared que entra en la habitación
parte el tramo). En una habitación rectangular aparecen dos, el largo y el ancho libres, porque la cara opuesta
repetiría la misma cifra. La cara de una pared entre dos habitaciones que ya lleva la cota de partición no repite
otra interior.

Son cotas automáticas: siguen a las paredes, se escriben en metros con dos decimales y se seleccionan, arrastran,
renombran, ocultan, convierten en manuales y restauran igual que las demás. Las cotas por fuera siguen midiendo los
ejes. Para la cifra al milímetro, selecciona la cota y mira «distancia real».

En habitaciones estrechas (un baño o un pasillo de 1 m), el nombre y el área se apartan a un lado para no quedar
debajo de una cifra. Solo se mueven en el plano; las cotas manuales no los desplazan.

**Vista → Cotas interiores** las muestra u oculta todas a la vez, en el plano y en el SVG/PNG exportado. Ocultas no se
pueden seleccionar, y los rótulos de las habitaciones vuelven a su sitio. Es una opción de la vista, no del proyecto:
no se deshace con Ctrl+Z y la aplicación la recuerda en este equipo. Las cotas de los muros y las manuales no cambian.
Decisiones: [ADR-0034](adr/ADR-0034-interior-dimensions.md).

## Nombre y área de cada habitación

Si el nombre o el área de una habitación no caben o tapan una cota:

- **Moverlos:** en el plano 2D (Tab), con Seleccionar, arrastra el nombre o el área; cada uno se mueve por su cuenta y
  queda donde lo sueltes, aunque cambien las cotas. Pulsar el texto también selecciona la habitación (sus textos se
  ven en color de selección). Un texto encima de una cifra se coge antes que la cota; Esc cancela el arrastre.
- **Tamaño:** en **Propiedades → Rótulo en el plano**, *Tamaño del nombre* y *Tamaño del área* son la altura de las
  letras en el plano (sin unidad = cm; por defecto 20 y 15 cm; de 5 cm a 1 m). Solo cambian esa habitación.
- **Colocación automática** devuelve los dos textos a su sitio automático (apartados de las cotas) y conserva los
  tamaños.

Todo se deshace con Ctrl+Z, se guarda en el proyecto (`project.json` esquema 14) y sale igual en el SVG y el PNG. Al
dividir una habitación con una pared, las dos nuevas heredan su rótulo. En 3D los rótulos no cambian. Decisiones:
[ADR-0035](adr/ADR-0035-room-label-layout.md).

## Verificación

- 303 pruebas C++ / 13.880 aserciones aprobadas, incluidas mediciones libres
  junto a caras, esquinas y fuera de la rejilla; arrastre de ambos extremos y
  conversión reversible de automáticas.
- 85 pruebas Unreal aprobadas, incluidas `Floor101.Interaction.Dimensions` y
  `Floor101.Interaction.OverlappingRoomNames`.
  Reporte: `Saved/Automation/20261005-151148`.

- Interfaz real revisada a 1280×720 y 800×600; cifras con mínimo legible
  en pantalla y propiedades desplazables.
- Portable Windows **0.1.5 Shipping**, generado desde la fusión `77290b7` con
  el árbol limpio. Integridad y ejecución independiente aprobadas: extremos
  arrastrables sin snapping, conversión de automáticas a manuales, reversión
  atómica, guardado/reapertura, exportaciones y funciones anteriores.
  Reporte: `Saved/PackageChecks/20261005-151937-functional`.
- Entrega anterior **0.1.4 Shipping**, generada desde `93cb3df` con el árbol
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

Extrae `Packaged/0.1.7/101st-Floor-portable.zip` y abre `Floor101.exe`.
Conserva todos los archivos de la carpeta junto al ejecutable. La carpeta
ya extraída está en `Packaged/0.1.7/Windows/`.
La validación se ejecutó en este equipo; queda pendiente probar la entrega
en otro equipo sin Unreal instalado.

![Cotas manuales y automáticas](images/dimensions.png)
![Propiedades en ventana pequeña](images/dimensions-small.png)

![Cota libre con coordenadas exactas](images/dimensions-exact.png)

![Extremos y esquinas para ajustar una cota](images/dimension-grips.png)
![Controles de cota en ventana pequeña](images/dimension-grips-small.png)

## Cotas cortas (0.1.7)

Las cifras que no caben entre las marcas se sitúan fuera del tramo, con una línea de referencia. Las marcas se
reducen para no superponerse y el texto tiene fondo blanco. Se mantienen la distancia exacta, los extremos,
la edición y la selección al pulsar sobre la cifra. La misma presentación se utiliza en el plano, SVG y PNG.

Validación 0.1.7: 315 casos C++ / 14.064 aserciones y 87 pruebas Unreal aprobadas.
Las regresiones incluyen horizontales, verticales, diagonales, automáticas, rótulos UTF-8, selección desde la cifra,
precisión persistida y separación de una cota corta en cadenas de huecos. Reporte: `Saved/Automation/20261005-192519`.

![Cota de 8 cm junto a la unión, con cifra separada](images/short-dimensions.png)
![Cota corta y propiedades en ventana pequeña](images/short-dimensions-small.png)

Portable Windows **0.1.7 Shipping** generado desde `4e30d1f` con árbol limpio. Integridad, dependencias y comprobación
funcional aprobadas en el ejecutable real; guardado, reapertura, cotas, SVG/PNG, arrastre de paredes y deshacer/rehacer
siguen pasando. Reporte: `Saved/PackageChecks/20261005-193542-functional`.
