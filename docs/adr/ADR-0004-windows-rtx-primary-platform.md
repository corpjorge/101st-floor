# ADR-0004: Windows 11 + DirectX 12 + RTX como plataforma principal

Estado: aceptado y verificado (Fases A y B, 2026-09-28)

## Contexto
El prompt maestro fija Windows 11 x64, DirectX 12, SM6 y una RTX 5070 Ti como plataforma de referencia, con Lumen por
hardware para el Modo Diseño. Versiones anteriores del prompt asumían macOS/Metal.

## Alternativas
| Opción | Ventajas | Riesgos |
|---|---|---|
| Multiplataforma desde el inicio (DX12 + Vulkan + Metal) | alcance | multiplica configuraciones y validación; sin hardware de prueba equivalente |
| DX12 con fallback a DX11/SM5 | arranca en más equipos | un fallback silencioso ocultaría problemas de RTX/Lumen; duplica el coste de shaders |
| **Solo DX12/SM6 en Windows** | una sola ruta gráfica, validable de principio a fin | no arranca en GPU sin SM6 (fuera del objetivo) |

## Decisión
- `DefaultGraphicsRHI=DX12`, formatos de shader solo `PCD3D_SM6`, **sin formatos D3D11**: si DX12 falla, la aplicación
  no arranca, en lugar de degradarse sin avisar.
- Ray tracing por hardware, Lumen GI y reflexiones con HWRT, VSM, Nanite disponible, Substrate.
- El CAD Core, la geometría y la escena siguen siendo independientes de GPU y plataforma (ADR-0002). Solo el adaptador
  de Unreal y la configuración del motor conocen DX12.

## Consecuencias
- `CheckRendering.ps1` es la verificación objetiva de la plataforma (14 comprobaciones sobre logs y ProfileGPU).
- Portar a otra plataforma afectaría a la configuración del motor y a la validación, no al núcleo.

## Verificación
Fase B: `CheckRendering.ps1` 14/14. DX12 con SM6, RTX 5070 Ti, DXR 1.1, atomics de 64 bits de SM6.6, 9 pases de ray
tracing de Lumen en ProfileGPU y 0 errores de RHI. Detalle en `docs/rendering-rtx.md`.
