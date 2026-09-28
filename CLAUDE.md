# 101st Floor — reglas para Claude

Producto visible: **101st Floor**. Proyecto Unreal y módulo C++: `Floor101`. Namespace propio: `floor101`/`f101`.
Detalle y decisiones en `docs/` (build: `docs/build.md`, render: `docs/rendering-rtx.md`, ADRs: `docs/adr/`).

## Arquitectura (no romper sin ADR)
- Fuente de verdad: el modelo de 101st Floor (CAD Core). Unreal solo representa; Twinmotion solo presenta.
- CAD Core, Geometry Engine y Scene Model: C++ estándar, sin tipos de Unreal, sin MCP, testeables sin Unreal.
- Unidades persistentes: milímetros enteros (`int64_t`). `double` solo como cálculo temporal con cuantización explícita.
- Sin lógica de producto en Blueprints. Sin pasos manuales en el Editor: todo reproducible desde Git + `Scripts/`.
- MCP es herramienta de desarrollo, nunca lógica de producto. Llamadas MCP de Unreal siempre en serie.

## Entorno
- Windows 11 + RTX 5070 Ti + UE 5.8.x (Launcher). Scripts en PowerShell 7: `Scripts/*.ps1`.
- Nunca invocar `bash` desde `.ps1` (en PowerShell resuelve a WSL). Localizar el motor con `Scripts/Common.ps1`.
- Compilar: `.\Scripts\Build.ps1` (con el editor cerrado; abierto → Live Coding vía MCP). Validar render: `.\Scripts\CheckRendering.ps1`.
- Editor + MCP: `.\Scripts\RunEditorMcp.ps1`; comprobar: `.\Scripts\CheckMcp.ps1`. Detalle en `docs/unreal-mcp.md`.
- Unreal MCP: descubrir con `list_toolsets`/`describe_toolset`, invocar con `call_tool`, verificar cada resultado.
  Commit antes de sesiones MCP largas o mutativas.
- Render: solo DX12/SM6; los formatos D3D11 están eliminados a propósito (un fallback debe fallar, no pasar desapercibido).

## Forma de trabajo
- Inspeccionar antes de modificar; compilar y ejecutar tests de verdad; no declarar éxito sin validación.
- No desactivar tests ni silenciar warnings para obtener verde; no desactivar features para esconder fallos.
- Decisiones importantes → ADR en `docs/adr/`. No crear ADRs para decisiones triviales.
- No empezar la siguiente fase o MVP sin que el usuario lo pida.
