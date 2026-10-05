# ADR-0028: Distribución independiente para Windows

Estado: aceptado; validación del paquete pendiente.

## Contexto
Run.ps1 utiliza UnrealEditor -game. El usuario solicita una aplicación independiente,
primero portable y después instalable, conservando la pantalla de carga y diagnóstico.

## Decisión
- BuildCookRun compila el objetivo Game, cocina los recursos y archiva Windows x64
  Shipping con IoStore, prerrequisitos y sin símbolos de depuración en la entrega.
- Package.ps1 utiliza un directorio nuevo bajo Packaged/ y registra versión, commit,
  motor y hashes. Nunca sobrescribe una entrega anterior.
- Library/furniture y Library/textures se copian como archivos ordinarios a
  Floor101/Library, donde los lectores C++ estándar ya buscan. No se incluye Blender.
- Splash.png es NonUFS; el icono se genera del mismo monograma propio. Los proyectos
  y las preferencias del usuario no forman parte del paquete.
- Floor101Diagnostics (Core, PreLoadingScreen) registra el producto en Shipping y
  recibe errores fatales del motor. El motor instalado deshabilita sus propias trazas:
  activar bUseLoggingInShipping cambia categorías de cabeceras inline e incompatibiliza
  las bibliotecas precompiladas (LogSlate/LogSerialization). No se usa ese override ni
  se modifican/suprimen comprobaciones del motor. En Development se mantiene UE_LOG.
  La consola de comandos de desarrollo
  permanece deshabilitada. No se incluye el módulo Floor101Editor ni sus plugins MCP.
- WiX 5.0.2 local genera MSI por usuario y un setup Burn con los prerrequisitos de
  Visual C++ x64 que distribuye Unreal 5.8 (sustituye a UEPrereqSetup_x64.exe);
  detecta runtimes ya instalados para no degradarlos. Versiones fijadas y herramientas solo dentro del repositorio.
- Se desactiva la precarga especulativa de PSO: los hilos PSOPrecompilePool seguían
  activos y demoraban el cierre más de cinco minutos en el primer arranque Shipping.
  Se conserva la caché bajo demanda del motor/driver; puede haber una pausa al
  mostrar un material por primera vez. No se quitan shaders ni funciones de Foto.
- El instalador conserva los proyectos; la primera entrega se identifica como preliminar.

## Validación
Se requiere arrancar el ejecutable archivado, comprobar recursos y dependencias,
guardar/reabrir, deshacer/rehacer, exportar SVG/PNG, probar controles, HDR y diagnóstico.
La ejecución en un equipo limpio sin Unreal instalado es una comprobación adicional
que no puede inferirse de pasar las pruebas del editor en el equipo de desarrollo.
