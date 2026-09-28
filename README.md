# 101st Floor

Aplicación de diseño arquitectónico: construcción visual tipo "videojuego CAD" con precisión dimensional,
modelo paramétrico propio y visualización en tiempo real con Unreal Engine 5.8 (Lumen + ray tracing por hardware).

> Estado: **MVP 1 completo** ([informe](docs/mvp1-report.md); MVP 0: [informe](docs/mvp0-report.md)). Herramienta de
> muro interactiva (clic, vista previa, longitud escrita, Enter → muro exacto), selección, borrar, longitud, espesor y
> altura, arrastre de extremos, deshacer/rehacer y `project.json`, sobre Unreal con Lumen HWRT. La lógica vive en un
> núcleo C++ puro probado sin Unreal. Siguiente MVP: pendiente de decisión.

## Controles
W: muro · V: seleccionar · escribir `355` / `3.55 m` / `3550 mm` + Enter: muro exacto (sin unidad = cm) · Supr: eliminar ·
Ctrl+Z / Ctrl+Y: deshacer / rehacer · Ctrl+S / Ctrl+O: guardar / abrir · Esc o clic derecho: cancelar ·
derecho arrastrando: orbitar · central: desplazar · rueda: zoom · F: encuadrar. Detalle en
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
.\Scripts\CheckRendering.ps1   # valida DX12, SM6, GPU, ray tracing, Lumen HWRT, VSM, Nanite desde los logs
.\Scripts\RunEditorMcp.ps1     # abre Unreal Editor con el servidor MCP (desarrollo con Claude Code)
.\Scripts\CheckMcp.ps1         # prueba de humo del MCP (handshake, toolsets, operación read-only)
```

## Documentación
- [docs/architecture.md](docs/architecture.md) — módulos, dependencias, unidades, flujo de datos, tests ([ADRs](docs/adr/README.md))
- [docs/cad-core.md](docs/cad-core.md) · [docs/geometry.md](docs/geometry.md) · [docs/scene-model.md](docs/scene-model.md) · [docs/unreal-integration.md](docs/unreal-integration.md) — cada capa
- [docs/build.md](docs/build.md) — toolchain, compilación y localización del motor
- [docs/rendering-rtx.md](docs/rendering-rtx.md) — configuración de render y cómo se verifica
- [docs/unreal-mcp.md](docs/unreal-mcp.md) — Unreal MCP: plugins, arranque, seguridad, serialización
