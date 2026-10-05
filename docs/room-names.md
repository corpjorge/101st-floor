# Nombres de habitaciones

En **Diseñar → Seleccionar**, haz clic en el suelo de una habitación cerrada por
muros. En el panel de propiedades, escribe **Nombre** y pulsa Enter o sal del campo.
En ventanas pequeñas, abre **Propiedades** para ver el mismo control.

Puedes escribir Cocina, Sala de estar, Baño o cualquier nombre de una sola línea.
Los cambios aparecen en el rótulo sobre el suelo y en el plano 2D. Las exportaciones
SVG/PNG usan esos mismos nombres. Borrar el texto restaura «Habitación 1»,
«Habitación 2», etc. Deshacer/Rehacer incluye cada cambio confirmado.

El nombre se guarda en el proyecto, independientemente de los acabados. Al dividir
automáticamente una habitación, las resultantes heredan su nombre y pueden
renombrarse por separado. No cambian dimensiones ni áreas.

El formato de proyecto pasa al esquema 11; los proyectos anteriores siguen
abriendo con nombres automáticos. Después de guardar con esta versión, el archivo
requiere una versión del juego compatible con el esquema 11. Los nombres siguen
las claves derivadas de los muros; ver [ADR-0029](adr/0029-room-names.md) para las
limitaciones de reasociación al eliminar muros.

## Verificación

- 281 tests C++ y 13.090 aserciones aprobadas: renombrar, borrar, notificación,
  deshacer/rehacer, UTF-8, validación de archivos, planos y división de habitaciones.
- 82 tests Unreal aprobados; `Floor101.Interaction.RoomNames` comprueba selección
  por suelo, área intacta, guardado/reapertura y SVG. Reporte:
  `Saved/Automation/20261004-203543`.
- Capturas reales revisadas a 1280×720 y 800×600.
- Portable Shipping 0.1.1 aprobado: renombrar, dividir, guardar/reabrir y SVG con
  nombre personalizado; también PNG, biblioteca, controles, diagnóstico y foto.
  Reporte: `Saved/PackageChecks/20261004-204130-functional`.
- Instalador 0.1.1 aprobado: instalación, accesos directos, arranque independiente,
  desinstalación que conserva el proyecto de prueba y payloads offline verificados
  por SHA-256. Reporte: `Packaged/InstallerChecks/20261004-204327`.

El ejecutable está en `Packaged/0.1.1/Windows/Floor101.exe`; conserva completa esa
carpeta. El ZIP portable y el instalador están en `Packaged/0.1.1/`. Ambos se
generaron desde el commit `7034928`, sin cambios pendientes de código.

![Propiedades de una habitación](images/room-properties.png)
![Propiedades en ventana pequeña](images/room-properties-small.png)
