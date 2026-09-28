# Render y RTX

La configuración vive en `Config/DefaultEngine.ini` y se valida en ejecución con `Scripts/CheckRendering.ps1`.
Regla: ninguna opción se da por activa porque aparezca en un `.ini`; se comprueba en logs, APIs del motor y ProfileGPU.

## Configuración (Modo Diseño)

| Ajuste | Valor | Por qué |
|---|---|---|
| `DefaultGraphicsRHI` | `DX12` | RHI principal. |
| `D3D12TargetedShaderFormats` | solo `PCD3D_SM6` | SM6 requerido por Nanite/VSM/Lumen HWRT y bindless. |
| `D3D11TargetedShaderFormats` | vacío (se elimina `PCD3D_SM5`) | Hace imposible un fallback silencioso a DX11: si DX12 falla, el arranque falla. |
| `r.RayTracing` | `True` | Ray tracing por hardware (DXR). Es read-only en runtime: requiere reiniciar. |
| `r.SkinCache.CompileShaders` | `True` | Obligatorio con ray tracing (el motor aborta si falta). |
| `r.RayTracing.RayTracingProxies.ProjectEnabled` | `True` | Default de UE 5.8 para proyectos nuevos. |
| `r.DynamicGlobalIlluminationMethod` | `1` (Lumen) | GI dinámica. |
| `r.ReflectionMethod` | `1` (Lumen) | Reflexiones Lumen. |
| `r.Lumen.HardwareRayTracing` | `True` | Default del motor ya es 1; explícito para versionar la intención. |
| `r.GenerateMeshDistanceFields` | `True` | Mantiene disponible el ray tracing por software de Lumen como fallback. |
| `r.AllowStaticLighting` | `False` | Sin lightmaps: todo dinámico (paredes que se regeneran). |
| `r.Shadow.Virtual.Enable` | `1` | Virtual Shadow Maps. |
| `r.Substrate` | `True` | Sistema de materiales por defecto en proyectos nuevos de UE 5.8. |
| Nanite | default del motor (activo) | Se usará cuando aplique (assets densos); las paredes CAD son geometría simple. |
| Path Tracing | default del motor (`r.PathTracing=1`) | Disponible para Modo Foto; no se usa en Modo Diseño. |

Los valores replican lo que el asistente de proyectos de UE 5.8.3 escribe para un proyecto nuevo con ray tracing
(`GameProjectUtils.cpp`: `AddLumenConfigValues`, `AddRaytracingConfigValues`, `AddSubstrateConfigValues`,
`AddNewProjectDefaultShadowConfigValues`, `AddPostProcessingConfigValues`), más la eliminación de SM5/D3D11.

## Cómo se valida
`Scripts/CheckRendering.ps1` lanza `UnrealEditor.exe Floor101.uproject <mapa> -game -F101RenderCheck`.
`Source/Floor101/Private/RenderCheck.cpp` (solo lectura, sin cambiar ajustes):
1. espera a que el mundo esté en juego y a que termine la compilación de shaders;
2. calienta N frames (convergencia de Lumen) y muestrea tiempo de frame y de GPU;
3. registra el estado real (`F101RenderCheck: clave=valor`): RHI, feature level, shader platform, adaptador,
   driver, capacidades RHI de ray tracing, `IsRayTracingEnabled`, Nanite, VSM, Lumen y cvars efectivos;
4. ejecuta `ProfileGPU` y sale.

Para re-analizar un log sin relanzar el motor: `.\Scripts\CheckRendering.ps1 -AnalyzeLog <ruta>`.

El script cruza esos valores con mensajes propios del motor:

| Comprobación | Evidencia |
|---|---|
| DX12 activo | `LogRHI: RHI D3D12 with Feature Level SM6 is supported and will be used` + `rhi=D3D12` |
| Sin fallback | ausencia de `attempting to fall back` y de `Loading RHI module D3D11RHI` |
| SM6 | `shader_platform=PCD3D_SM6` |
| GPU | `adapter` contiene `RTX 5070 Ti` (`Chosen D3D12 Adapter Id`) |
| SM6.6 atomics | `LogD3D12RHI: Shader Model 6.6 atomic64 is supported` |
| HWRT en RHI | `LogD3D12RHI: D3D12 ray tracing tier 1.1 and bindless resources are supported` |
| Ray tracing activo | `ray_tracing_enabled=1` (+ `LogRendererCore: Ray tracing is enabled`) |
| Lumen HWRT en ejecución | condición de `Lumen::UseHardwareRayTracing` cumplida **y** pases `HardwareRayTracing` en ProfileGPU |
| Errores | ninguna línea `Error`/`Fatal` de RHI, shaders, renderer o materiales |
| Rendimiento | GPU media ≤ 33,3 ms (≥ 30 FPS) en 1920×1080; por encima es WARN |

## Resultados (2026-09-28, UE 5.8.3, driver 617.14)

`.\Scripts\CheckRendering.ps1` → **14/14 PASS**, exit 0. Mapa `/Engine/Maps/Templates/Template_Default`, ventana 1920×1080.

| Comprobación | Resultado |
|---|---|
| RHI | D3D12, feature level SM6, shader platform `PCD3D_SM6`, DirectX Agility SDK cargado |
| Fallback | ninguno (sin D3D11 ni "fall back") |
| GPU | NVIDIA GeForce RTX 5070 Ti, adaptador 0, driver 617.14 |
| Capacidades | DXR tier 1.1 + bindless (`Configuration=RayTracing`), ray tracing shaders + inline, SM6.6 atomic64, mesh shader tier 1.0 |
| Extensiones NVIDIA | `NVIDIA Ray Tracing Cluster Operations supported and enabled`, `NVIDIA Extended ray tracing pipeline state applied` (activadas por el motor, sin configuración propia) |
| Ray tracing | `Ray tracing is enabled (dynamic)` (`r.RayTracing=1`, `r.RayTracing.EnableOnDemand=1`) |
| Lumen | GI y reflexiones Lumen activas; HWRT esperado = 1 |
| Lumen HWRT en ejecución | 9 pases HWRT en ProfileGPU: 8 en la cola **Compute** (iluminación directa, radiosidad, radiance cache, screen probes, inline) + 1 en **Graphics** (reflexiones) |
| VSM / Nanite | activos |
| Errores RHI/shaders | 0 |
| Tiempos | GPU 3,5 ms media (p95 4,0 ms); frame 4,8–5,0 ms media; TSR (`r.AntiAliasingMethod=4`), resolución interna automática 1400×788 → 1920×1080 |

Los tiempos son una **línea base** con una escena trivial (suelo + cielo): no representan una vivienda.
La primera ejecución tardó ~10 min compilando shaders globales (8 hilos); con la DDC caliente la validación
completa tarda ~20–30 s.

Lumen ejecuta por defecto la GI y la iluminación de escena en *async compute* (`r.Lumen.AsyncCompute=1`,
`r.Lumen.DiffuseIndirect.AsyncCompute=1`) y las reflexiones en la cola gráfica. ProfileGPU imprime una tabla por
cola (Compute antes que Graphics); el script las analiza todas.

Warnings conocidos no relacionados con render: `LogEditorDataStorageUI` (UI del editor inexistente al ejecutar el
binario del editor con `-game`).

## Notas
- El monitor virtual de Meta hace que la RTX aparezca dos veces en DXGI (adaptadores 0 y 2). El motor elige el 0.
- UE informa "shader model 6.7" como máximo del adaptador aunque el driver expone 6.8: es el límite que consulta el
  motor, y SM6.6 (lo requerido) está cubierto.
- Modo Foto (Path Tracer / Lumen máxima calidad) se configurará cuando se implemente; no forma parte del MVP 0.
