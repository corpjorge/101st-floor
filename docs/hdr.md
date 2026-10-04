# HDR de pantalla

HDR se habilita desde «Activar HDR» en la barra o en Modo Foto; «Desactivar HDR» vuelve a SDR. La preferencia se
guarda en `Saved/Config/.../GameUserSettings.ini`, por equipo. Antes de activar el botón, habilita **Usar HDR** en
Windows > Sistema > Pantalla para el monitor donde está la ventana. Si no está disponible, el botón explica ese
requisito en su ayuda. No modifica Windows automáticamente.

La configuración está en `Config/DefaultEngine.ini` y la adaptación en `Floor101Hdr.cpp` (ADR-0022). UE 5.8 obtiene
luminancia máxima, gamut y blanco de referencia de la pantalla/sistema, con composición HDR de Slate. El perfil
1000 del motor selecciona la codificación; no fija el brillo del panel a 1000 nits. No se fuerza un perfil ASUS:
la adaptación sigue los metadatos de la pantalla donde está la ventana.

## Monitor comprobado (2026-10-03)

| Dato | Resultado |
|---|---|
| Identidad WMI | ASUS TUF VG27AQ1A |
| EDID | PQ/ST-2084, metadatos estáticos tipo 1 (HDR10); HLG no anunciado |
| Resolución del escritorio | 2560 × 1440 |
| Estado inicial | Windows HDR apagado, RGB/sRGB de 8 bits |
| Con HDR de Windows activo | 10 bits, DXGI `RGB_FULL_G2084_NONE_P2020` |
| Luminancia declarada por DXGI | pico y fotograma completo: 603,70 nits; mínimo: 0 |
| Salida de la app (`-game`, UE 5.8.3) | ventana y viewport HDR; scRGB, output device 5, gamut 0 |
| Tone mapping efectivo | pico de 603,70 nits; blanco de referencia de 203 nits |
| EXR del Modo Foto | 1280 × 720, máximo lineal 3,939, media 0,750: conserva valores superiores a 1 |
| Vuelta a SDR | ventana y viewport SDR; nombre PNG; captura legible y no negra |
| Restauración del test | Windows HDR apagado, señal de 8 bits; preferencias previas restauradas |

El brillo típico publicado por [ASUS](https://www.asus.com/au/displays-desktops/monitors/tuf-gaming/tuf-gaming-vg27aq1a/techspec/)
es 250 nits. El valor 603,70 procede del controlador/EDID y no demuestra el brillo físico: no usarlo como medición.
Para calibración personal, el sistema puede suministrar su perfil de pantalla. No se ha medido el panel con
colorímetro ni validado todavía un ejecutable empaquetado. La prueba usa la pared de referencia y no representa una
vivienda completa ni demuestra precisión colorimétrica.

## Comprobación reproducible

```powershell
.\Scripts\CheckHdr.ps1                       # solo consulta; Saved/HDR/Monitor.json
.\Scripts\Build.ps1                          # cerrar Unreal antes
.\Scripts\Test.ps1 -Integration              # incluye rechazo de HDR sin ventana
.\Scripts\CheckHdrOutput.ps1                 # comprueba el estado actual, también con HDR de Windows apagado
.\Scripts\CheckHdrOutput.ps1 -EnableWindowsHdrForTest # activa Windows HDR temporalmente y lo restaura
```

El test real compara el estado de la ventana y el viewport después de alternar la misma operación que usa el botón.
Con HDR disponible también converge el path tracer a 64 muestras y decodifica el EXR para comprobar rango y brillo.
Logs y capturas quedan en `Saved/HDR/` (no versionado). Ejecútalo sin otras instancias de la aplicación abiertas.
La selección manual del botón y el aspecto perceptual del HDR quedan para la revisión del usuario en su pantalla.

La compilación Win64 Development pasó. `Test.ps1 -Integration`: 265/265 tests unitarios (12.892 aserciones) y 59/59
tests Unreal, incluidos `Floor101.Rendering.HDRUnavailable` y `DisplayUnavailable`. Pasaron el ciclo real con Windows HDR activo y el caso
con Windows HDR apagado. `CheckRendering.ps1 -AnalyzeLog Saved/HDR/RenderRegression.log`: todas las comprobaciones
pasaron, con Lumen HWRT, DX12/SM6, materiales y captura PNG a 64 muestras. La ejecución usó 30 frames de calentamiento
y 60 de muestreo a 1920 × 1080, render al 100 %; GPU media 3,85 ms en la escena de referencia.

También pasó la comprobación de [pantalla completa nativa y resolución](display.md): HDR activo al entrar a
2560 × 1440, imagen y render interno a esa resolución, selección de 1920 × 1080 y retorno a ventana.

![Botón Activar HDR en la barra de 101st Floor](images/hdr-toggle.png)

## Guardar fotos

HDR activo: `.exr`, sin UI, conserva la salida HDR en scRGB lineal. HDR desactivado: `.png`, sin UI, formato SDR
habitual. El nombre predeterminado elige la extensión del viewport real. Para obtener un PNG, desactiva HDR antes de
guardar. Los planos 2D siguen exportándose a SVG/PNG. EXR representa la salida de pantalla capturada, no un volcado
sin tone mapping de la radiancia de la escena.

Referencia general: [salida HDR de Unreal](https://dev.epicgames.com/documentation/en-us/unreal-engine/high-dynamic-range-display-output-in-unreal-engine).
La documentación general conserva limitaciones antiguas de ventanas; esta rama verifica la ventana DX12 en UE 5.8.3
directamente y se basa en `HDRHelper.h`, `RenderCore.cpp`, `WindowsPlatformMisc.h` y `GameUserSettings.cpp` instalados.
