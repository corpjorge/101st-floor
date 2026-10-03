# 101st Floor

Aplicación de diseño arquitectónico: construcción visual tipo "videojuego CAD" con precisión dimensional,
modelo paramétrico propio y visualización en tiempo real con Unreal Engine 5.8 (Lumen + ray tracing por hardware).

> Estado: **MVP 13 completo** ([informe](docs/mvp13-report.md); [MVP 12](docs/mvp12-report.md), [MVP 11](docs/mvp11-report.md), [MVP 10](docs/mvp10-report.md), [MVP 9](docs/mvp9-report.md), [MVP 8](docs/mvp8-report.md), [MVP 7](docs/mvp7-report.md), [MVP 6](docs/mvp6-report.md),
> [MVP 5](docs/mvp5-report.md), [MVP 4](docs/mvp4-report.md), [MVP 3](docs/mvp3-report.md), [MVP 2](docs/mvp2-report.md),
> [MVP 1](docs/mvp1-report.md), [MVP 0](docs/mvp0-report.md)). Muros con medidas exactas (clic, vista previa, longitud escrita, Enter), esquinas limpias
> en L/T/X, habitaciones con suelo y área neta que se actualizan en vivo, arrastre de esquinas compartidas, puertas y
> ventanas con marco, hojas, manillas y vidrio (abatibles, de dos hojas, correderas, fijas; el tipo se elige antes de
> colocarlas; abiertas en ángulo, con su símbolo de apertura) que se arrastran con el ratón y respetan las uniones de
> muros, varias plantas con forjados, 80 acabados por categoría (pinturas, baldosas, laminado, parquet en espiga,
> piedra, moquetas...) más fotografías reales, por cara de muro, suelo (pestaña Suelos en Pintar), techo y carpintería,
> selección, propiedades, deshacer/rehacer y `project.json`, planos 2D editables con cotas y habitaciones exportables
> a SVG y PNG, mobiliario paramétrico y una biblioteca integrada de 53 muebles con imagen por categoría (camas, sillas,
> sofás, mesas, almacenaje, cocina, baño, luces, decoración), importación de assets propios (texturas PNG/JPEG y
> modelos glTF/GLB/OBJ, copiados junto al proyecto), pilares rectangulares y redondos, instalaciones de electricidad
> (verde), agua (blanco), aguas negras (amarillo) y gas (dorado) dibujadas por tramos y vistas con rayos X, Modo Foto
> con path tracing y exportación de la casa entera a
> Twinmotion (`.glb` con materiales y texturas), sobre Unreal con Lumen HWRT. La lógica vive en un núcleo C++ puro
> probado sin Unreal. Siguiente MVP: pendiente de decisión.

## Controles
W: muro · P: puerta · O: ventana (el tipo se elige en el panel antes de colocarla) · M: pintar (pestañas Paredes / Suelos) · A: amueblar (pestañas por categoría) · C: pilar · I: instalación (clic por punto, doble clic o Enter para terminar; tipo y altura en el panel) · X: rayos X (ver las instalaciones; la fila "Redes" elige electricidad, agua, aguas negras o gas, también en el plano y su exportación) · V: seleccionar · R: Modo Foto (path tracing; "Guardar imagen") · escribir `355` / `3.55 m` / `3550 mm` + Enter: muro exacto (sin unidad = cm) · Supr: eliminar ·
Ctrl+Z / Ctrl+Y: deshacer / rehacer · Ctrl+S / Ctrl+Shift+S / Ctrl+O: guardar / guardar como (eliges carpeta y nombre) / abrir · Esc o clic derecho: cancelar ·
arrastrar un asa amarilla: mover la esquina · con un hueco seleccionado, arrastrarlo: moverlo; arrastrar sus asas:
cambiar el ancho · Espacio: bisagras al otro lado (al colocar, la puerta abre hacia donde apuntas) o girar 90° el mueble o el pilar · con un mueble, un pilar o una tubería seleccionados, arrastrarlo: moverlo (las asas de una tubería mueven un codo) · derecho arrastrando: orbitar · central: desplazar · rueda: zoom ·
F: encuadrar · T: vista cenital · Tab: plano 2D (Tab otra vez: 3D) · "Plano: SVG / PNG": exportar el plano ·
"Twinmotion (.glb)…": exportar la casa entera donde elijas (se recuerdan las últimas carpetas) (en Twinmotion: Importar → el `.glb`) ·
"Importar textura…" (en Pintar) / "Importar modelo…" (en Amueblar): traer una imagen o un modelo 3D al proyecto ·
RePág / AvPág: planta de arriba / de abajo · L: ver todas las plantas. Detalle en
[docs/unreal-integration.md](docs/unreal-integration.md#controles).

## Requisitos
Windows 11 x64, GPU con DXR 1.1 + SM6.6 (referencia: RTX 5070 Ti), Unreal Engine 5.8.x (Epic Games Launcher),
Visual Studio 2026 con MSVC 14.50 y Windows SDK 10.0.26100. Detalle: [docs/build.md](docs/build.md).

## Uso
```powershell
git clone https://github.com/corpjorge/101st-floor.git; cd 101st-floor
.\Scripts\Setup.ps1            # comprueba el entorno
.\Scripts\Build.ps1            # compila Floor101Editor (Win64 Development)
.\Scripts\Test.ps1             # arquitectura + tests unitarios sin Unreal (-Integration: + Unreal Automation)
.\Scripts\Run.ps1              # abre 101st Floor con la pared de referencia (-Demo:$false: documento vacío)
.\Scripts\CreateDesktopShortcut.ps1  # acceso directo "101st Floor" en el escritorio que ejecuta Run.ps1
.\Scripts\CheckRendering.ps1   # valida DX12, SM6, GPU, ray tracing, Lumen HWRT, VSM, Nanite y el path tracing del Modo Foto
.\Scripts\CheckHdr.ps1         # consulta HDR10/EDID, señal y luminancia declarada del monitor
.\Scripts\CheckHdrOutput.ps1   # valida HDR/SDR real (-EnableWindowsHdrForTest: activa Windows HDR temporalmente)
.\Scripts\RunEditorMcp.ps1     # abre Unreal Editor con el servidor MCP (desarrollo con Claude Code)
.\Scripts\CheckMcp.ps1         # prueba de humo del MCP (handshake, toolsets, operación read-only)
.\Scripts\GenerateAssets.ps1   # regenera los materiales de acabados y vidrio (Content/Floor101) desde código
.\Scripts\GenerateLibrary.ps1  # regenera los muebles de la biblioteca (Library/furniture) con Blender
.\Scripts\CheckGltf.ps1 -Path casa.glb  # segunda opinión de una exportación: la importa y renderiza en Blender
```

La biblioteca integrada vive en [Library/](Library/README.md): muebles modelados por código y texturas CC0 de
Poly Haven. Sus binarios van por Git LFS (`git lfs install` antes de clonar).

## Documentación
- [docs/architecture.md](docs/architecture.md) — módulos, dependencias, unidades, flujo de datos, tests ([ADRs](docs/adr/README.md))
- [docs/cad-core.md](docs/cad-core.md) · [docs/geometry.md](docs/geometry.md) · [docs/scene-model.md](docs/scene-model.md) · [docs/drawing.md](docs/drawing.md) · [docs/exporters.md](docs/exporters.md) · [docs/unreal-integration.md](docs/unreal-integration.md) — cada capa
- [docs/build.md](docs/build.md) — toolchain, compilación y localización del motor
- [docs/rendering-rtx.md](docs/rendering-rtx.md) — configuración de render y cómo se verifica
- [docs/hdr.md](docs/hdr.md) — botón HDR, monitor comprobado y fotos EXR/PNG
- [docs/unreal-mcp.md](docs/unreal-mcp.md) — Unreal MCP: plugins, arranque, seguridad, serialización
