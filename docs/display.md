# Pantalla completa y resolución

El botón **Pantalla completa (F11)** está en la barra y en Modo Foto. Selecciona la resolución nativa del monitor
donde está la ventana y renderiza al 100 %. **Volver a ventana (F11)** recupera el tamaño de ventana anterior.

**Resolución** permite elegir los modos admitidos por ese monitor; la opción **(nativa)** corresponde a sus píxeles
físicos. Una resolución menor reduce el detalle y el trabajo de la GPU. La fila de estado muestra valores efectivos:

| Indicador | Qué comprueba |
|---|---|
| Pantalla | Resolución nativa obtenida de la información del monitor |
| Imagen | Tamaño real del viewport de la aplicación |
| Render | Tamaño interno de SceneColor antes de TSR; en plano 2D muestra «plano 2D» |

Para comprobar correspondencia completa, los tres valores deben coincidir. La fila se resalta cuando difieren.
Después de un cambio pueden tardar unos fotogramas en actualizarse. La escala de render permanece al 100 % y la
resolución dinámica está desactivada; no se anuncia resolución nativa cuando el render 3D es menor.

La elección se guarda localmente en `GameUserSettings.ini`. `Run.ps1` respeta esas preferencias; puedes forzar una
ventana con `-Windowed`, pantalla completa con `-Fullscreen` o dimensiones con `-ResX 1920 -ResY 1080`.
HDR se activa con su botón independiente y requiere «Usar HDR» en Windows; consulta [HDR](hdr.md).

## Sincronización vertical

**Activar sincronización vertical** / **Desactivar sincronización vertical** está en la barra y en Modo Foto.
Activada, sincroniza la presentación con el monitor para reducir cortes de imagen; desactivada, la frecuencia de
fotogramas es independiente. El botón muestra la acción disponible según el estado efectivo de `r.VSync`.
Se aplica inmediatamente y se guarda en `GameUserSettings.ini` (`bUseVSync`) para la próxima sesión. Este cambio
conserva HDR, resolución, escala de render y límite de fotogramas.

## Comprobación reproducible

```powershell
.\Scripts\Build.ps1
.\Scripts\Test.ps1 -Integration
.\Scripts\CheckHdrOutput.ps1 -Display
.\Scripts\CheckHdrOutput.ps1 -Display -EnableWindowsHdrForTest
```

El diagnóstico utiliza las mismas operaciones que los controles: ventana → pantalla completa nativa → resolución
menor → nativa → ventana anterior. También activa/desactiva VSync y verifica que HDR y resolución se conservan.
Compara el viewport con la medición interna de render y guarda una captura en
`Saved/HDR/Fullscreen-native.png`. El test restaura HDR de Windows y las preferencias previas; ejecútalo sin otras
instancias abiertas. La verificación corresponde a UE 5.8.3 en el binario del editor con `-game`, no a un empaquetado.

## Monitor comprobado (2026-10-03)

En el ASUS VG27AQ1A, las pruebas con HDR de Windows activo y apagado confirmaron:

| Paso | Imagen | Render interno |
|---|---|---|
| Ventana inicial | 1280 × 720 | 1280 × 720 |
| Pantalla completa dedicada (`Fullscreen`, modo 0) | 2560 × 1440 | 2560 × 1440 |
| Resolución elegida menor; pantalla nativa 2560 × 1440 | 1920 × 1080 | 1920 × 1080 |
| Recuperar nativa en Modo Foto, 64 muestras | 2560 × 1440 | 2560 × 1440 |
| Volver a la ventana anterior | 1280 × 720 | 1280 × 720 |

La captura PNG tiene 2560 × 1440 píxeles. HDR permaneció activo al entrar en pantalla completa cuando Windows lo
ofrecía. El test apagó HDR de la aplicación para la captura legible y restauró el estado previo de Windows al terminar.
La comprobación del 2026-10-03 también activó VSync durante HDR conservando imagen y render a 2560 × 1440,
lo desactivó y restauró su estado inicial al finalizar.
Se verificaron las operaciones de los controles; la selección manual y el aspecto perceptual quedan para revisión.
Compilación Win64 Development, 265 tests unitarios (12.892 aserciones), 59 tests Unreal y comprobación de render
DX12/SM6, Lumen HWRT y foto pasaron en la validación inicial. Los controles de velocidad y VSync se compilaron
y pasaron 265 tests unitarios (12.892 aserciones) y 66 tests Unreal el 2026-10-03. El arranque respeta preferencias
guardadas y admite las opciones explícitas.

![Pantalla completa nativa: Pantalla, Imagen y Render a 2560 × 1440](images/fullscreen-native.png)

Configuración: `Config/DefaultEngine.ini`; adaptación: `Floor101Display.cpp`; decisión: [ADR-0023](adr/ADR-0023-native-fullscreen-resolution.md).
