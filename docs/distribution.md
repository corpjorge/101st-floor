# Distribución Windows

101st Floor se distribuye como una carpeta portable y un instalador Windows x64.
La versión 0.1.0 es preliminar. El objetivo Game usa Shipping; no inicia UnrealEditor.

## Preparar una entrega

Desde PowerShell 7, con el editor y el juego cerrados:

```powershell
.\Scripts\Package.ps1 -Output Packaged/0.1.0-validation -Zip
.\Scripts\CheckPackage.ps1 -Package Packaged/0.1.0-validation/Windows
.\Scripts\CheckPackage.ps1 -Package Packaged/0.1.0-validation/Windows -Check home
.\Scripts\CheckPackage.ps1 -Package Packaged/0.1.0-validation/Windows -Check diagnostics -ResX 800 -ResY 600
.\Scripts\CheckPackage.ps1 -Package Packaged/0.1.0-validation/Windows -Check display
.\Scripts\CheckPackagedHdr.ps1 -Package Packaged/0.1.0-validation/Windows -EnableWindowsHdrForTest
.\Scripts\Installer.ps1 -Package Packaged/0.1.0-validation/Windows -RestoreTools
.\Scripts\CheckInstaller.ps1 -Msi Packaged/0.1.0-validation/Installer/101st-Floor-0.1.0.msi -Setup Packaged/0.1.0-validation/Installer/101st-Floor-0.1.0-Setup.exe
```

Package.ps1 compila, cocina y archiva materiales, shaders SM6, mapa inicial,
splash NonUFS, motor y prerrequisitos. Copia la biblioteca ordinaria que utilizan
los lectores C++ y crea release-manifest.json con hashes SHA-256. Los directorios
de salida son nuevos y siempre están bajo Packaged/; quedan fuera de Git.
-SkipBuild omite la compilación, no la preparación de recursos.

## Entrega al usuario

- **Portable:** extraer todo el ZIP y abrir Windows/Floor101.exe. Mantener juntas
  las carpetas Engine, Floor101 y Library dentro de Floor101.
- **Instalador:** ejecutar 101st-Floor-0.1.0-Setup.exe. Incluye los prerrequisitos
  Visual C++ x64 proporcionados por Unreal 5.8, aplicación, icono y accesos directos de Escritorio/Inicio. Instala
  para el usuario; los prerrequisitos compartidos pueden solicitar elevación.
- La desinstalación retira los archivos instalados y accesos directos; no elimina
  los proyectos guardados por el usuario ni sus preferencias.
- No se necesitan Unreal Editor, Visual Studio, Blender ni Epic Games Launcher.
  Sí Windows 11 x64, drivers actualizados y GPU compatible DX12/SM6.6/DXR 1.1.
  HDR requiere pantalla compatible y HDR habilitado en Windows.

WiX 5.0.2 y sus extensiones Bal/Util se restauran desde NuGet dentro del repositorio,
sin instalación global. Se fijan versiones para reproducir el instalador.
No se publica ni firma una entrega automáticamente.

## Diagnóstico y comprobaciones

La biblioteca Shipping del motor instalado deshabilita sus logs. Floor101Diagnostics
conserva el registro propio de la aplicación y captura GErrorHist al recibir un error
fatal del motor; no recompila ni cambia la ABI de Unreal. Opciones → Diagnóstico
controla el detalle y la ventana opcional. Los errores del producto se guardan siempre
en Floor101-product.log dentro de la carpeta de logs del usuario. Las trazas completas
de Unreal requieren una compilación Development; no se prometen en Shipping.

El archivo se abre una vez por sesión y cada línea se añade sin volver a abrirlo;
otra instancia puede escribir en el mismo archivo. Si al arrancar supera 16 MB, pasa
a Floor101-product.old.log (sustituye al anterior) y empieza uno nuevo. En Shipping, el
estado de cada muro y habitación tras cada edición solo se registra en modo depuración;
en Development se sigue registrando siempre y `f101.Demo.Status` lo escribe a demanda.

La precarga especulativa de PSO se desactiva para evitar el cierre de varios minutos
observado al estrenar Shipping; se conservan los shaders cocinados y la caché del driver.
Puede haber una pausa al dibujar un material por primera vez.

CheckPackage verifica hashes y ausencia de editor/símbolos, abre el mismo ejecutable
del usuario y registra sus DLL cargadas. El chequeo funcional crea un proyecto aislado,
importa biblioteca, comprueba división de paredes, guardar/reabrir, deshacer/rehacer,
exportaciones SVG/PNG/GLB, controles, diagnóstico y captura Foto. Las opciones display
y hdr reutilizan las comprobaciones reales de pantalla y HDR. Sus resultados y capturas
quedan en Saved/PackageChecks; no usan proyectos ni preferencias personales.

La prueba en otro equipo sin herramientas de desarrollo sigue siendo necesaria para
certificar una distribución pública. Un arranque local y la inspección de DLL no sustituyen
esa prueba. Los resultados concretos de esta entrega se registran al terminar su validación.

## Verificación local (2026-10-04)

- Objetivos Editor Development y Game Shipping compilados; 275 unitarios
  (13.035 aserciones) y 81 Unreal Automation, todos aprobados. Reporte:
  `Saved/Automation/20261004-185146`.
- Shipping: biblioteca 53/14, materiales cocinados, división de paredes, guardar/reabrir,
  deshacer/rehacer, importación, SVG/PNG/GLB, velocidad 0/3 y WASD, ventanas de diagnóstico
  y foto PNG: `Saved/PackageChecks/20261004-192705-functional`. Cierre normal aprobado.
- Pantalla nativa 2560×1440, render al 100 %, resolución elegida, VSync y Salir:
  `Saved/PackageChecks/20261004-191128-display`.
- HDR activado temporalmente: salida HDR, EXR 1280×720 con máximo lineal 1,914,
  regreso a SDR y estado original de Windows restaurado:
  `Saved/PackageChecks/20261004-191406-hdr`.
- Inicio, splash y diagnóstico legibles a 800×600; DLL del paquete/Windows, sin
  dependencias cargadas desde la instalación de Unreal.
- MSI instalado en carpetas aisladas, accesos directos verificados, aplicación
  instalada arrancada y desinstalada conservando un proyecto externo:
  `Packaged/InstallerChecks/20261004-193432`. La copia offline del EXE se extrajo con
  WiX y sus archivos incrustados coincidieron por SHA-256 con el MSI y Visual C++ x64.

![Shipping: diseño](images/packaged-design.png)
![Shipping: inicio](images/packaged-home.png)
![Shipping: diagnóstico](images/packaged-diagnostics.png)
![Shipping: carga](images/packaged-startup.png)

## Entrega 0.1.9 (2026-10-09)

Portable Windows **0.1.9 Shipping** aprobado, generado desde `ba98025` con árbol limpio: editor de cotas propio
(`DimensionEditor`), cortes de muro que conservan los huecos, divisiones que no desconectan ramas, traspaso conjunto de
acabados, nombres y rodapiés de habitación, medida real junto a cotas con texto propio y log de producto acotado.
Antes: 320 unitarios y 88 Unreal Automation aprobados (`Saved/Automation/20261009-201104`). El ejecutable aprueba las
86 comprobaciones funcionales, integridad SHA-256 y dependencias locales: `Saved/PackageChecks/20261009-201850-functional`.
Ejecutable: `Packaged/0.1.9/Windows/Floor101.exe`. ZIP: `Packaged/0.1.9/101st-Floor-portable.zip`.
La comprobación se realizó en este equipo; la prueba en otro equipo sin Unreal sigue pendiente.

## Entrega 0.2.0 (2026-10-10)

Portable Windows **0.2.0 Shipping**, generado desde `0f87f1a` con árbol limpio: interfaz Cota Cero en todo el estudio
(ADR-0033, `docs/ui-cota-cero.md`) sin cambios de funciones. Antes: 320 unitarios y 88 Unreal Automation aprobados
(`Saved/Automation/20261010-124543`). El ejecutable aprueba las 86 comprobaciones funcionales, integridad SHA-256 y
dependencias locales (`Saved/PackageChecks/20261010-131358-functional`) y la de inicio a 1920 × 1080
(`Saved/PackageChecks/20261010-131415-home`). Ejecutable: `Packaged/0.2.0/Windows/Floor101.exe`. ZIP:
`Packaged/0.2.0/101st-Floor-portable.zip`. Sin instalador MSI en esta entrega. La comprobación se realizó en este
equipo; la prueba en otro equipo sin Unreal sigue pendiente.

## Entrega 0.2.1 (2026-10-13)

Portable Windows **0.2.1 Shipping**, generado desde `fe48585` con árbol limpio: cotas interiores automáticas de cara a
cara, con su interruptor en Vista → Cotas interiores y en el toolset MCP (ADR-0034). Antes: 329 unitarios y 88 Unreal
Automation aprobados (`Saved/Automation/20261013-162629`). El ejecutable aprueba las 86 comprobaciones funcionales,
integridad SHA-256 y dependencias locales, con las cotas interiores en el plano SVG/PNG exportado
(`Saved/PackageChecks/20261013-170233-functional`), y la de inicio a 1920 × 1080
(`Saved/PackageChecks/20261013-170311-home`). Ejecutable: `Packaged/0.2.1/Windows/Floor101.exe`. ZIP:
`Packaged/0.2.1/101st-Floor-portable.zip`. Sin instalador MSI en esta entrega. La comprobación se realizó en este
equipo; la prueba en otro equipo sin Unreal sigue pendiente.
