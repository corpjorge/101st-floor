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

Portable Windows **0.1.9 Shipping** aprobado, generado desde `1cbc374` con árbol limpio: editor de cotas propio
(`DimensionEditor`), cortes de muro que conservan los huecos, divisiones que no desconectan ramas, traspaso conjunto de
acabados, nombres y rodapiés de habitación, medida real junto a cotas con texto propio y log de producto acotado.
Antes: 320 unitarios y 88 Unreal Automation aprobados (`Saved/Automation/20261009-201104`). El ejecutable aprueba las
86 comprobaciones funcionales, integridad SHA-256 y dependencias locales: `Saved/PackageChecks/20261009-201850-functional`.
Ejecutable: `Packaged/0.1.9/Windows/Floor101.exe`. ZIP: `Packaged/0.1.9/101st-Floor-portable.zip`.
La comprobación se realizó en este equipo; la prueba en otro equipo sin Unreal sigue pendiente.

## Entrega 0.2.0 (2026-10-10)

Portable Windows **0.2.0 Shipping**, generado desde `d39a033` con árbol limpio: interfaz Cota Cero en todo el estudio
(ADR-0033, `docs/ui-cota-cero.md`) sin cambios de funciones. Antes: 320 unitarios y 88 Unreal Automation aprobados
(`Saved/Automation/20261010-124543`). El ejecutable aprueba las 86 comprobaciones funcionales, integridad SHA-256 y
dependencias locales (`Saved/PackageChecks/20261010-131358-functional`) y la de inicio a 1920 × 1080
(`Saved/PackageChecks/20261010-131415-home`). Ejecutable: `Packaged/0.2.0/Windows/Floor101.exe`. ZIP:
`Packaged/0.2.0/101st-Floor-portable.zip`. Sin instalador MSI en esta entrega. La comprobación se realizó en este
equipo; la prueba en otro equipo sin Unreal sigue pendiente.

## Entrega 0.2.1 (2026-10-13)

Portable Windows **0.2.1 Shipping**, generado desde `f9a925b` con árbol limpio: cotas interiores automáticas de cara a
cara, con su interruptor en Vista → Cotas interiores y en el toolset MCP (ADR-0034). Antes: 329 unitarios y 88 Unreal
Automation aprobados (`Saved/Automation/20261013-162629`). El ejecutable aprueba las 86 comprobaciones funcionales,
integridad SHA-256 y dependencias locales, con las cotas interiores en el plano SVG/PNG exportado
(`Saved/PackageChecks/20261013-170233-functional`), y la de inicio a 1920 × 1080
(`Saved/PackageChecks/20261013-170311-home`). Ejecutable: `Packaged/0.2.1/Windows/Floor101.exe`. ZIP:
`Packaged/0.2.1/101st-Floor-portable.zip`. Sin instalador MSI en esta entrega. La comprobación se realizó en este
equipo; la prueba en otro equipo sin Unreal sigue pendiente.

## Entrega 0.2.2 (2026-10-13)

Portable Windows **0.2.2 Shipping**, generado desde `21c65aa` con árbol limpio: tamaño del nombre y del área de cada
habitación y cada texto arrastrable en el plano 2D (ADR-0035, `project.json` esquema 14). Antes: 339 unitarios y 90
Unreal Automation aprobados (`Saved/Automation/20261013-175148`). El ejecutable aprueba las 93 comprobaciones
funcionales, entre ellas arrastrar el nombre de la habitación estrecha, reducir sus textos y reabrirlos, con integridad
SHA-256 y dependencias locales (`Saved/PackageChecks/20261013-175030-functional`), y la de inicio a 1920 × 1080
(`Saved/PackageChecks/20261013-175051-home`). Ejecutable: `Packaged/0.2.2/Windows/Floor101.exe`. ZIP:
`Packaged/0.2.2/101st-Floor-portable.zip`. Sin instalador MSI en esta entrega. El panel del rótulo no se probó a mano en
la ventana. La comprobación se realizó en este equipo; la prueba en otro equipo sin Unreal sigue pendiente.

## Entrega 0.2.3 (2026-10-14)

Portable Windows **0.2.3 Shipping**, generado desde `3a1d979` con árbol limpio: los pilares ya no parpadean donde se
superponen a un muro o a un forjado; en 3D sobresalen `ColumnProudMm` (1 mm) de su huella y de su planta (ADR-0021),
sin cambiar el modelo ni el plano. Antes: 340 unitarios y 90 Unreal Automation aprobados
(`Saved/Automation/20261014-195459`). El ejecutable aprueba las 93 comprobaciones funcionales, integridad SHA-256 y
dependencias locales (`Saved/PackageChecks/20261014-201731-functional`) y la de inicio a 1920 × 1080
(`Saved/PackageChecks/20261014-201749-home`). Ejecutable: `Packaged/0.2.3/Windows/Floor101.exe`. ZIP:
`Packaged/0.2.3/101st-Floor-portable.zip`. Sin instalador MSI en esta entrega. El parpadeo corregido no se comprobó a
simple vista en la ventana. La comprobación se realizó en este equipo; la prueba en otro equipo sin Unreal sigue
pendiente.

## Entrega 0.2.4 (2026-10-20)

Portable Windows **0.2.4 Shipping** para validar el catálogo de 24 familias paramétricas de puertas y ventanas
(ADR-0036, `project.json` esquema 15). Incluye miniaturas, búsqueda, filtro de apertura y medidas antes de colocar.
Generado con UE 5.8.3 y MSVC 14.50 desde el árbol de trabajo basado en `252b26a`, con los cambios del catálogo sin
commit (`dirty: true` en `release-manifest.json`).

- Ejecutable para abrir: `Packaged/0.2.4/Windows/Floor101.exe`.
- Portable completo: `Packaged/0.2.4/101st-Floor-portable.zip` (390.458.613 bytes); conservar completa su carpeta `Windows`.
- SHA-256 del ZIP: `bf4c99e737687781fba13e60ba5b48e82862631d5e638ddda7f13a5633fe6fbd`, también en `Packaged/0.2.4/sha256.txt`.
- `Package.ps1 -Output Packaged/0.2.4 -Zip`: compilación, cocinado y archivo correctos. El ZIP contiene los 154 archivos
  del manifiesto y el propio manifiesto; sus dos ejecutables y el manifiesto coinciden por SHA-256 con la carpeta probada.
- `CheckPackage.ps1 -Package Packaged/0.2.4/Windows`: **103 comprobaciones funcionales correctas**, resultado final PASS,
  integridad del manifiesto y dependencias locales verificadas. Incluye el catálogo 12/12, colocación y persistencia de
  las dos clases de familia, exportación SVG/PNG/GLB, biblioteca, controles y Foto.
  Informe: `Saved/PackageChecks/20261020-102405-functional`.
- `CheckPackage.ps1 -Package Packaged/0.2.4/Windows -Check doors -ResX 1920 -ResY 1080`: PASS.
  Captura revisada: `Saved/PackageChecks/20261020-102502-doors/doors.png`.
- `CheckPackage.ps1 -Package Packaged/0.2.4/Windows -Check windows -ResX 800 -ResY 600`: PASS.
  Captura revisada: `Saved/PackageChecks/20261020-102532-windows/windows.png`.

El ejecutable se probó en este equipo sin cargar dependencias desde la instalación de Unreal Editor. El usuario confirmó
el funcionamiento del catálogo el 2026-10-20; la prueba en otro equipo sigue pendiente. Esta entrega es portable;
la versión 0.2.3 se conserva.

## Entrega 0.2.5 (2026-10-20)

Portable Shipping con iluminación puntual real vinculada a luminarias, elevación de mobiliario y controles de
encendido y lúmenes (ADR-0037, esquema 16). Los proyectos anteriores se abren; el nuevo proyecto iluminado requiere
esta versión. Ejecutable: `Packaged/0.2.5/Windows/Floor101.exe`; ZIP completo: `Packaged/0.2.5/101st-Floor-portable.zip`.

351 unitarios y 93 Unreal Automation aprobados (`Saved/Automation/20261020-165449`). El ejecutable real pasó
110 comprobaciones funcionales, integridad del manifiesto y dependencias locales
(`Saved/PackageChecks/20261020-170109-functional`), incluidas luces puntuales, unidades físicas y persistencia.
El paquete registra el árbol de trabajo basado en `4aaef8b` con cambios sin commit (`dirty: true`).
No se genera instalador MSI ni se sustituye la entrega 0.2.4. La validación se realizó en este equipo.

## Entrega 0.2.6 (2026-10-20)

Portable Shipping con cubiertas translúcidas por habitación y acabados independientes del vidrio de los huecos
(ADR-0038, esquema 17). Incluye teja opal ondulada, perfiles blancos, vidrio reflectivo y baldosa gris mate de garaje.
Ejecutable: `Packaged/0.2.6/Windows/Floor101.exe`; ZIP: `Packaged/0.2.6/101st-Floor-portable.zip`.

355 unitarios (987.971 aserciones) y 94 Unreal Automation aprobados (`Saved/Automation/20261020-230508`).
117 comprobaciones del ejecutable real, manifiesto y dependencias locales: PASS
(`Saved/PackageChecks/20261020-231049-functional`), incluida persistencia de cubiertas y material reflectivo cocinado.
El proyecto de vivienda real conserva su ruta de trabajo y se reabre con este ejecutable. El paquete registra cambios
sin commit; no se genera MSI. Materiales regenerados con `Scripts/GenerateAssets.ps1`. Validación en este equipo.

## Entrega 0.3.0 (2026-10-22), primera versión pública

Windows **0.3.0 Shipping** generado desde `c216a57` con árbol limpio: licencia MIT con avisos de terceros en el paquete,
actualizaciones firmadas desde GitHub Releases (ADR-0039), "Guardar imagen…" con diálogo en Foto y README público.
Antes: 355 unitarios y 100 Unreal Automation aprobados (`Saved/Automation/20261022-141355`). El ejecutable aprueba las
119 comprobaciones funcionales, integridad SHA-256 y dependencias locales
(`Saved/PackageChecks/20261022-140915-functional`) y la de inicio a 1920 × 1080
(`Saved/PackageChecks/20261022-140948-home`). Instalador: el `Setup.exe` contiene el MSI exacto y el runtime de Visual
C++; instalación aislada, accesos directos, arranque y desinstalación conservando un proyecto del usuario
(`Packaged/InstallerChecks/20261022-141203`). Actualizador: el ejecutable real, haciéndose pasar por 0.0.1 con el
release local firmado, verifica la firma con la clave del proyecto, descarga el `Setup.exe` real (368 MB) y comprueba
tamaño y SHA-256 antes de detenerse sin instalar (`Saved/PackageChecks/20261022-141313-update`). Publicado como
[v0.3.0](https://github.com/corpjorge/101st-floor/releases/tag/v0.3.0) (etiqueta en `c216a57`); el mismo ejecutable,
consultando el GitHub real, encuentra la versión, sigue la redirección de descarga y verifica firma, tamaño y SHA-256
(`Saved/PackageChecks/20261022-142549-update`). Archivos del release
en `Packaged/0.3.0/Release` con `SHA256SUMS.txt`. Sin firma Authenticode: SmartScreen avisa en el primer arranque del
instalador. La prueba en otro equipo sin Unreal sigue pendiente.
