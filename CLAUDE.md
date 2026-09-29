# 101st Floor — reglas para Claude

Producto visible: **101st Floor**. Proyecto Unreal y módulo C++: `Floor101`. Namespace propio: `floor101`/`f101`.
Antes de una feature importante leer `docs/architecture.md` (módulos, dependencias, flujo) y los ADRs en `docs/adr/`.
Otros: build `docs/build.md`, render `docs/rendering-rtx.md`, MCP `docs/unreal-mcp.md`.

## Arquitectura (no romper sin ADR)
- Fuente de verdad: el modelo de 101st Floor (CAD Core). Unreal solo representa; Twinmotion solo presenta.
- CAD Core, Geometry Engine y Scene Model: C++ estándar, sin tipos de Unreal, sin MCP, testeables sin Unreal.
- Unidades persistentes: milímetros enteros (`int64_t`). `double` solo como cálculo temporal con cuantización explícita.
- Sin lógica de producto en Blueprints. Sin pasos manuales en el Editor: todo reproducible desde Git + `Scripts/`.
- La interacción vive en `f101::cad::EditController` (puro); Unreal solo traduce la entrada y muestra (ADR-0008).
- Uniones de muros y habitaciones se derivan de los muros en cada compilación; no se guardan (ADR-0009).
- MCP es herramienta de desarrollo, nunca lógica de producto. Llamadas MCP de Unreal siempre en serie.

## Entorno
- Windows 11 + RTX 5070 Ti + UE 5.8.x (Launcher). Scripts en PowerShell 7: `Scripts/*.ps1`.
- Nunca invocar `bash` desde `.ps1` (en PowerShell resuelve a WSL). Localizar el motor con `Scripts/Common.ps1`.
- Compilar: `.\Scripts\Build.ps1` (con el editor cerrado; abierto → Live Coding vía MCP). Validar render: `.\Scripts\CheckRendering.ps1`.
- Tests: `.\Scripts\Test.ps1` (arquitectura + unitarios CMake, segundos); `-Integration` añade Unreal Automation.
- Editor + MCP: `.\Scripts\RunEditorMcp.ps1`; comprobar: `.\Scripts\CheckMcp.ps1`. Detalle en `docs/unreal-mcp.md`.
- Para editar el documento por MCP: toolset `Floor101Editor.Floor101DocumentToolset` (paredes en mm, esquinas,
  habitaciones, deshacer/rehacer, guardar/abrir). Con PIE activo actúa sobre el documento de PIE; si no, sobre el del
  editor.
- Unreal MCP: descubrir con `list_toolsets`/`describe_toolset`, invocar con `call_tool`, verificar cada resultado.
  Commit antes de sesiones MCP largas o mutativas.
- Render: solo DX12/SM6; los formatos D3D11 están eliminados a propósito (un fallback debe fallar, no pasar desapercibido).

## Forma de trabajo
- Inspeccionar antes de modificar; compilar y ejecutar tests de verdad; no declarar éxito sin validación.
- No desactivar tests ni silenciar warnings para obtener verde; no desactivar features para esconder fallos.
- Decisiones importantes → ADR en `docs/adr/`. No crear ADRs para decisiones triviales.
- No empezar la siguiente fase o MVP sin que el usuario lo pida.
