# 101st Floor

**Estudio de diseño arquitectónico para Windows:** dibuja la casa con medidas exactas, recórrela en tiempo real con
Unreal Engine 5 (Lumen y trazado de rayos), saca fotos con path tracing, genera los planos acotados y envíala a
Twinmotion.

*An architectural design studio for Windows: precise CAD modelling, real-time walkthroughs with Unreal Engine 5,
path-traced photos, dimensioned plans and Twinmotion export. Free and open source (MIT).*

[![Última versión](https://img.shields.io/github/v/release/corpjorge/101st-floor?label=versi%C3%B3n)](https://github.com/corpjorge/101st-floor/releases/latest)
[![Licencia MIT](https://img.shields.io/badge/licencia-MIT-blue)](LICENSE)
![Windows 11 x64](https://img.shields.io/badge/Windows%2011-x64-0078d6)

![Una casa en Modo Foto](docs/images/ui-cota-cero/photo-1920.jpg)

## Qué puedes hacer

- **Muros con medidas exactas**: clic, vista previa y longitud escrita (`355`, `3.55 m`, `3550 mm`); esquinas limpias
  en L, T y X; habitaciones con su área neta que se recalculan al momento.
- **Puertas y ventanas** de un catálogo de 24 diseños paramétricos (abatibles, de dos hojas, correderas empotradas,
  fijas), con marco, hojas, manillas y vidrio.
- **Varias plantas** con forjados, **pilares** e **instalaciones** de electricidad, agua, aguas negras y gas vistas
  con rayos X.
- **Materiales**: 80 acabados por categoría y 14 texturas fotográficas, por cara de muro, suelo, techo y carpintería;
  rodapiés por habitación.
- **Muebles**: una biblioteca de 53 muebles con imagen, muebles paramétricos e importación de tus propios modelos
  (glTF, GLB, OBJ) y texturas (PNG, JPEG).
- **Planos 2D** editables con cotas automáticas y manuales, cotas interiores y nombres de habitación, exportables a
  SVG y PNG.
- **Recorrer** la casa con WASD y **Modo Foto** con path tracing (PNG, o EXR en pantallas HDR).
- **Exportar a Twinmotion** la casa entera como `.glb` con sus materiales y texturas.
- Deshacer y rehacer todo, y proyectos en un `project.json` legible.

| | |
|---|---|
| ![Pantalla de inicio](docs/images/ui-cota-cero/home-1920.jpg) | ![Diseñar: un muro seleccionado con sus medidas](docs/images/ui-cota-cero/selection-1920.jpg) |
| ![Materiales por cara de muro](docs/images/ui-cota-cero/materials-1920.jpg) | ![Biblioteca de muebles](docs/images/ui-cota-cero/furniture-1920.jpg) |
| ![Catálogo de puertas](docs/images/opening-catalogue/doors-1920.png) | ![Recorrer la casa](docs/images/ui-cota-cero/walk-1920.jpg) |

## Descargar e instalar

Descarga la última versión desde **[Releases](https://github.com/corpjorge/101st-floor/releases/latest)**. No hace falta
instalar Unreal Engine, Visual Studio ni nada más.

- **Instalador (recomendado):** `101st-Floor-<versión>-Setup.exe`. Instala la aplicación para tu usuario, con accesos
  directos en el Escritorio y en Inicio, y se actualiza sola. La primera vez puede pedir permiso de administrador para
  el runtime de Visual C++ de Microsoft, si tu equipo no lo tiene.
- **Portable:** `101st-Floor-<versión>-portable.zip`. Extráelo entero y abre `Windows\Floor101.exe`. No se actualiza
  solo: descarga la versión nueva cuando quieras.

Windows SmartScreen puede mostrar *"Windows protegió tu PC"* porque el instalador no está firmado con un certificado
comercial: pulsa **Más información → Ejecutar de todas formas**. Puedes comprobar cualquier descarga con
`SHA256SUMS.txt`, que acompaña a cada versión.

**Requisitos:** Windows 11 x64; GPU con DirectX 12 (Shader Model 6) y trazado de rayos por hardware (DXR) para Lumen y
el Modo Foto, con drivers actualizados. HDR es opcional y necesita una pantalla compatible.

**Desinstalar:** desde Configuración de Windows → Aplicaciones. Tus proyectos y preferencias no se borran.

## Actualizaciones

La copia instalada pregunta a GitHub al arrancar si hay una versión nueva (puedes desactivarlo en
**Opciones → Acerca de**). Si la hay, aparece en la pantalla de inicio y en la barra: **Descargar** la baja en segundo
plano y **Instalar y reiniciar** guarda tu trabajo si quieres, instala la versión nueva y vuelve a abrir la aplicación.

Cada versión trae una descripción firmada con la clave del proyecto; la aplicación solo instala un instalador cuyo
tamaño y huella SHA-256 coinciden con esa descripción firmada, y nunca una versión anterior. No se envía nada tuyo ni de
tus proyectos. Detalle: [ADR-0039](docs/adr/ADR-0039-public-release-and-updates.md).

## Primeros pasos

1. **Nuevo proyecto** en la pantalla de inicio.
2. **B** (Pared): clic donde empieza el muro, escribe la longitud (`400` son 400 cm) y pulsa Enter; sigue con el
   siguiente. Al cerrar el contorno aparece la habitación con su área.
3. **P** / **O**: elige un diseño de puerta o ventana en el panel y haz clic sobre un muro.
4. **M** (Materiales) y **G** (Muebles): pinta caras y suelos, coloca muebles.
5. **Tab**: plano 2D con cotas; **H** añade cotas; exporta el plano en SVG o PNG desde Documento.
6. **N** (Recorrer): muévete con WASD. **R** (Foto): espera a que se afine y pulsa **Guardar imagen…**.
7. **Ctrl+S** guarda el proyecto.

Todas las teclas y gestos: **[controles](docs/controls.md)**.

## Guías

[Controles](docs/controls.md) · [Catálogo de puertas y ventanas](docs/opening-catalogue.md) ·
[Cotas](docs/dimensions.md) · [Nombres de habitaciones](docs/room-names.md) · [Rodapiés](docs/baseboards.md) ·
[Navegación y cámara](docs/camera-navigation.md) · [Pantalla](docs/display.md) · [HDR](docs/hdr.md) ·
[División de paredes](docs/wall-splitting.md) · [Inicio y diagnóstico](docs/startup.md)

## Para desarrolladores

101st Floor separa un núcleo CAD en C++ estándar (modelo paramétrico en milímetros enteros, geometría, planos,
exportadores), probado sin Unreal, de la aplicación en Unreal Engine 5.8, que solo representa y traduce la entrada.
Empieza por la [arquitectura](docs/architecture.md) y las [decisiones (ADR)](docs/adr/README.md).

**Compilar** necesita Windows 11 x64, Unreal Engine 5.8.x (Epic Games Launcher, con su EULA aceptado), Visual Studio
2026 con MSVC 14.50 y Windows SDK 10.0.26100, PowerShell 7 y Git LFS ([detalle](docs/build.md)):

```powershell
git lfs install
git clone https://github.com/corpjorge/101st-floor.git; cd 101st-floor
.\Scripts\Setup.ps1            # comprueba el entorno
.\Scripts\Build.ps1            # compila Floor101Editor (Win64 Development)
.\Scripts\Test.ps1             # arquitectura + pruebas unitarias sin Unreal (-Integration: + Unreal Automation)
.\Scripts\Run.ps1              # abre 101st Floor (-Demo:$false: documento vacío)
.\Scripts\CheckRendering.ps1   # valida DX12, SM6, ray tracing, Lumen HWRT, VSM, Nanite y el path tracing de Foto
.\Scripts\GenerateAssets.ps1   # regenera los materiales de acabados y vidrio (Content/Floor101) desde código
.\Scripts\GenerateLibrary.ps1  # regenera los muebles de la biblioteca (Library/furniture) con Blender
.\Scripts\CheckGltf.ps1 -Path casa.glb  # segunda opinión de una exportación: la importa y renderiza en Blender
```

**Distribuir:** `Package.ps1`, `CheckPackage.ps1`, `Installer.ps1` y `Release.ps1`; el proceso completo, con la clave
de firma de las actualizaciones, está en [publicar una versión](docs/releasing.md) y
[distribución Windows](docs/distribution.md).

Más: [núcleo CAD](docs/cad-core.md) · [geometría](docs/geometry.md) · [escena](docs/scene-model.md) ·
[planos](docs/drawing.md) · [exportadores](docs/exporters.md) · [integración con Unreal](docs/unreal-integration.md) ·
[render](docs/rendering-rtx.md) · [Unreal MCP](docs/unreal-mcp.md) · [biblioteca](Library/README.md)

## Licencia

101st Floor es software libre con licencia **[MIT](LICENSE)**.

101st Floor uses the Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United
States of America and elsewhere. Unreal® Engine, Copyright 1998 – 2026, Epic Games, Inc. All rights reserved. El motor
se rige por el [EULA de Unreal Engine](https://www.unrealengine.com/eula); la licencia MIT no se aplica a él.

Componentes y recursos de terceros (nlohmann/json, WiX, runtime de Visual C++, texturas CC0 de Poly Haven):
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
