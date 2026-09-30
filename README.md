# 101st Floor

Aplicación de diseño arquitectónico: construcción visual tipo "videojuego CAD" con precisión dimensional,
modelo paramétrico propio y visualización en tiempo real con Unreal Engine 5.8 (Lumen + ray tracing por hardware).

> Estado: **MVP 10 completo** ([informe](docs/mvp10-report.md); [MVP 9](docs/mvp9-report.md), [MVP 8](docs/mvp8-report.md), [MVP 7](docs/mvp7-report.md), [MVP 6](docs/mvp6-report.md),
> [MVP 5](docs/mvp5-report.md), [MVP 4](docs/mvp4-report.md), [MVP 3](docs/mvp3-report.md), [MVP 2](docs/mvp2-report.md),
> [MVP 1](docs/mvp1-report.md), [MVP 0](docs/mvp0-report.md)). Muros con medidas exactas (clic, vista previa, longitud escrita, Enter), esquinas limpias
> en L/T/X, habitaciones con suelo y área neta que se actualizan en vivo, arrastre de esquinas compartidas, puertas y
> ventanas con marco, hojas, manillas y vidrio (abatibles, de dos hojas, correderas, fijas; abiertas en ángulo, con su
> símbolo de apertura) que se arrastran con el ratón y respetan las uniones de muros, varias plantas con forjados, acabados
> (pintura, ladrillo, hormigón, azulejo, madera...) por cara de muro, suelo, techo y carpintería, selección,
> propiedades, deshacer/rehacer y `project.json`, planos 2D editables con cotas y habitaciones exportables a SVG y PNG,
> mobiliario paramétrico (camas, sofá, mesa, sillas, armario, cocina, baño) que se coloca, mueve y gira con el ratón
> apoyándose en los muros, e importación de assets propios (texturas PNG/JPEG como acabados y modelos glTF/GLB/OBJ
> como muebles, copiados junto al proyecto), sobre Unreal con Lumen HWRT. La lógica vive en un núcleo C++ puro
> probado sin Unreal. Siguiente MVP: pendiente de decisión.

## Controles
W: muro · P: puerta · O: ventana · M: pintar · A: amueblar · V: seleccionar · escribir `355` / `3.55 m` / `3550 mm` + Enter: muro exacto (sin unidad = cm) · Supr: eliminar ·
Ctrl+Z / Ctrl+Y: deshacer / rehacer · Ctrl+S / Ctrl+O: guardar / abrir · Esc o clic derecho: cancelar ·
arrastrar un asa amarilla: mover la esquina · con un hueco seleccionado, arrastrarlo: moverlo; arrastrar sus asas:
cambiar el ancho · Espacio: bisagras al otro lado (al colocar, la puerta abre hacia donde apuntas) o girar 90° el mueble · con un mueble seleccionado, arrastrarlo: moverlo · derecho arrastrando: orbitar · central: desplazar · rueda: zoom ·
F: encuadrar · T: vista cenital · Tab: plano 2D (Tab otra vez: 3D) · "Plano: SVG / PNG": exportar el plano ·
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
.\Scripts\CheckRendering.ps1   # valida DX12, SM6, GPU, ray tracing, Lumen HWRT, VSM, Nanite desde los logs
.\Scripts\RunEditorMcp.ps1     # abre Unreal Editor con el servidor MCP (desarrollo con Claude Code)
.\Scripts\CheckMcp.ps1         # prueba de humo del MCP (handshake, toolsets, operación read-only)
.\Scripts\GenerateAssets.ps1   # regenera los materiales de acabados y vidrio (Content/Floor101) desde código
```

## Documentación
- [docs/architecture.md](docs/architecture.md) — módulos, dependencias, unidades, flujo de datos, tests ([ADRs](docs/adr/README.md))
- [docs/cad-core.md](docs/cad-core.md) · [docs/geometry.md](docs/geometry.md) · [docs/scene-model.md](docs/scene-model.md) · [docs/drawing.md](docs/drawing.md) · [docs/unreal-integration.md](docs/unreal-integration.md) — cada capa
- [docs/build.md](docs/build.md) — toolchain, compilación y localización del motor
- [docs/rendering-rtx.md](docs/rendering-rtx.md) — configuración de render y cómo se verifica
- [docs/unreal-mcp.md](docs/unreal-mcp.md) — Unreal MCP: plugins, arranque, seguridad, serialización
