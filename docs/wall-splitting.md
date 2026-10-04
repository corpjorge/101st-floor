# División automática de paredes

Al dibujar una pared que llega a otra, la pared anfitriona se convierte en dos piezas independientes. Si dos
paredes se cruzan por su interior, ambas quedan divididas. Se aplica también al mover extremos o esquinas y al
cambiar una longitud que crea una unión. Las paredes deben pertenecer a la misma planta.

## Uso
1. Dibuja una pared de 10 m.
2. Dibuja otra que llegue a ella a 4 m del inicio: quedan tramos de 4 m y 6 m.
3. En **Seleccionar (V)**, haz clic sobre el tramo que quieres editar. **Pintar (M)** afecta solo a la cara de ese tramo;
   **Supr** borra únicamente la pieza seleccionada.
4. **Ctrl+Z** deshace el trazado y sus divisiones juntos. **Ctrl+Y** los restaura con los mismos IDs.

Los tramos heredan altura, espesor y acabados. Puertas y ventanas conservan su posición y pasan a la pieza que las
contiene. Los suelos y techos mantienen sus acabados aunque cambien las claves de las habitaciones. Guardar y abrir
conserva las piezas separadas.

Si la división atraviesa una puerta, ventana o el bolsillo de una puerta corrediza, aparece un aviso y la operación
no se aplica. Sitúa la unión fuera de ese hueco. Los muros paralelos o superpuestos no se fusionan automáticamente.

Los archivos existentes se abren sin modificaciones automáticas: sus paredes se dividen cuando un nuevo trazado
o una modificación de ejes crea una unión con ellas. Quitar la pared divisoria no vuelve a fusionar los tramos.

Implementación y validación: [ADR-0025](adr/ADR-0025-wall-junction-segments.md).

Ejemplo generado desde el modelo: tramo de 4 m en ladrillo y tramo de 6 m en pintura verde, separados por una T.
Foto real de la aplicación con 64 muestras de path tracing.

![Dos tramos con acabados independientes](images/wall-splitting.png)
