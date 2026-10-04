# ADR-0025: tramos de pared independientes en uniones y cruces

Estado: aceptado.

## Contexto
El grafo de habitaciones ya divide ejes en T y cruces (ADR-0009), pero esas divisiones eran derivadas. Seleccionar,
pintar o borrar seguía afectando a toda la pared original. El usuario necesita editar cada tramo por separado.

## Decisión
- Después de crear o modificar ejes, dividir los muros de la misma planta en las uniones que involucran esos ejes.
  Una T produce dos tramos del anfitrión; un cruce interior divide ambas paredes. No unir solapes colineales ni
  dividir paredes de otras plantas.
- Guardar cada tramo como `Wall`, sin cambiar el formato del proyecto. El primer tramo conserva su ID; los demás
  reciben IDs nuevos una sola vez. Conservar sentido, planta, altura, espesor y ambos acabados.
- Cuantizar las intersecciones explícitamente a milímetros y compartir el punto entre los tramos. Cuando una T
  termina en la cara del anfitrión, llevar su extremo al eje siguiendo su dirección; los acabados y las uniones
  geométricas siguen siendo derivados.
- Trasladar puertas y ventanas al tramo correspondiente con el mismo ID y posición, ajustando su distancia local.
  Rechazar toda la operación si corta un hueco o el bolsillo de una puerta corrediza, o genera un tramo inválido.
- Construir el resultado sobre una copia y registrar creación/edición, divisiones y acabados en una transacción.
  Deshacer restaura exactamente el proyecto; rehacer conserva IDs y orden. Borrar un tramo no vuelve a fusionar los demás.
- El Geometry Engine inyecta una transferencia de acabados de habitaciones, siguiendo el patrón de obstrucciones
  de ADR-0013. Las habitaciones resultantes heredan suelo/techo de la habitación previa que contiene su punto interior,
  salvo que su clave ya tenga un acabado. Conservar las claves antiguas, como el modelo existente; deshacer revierte
  también las nuevas asignaciones. CAD Core no depende de Geometry.

## Consecuencias y verificación
Las divisiones se aplican a nuevos trazados y cambios de ejes, incluidos arrastres de esquinas y cambios de longitud.
Abrir un archivo antiguo no lo modifica ni divide automáticamente todas sus paredes. Pintar, cambiar altura/espesor
o editar huecos tampoco altera la partición.

Las pruebas cubren T, cruces múltiples, diagonales, contacto en caras, plantas, huecos y bolsillos, acabados,
selección/pintura/borrado independientes, guardado y deshacer/rehacer. Unreal Automation comprueba actores separados,
reconciliación, selección y persistencia en la aplicación. Guía: [división de paredes](../wall-splitting.md).
