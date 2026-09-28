# 101st Floor

Aplicación de diseño arquitectónico: construcción visual tipo "videojuego CAD" con precisión dimensional,
modelo paramétrico propio y visualización en tiempo real con Unreal Engine 5.8 (Lumen + ray tracing por hardware).

> Estado: Fase B completada — proyecto Unreal `Floor101` creado y ruta gráfica DX12/SM6/HWRT/Lumen validada
> en ejecución (14/14 checks). Siguiente: Fase C (Unreal MCP). MVP 0 pendiente.

## Requisitos
Windows 11 x64, GPU con DXR 1.1 + SM6.6 (referencia: RTX 5070 Ti), Unreal Engine 5.8.x (Epic Games Launcher),
Visual Studio 2026 con MSVC 14.50 y Windows SDK 10.0.26100. Detalle: [docs/build.md](docs/build.md).

## Uso
```powershell
.\Scripts\Build.ps1            # compila Floor101Editor (Win64 Development)
.\Scripts\CheckRendering.ps1   # valida DX12, SM6, GPU, ray tracing, Lumen HWRT, VSM, Nanite desde los logs
```

## Documentación
- [docs/build.md](docs/build.md) — toolchain, compilación y localización del motor
- [docs/rendering-rtx.md](docs/rendering-rtx.md) — configuración de render y cómo se verifica
