# ADR-0037: Luminarias con emisión real y elevación de mobiliario

Estado: aceptado y verificado (2026-10-20).

El usuario pide iluminación real de una vivienda, manteniendo sus modelos importados y pudiendo guardar el resultado.
Una lámpara importada por sí sola era geometría sin emisión. El estado de Unreal no debe ser fuente de verdad.

Se añade `Furniture::elevationMm` (base sobre el suelo terminado) y un `FurnitureLight` opcional: posición local en mm,
flujo entero en lúmenes, radio en mm, color sRGB y encendida. El esquema 16 conserva ambos; archivos anteriores tienen
elevación cero y ninguna luz. Los lectores antiguos rechazan el esquema 16, evitando perder la iluminación al guardar.

La luz pertenece a su luminaria: mover, girar, copiar, eliminar y deshacer usan las transacciones de mobiliario
existentes. No hay actores guardados ni dependencias nuevas de Unreal en el núcleo. Los offsets no se escalan al
redimensionar el mueble: son posiciones físicas editables en el archivo; el inspector permite elevación, encendido y flujo.
El resto de parámetros conserva sus valores al editar.

Geometry eleva las mallas paramétricas, importadas y de sustitución. SceneModel deriva luces puntuales absolutas con
id estable `<fixture>/body/light` y filtra las plantas igual que sus luminarias. El adaptador reconcilia actores
`APointLight` móviles y transitorios, con unidades en lúmenes, sombras y caída inversa al cuadrado. El GLB conserva
las fuentes mediante `KHR_lights_punctual`, en candelas (`lm / 4π`), metros y color lineal.

Alcance: fuentes puntuales vinculadas a muebles; no simulación eléctrica, cálculo reglamentario, IES, focos orientables
ni colocación independiente de luces. La carcasa no recibe un material emisivo automáticamente. Los modelos de
luminarias deben dejar el punto emisor fuera de su geometría opaca para que no bloquee su propia luz.

Validación: 351 unitarios y 93 Unreal Automation aprobados, incluida selección a la altura de un objeto elevado,
persistencia, rangos, compatibilidad, transformaciones, plantas, borrado/deshacer, exportación fotométrica y
reconciliación Unreal. Shipping 0.2.5 pasó 110 comprobaciones funcionales, incluidos emisores reales y reapertura.
Reportes: `Saved/Automation/20261020-165449` y `Saved/PackageChecks/20261020-170109-functional`.
