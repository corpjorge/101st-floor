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
| Path Tracing | default del motor (`r.PathTracing=1`) | Modo Foto (R, ADR-0018); no se usa en Modo Diseño. |

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
Con `-Demo` se carga la pared de referencia (`-F101Demo`), para validar también la geometría generada.
Con la pared, además, `Finish material loaded` exige que carguen el material de los acabados (`M_Floor101Finish`,
ADR-0012) y el del vidrio (`M_Floor101Glass`, ADR-0013): si faltara, las paredes caerían al material por defecto del
motor (o los vidrios quedarían sin material) y el check falla.

El vidrio es translúcido con iluminación por píxel (`TLM_SurfacePerPixelLighting`), para que refleje el cielo y el sol
con Lumen; no proyecta sombra, así que la luz directa y la de Lumen entran por las ventanas. No hay refracción.

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

### Con la pared de referencia (MVP 0, `CheckRendering.ps1 -Demo`)
14/14 PASS más `Reference wall rendered` (355 × 12 × 240 cm). Cobertura de la surface cache de Lumen
(`r.LumenScene.DumpStats 1`): sin la pared, 1 mesh card y 6 cards; con la pared, **2 mesh cards y 12 cards**. La pared
generada con `UDynamicMeshComponent` aporta sus 6 caras a Lumen (ADR-0005). GPU 3,55 ms frente a 3,47 ms sin la pared.

Warnings conocidos no relacionados con render: `LogEditorDataStorageUI` (UI del editor inexistente al ejecutar el
binario del editor con `-game`).

## Notas
- Salida HDR opcional (ADR-0022): botón «Activar HDR» / «Desactivar HDR», adaptación al monitor y fotos EXR cuando
  está activo. Requiere HDR habilitado en Windows. Monitor y validación independiente: [HDR](hdr.md).
- El monitor virtual de Meta hace que la RTX aparezca dos veces en DXGI (adaptadores 0 y 2). El motor elige el 0.
- UE informa "shader model 6.7" como máximo del adaptador aunque el driver expone 6.8: es el límite que consulta el
  motor, y SM6.6 (lo requerido) está cubierto.
- Modo Foto (MVP 11, ADR-0018): R pasa el viewport del juego al path tracer (`VMI_PathTracing`) sin marcas de edición;
  `r.PathTracing.SamplesPerPixel` según la calidad (Borrador 64, Media 256, Alta 1024), `r.PathTracing.ProgressDisplay 0`
  (la barra del motor nunca sale en la imagen), denoiser del motor (NNEDenoiser). Los shaders del path tracer se
  compilan la primera vez: al terminar, `InvalidatePathTracedOutput` reinicia la acumulación. La foto está lista cuando
  la acumulación lleva 3 fotogramas en el objetivo; "Guardar imagen…" escribe un PNG sin la UI donde se elija en el diálogo (por defecto, junto al proyecto).
- `CheckRendering.ps1` comprueba el Modo Foto ("Path tracing (photo mode)", `-PathTraceSamples`, 64 por defecto): las
  muestras llegan al objetivo y la imagen no es negra. Medido el 2026-09-30: 64/64 muestras en 1,5 s a 853 × 640 con la
  pared de referencia, brillo medio 0,62; `PathTrace-<fecha>.png` en `Saved/RenderCheck`.
- Aviso "[VSM] Non-Nanite Marking Job Queue overflow" (2026-10-02): la cola de marcado de páginas de los VSM para
  mallas no Nanite tiene tamaño fijo en el shader (`MARKING_JOB_QUEUE_SIZE`, sin cvar); si se desborda, el trabajo se
  hace igual pero más lento, sin artefactos. Con `r.Shadow.Virtual.NonNanite.NumPageAreaDiagSlots -1` y
  `r.Shadow.Virtual.NonNanite.LargeInstancePageAreaThreshold 32` el log nombra las mallas de más área: solo el suelo
  (`Floor`, 81–575 páginas) y la esfera del cielo (`SM_SkySphere`, 564) del mapa plantilla; ninguna malla del proyecto
  llega al umbral, ni con 34 muros en dos plantas. Por eso el subsistema les quita la sombra al empezar
  (`OnWorldBeginPlay`): su sombra cae donde no se ve, la imagen no cambia (diferencia media 0,36/255 entre capturas,
  brillo del path tracing 0,62 igual) y el log queda limpio con las mismas vistas. Con casas mucho mayores podría volver
  a salir (las mallas generadas no son Nanite): es un aviso de rendimiento solo de builds de desarrollo, no se oculta.
- `Run.ps1` y `CheckRendering.ps1` lanzan con `-DisablePython`: la app no usa Python, y sin él el plugin de Python del
  editor ejecuta en `-game` el `init_unreal.py` de ToolsetRegistry (solo editor), cuya clase `PythonTestRunner` no está
  cargada, y deja un traceback rojo en la consola al arrancar.
