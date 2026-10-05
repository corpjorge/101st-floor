# Mover una pared completa

En **Seleccionar (V)**, mantén pulsado el botón izquierdo sobre el cuerpo de la pared y arrastra. Puedes agarrarla
con el primer clic o después de seleccionarla en naranja. Las puntas amarillas siguen moviendo una esquina.

- El cuerpo traslada ambos extremos con el mismo desplazamiento: conserva longitud y dirección. El punto de agarre
  no salta a la cuadrícula; el movimiento se cuantiza a milímetros, también en perspectiva.
- Los extremos de otras paredes unidos a esos dos puntos se desplazan juntos, manteniendo sus otros extremos fijos.
  Solo se afectan paredes de la misma planta. Los huecos y acabados de la pared trasladada se conservan.
- La vista previa muestra todas las paredes afectadas. Si un movimiento colapsa una vecina o deja su hueco fuera de
  ella, se conserva la última posición válida. Las uniones nuevas mantienen la división automática existente;
  si una división corta un hueco, se rechaza el movimiento completo.
- Soltar confirma **un solo paso de deshacer**. **Esc** cancela antes de soltar; **Ctrl+Z** deshace después.
  Hacer clic sin mover no añade un paso al historial. Los muebles y las cotas manuales permanecen en sus posiciones.

Las puertas y ventanas seleccionadas conservan su arrastre propio; para trasladar el muro, agarra su parte sólida.

Implementación: [ADR-0032](adr/ADR-0032-wall-body-drag.md). Versión Windows: **0.1.6**.

## Validación

- 312 casos C++ y 13.983 aserciones aprobados; límites de módulos respetados.
- 86 pruebas Unreal Automation aprobadas, incluida `Floor101.Interaction.WallBodyDrag`.
  Reporte: `Saved/Automation/20261005-154826`.
- Captura real sobre una copia del plano de reproducción, sin modificar el documento del usuario.

![Pared trasladada desde su cuerpo](images/wall-body-drag.png)
![Propiedades en ventana pequeña](images/wall-body-drag-small.png)
