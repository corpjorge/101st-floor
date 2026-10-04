# ADR-0022: salida HDR opcional y adaptada a la pantalla

Estado: aceptado; validación en monitor de referencia el 2026-10-03.

## Contexto
Se solicita activar y desactivar HDR mediante un botón después de identificar el monitor real. El ASUS VG27AQ1A
anuncia PQ/ST-2084 y metadatos estáticos tipo 1 en su EDID (HDR10), sin HLG. Windows inicialmente tiene HDR apagado.
DXGI declara 603,70 nits de pico; ASUS publica 250 nits de brillo típico. Son datos de distinta naturaleza: ninguno
es una medición de luminancia de este panel.

## Alternativas
Forzar un pico por modelo no se adapta a otros monitores o a perfiles de Windows. Activar HDR solo por la GPU ignora
la pantalla donde está la ventana. Se elige la información efectiva de Windows y la adaptación del motor.

## Decisión
- Permitir HDR con `r.AllowHDR=1`; mantener SDR por defecto y guardar la elección local con `UGameUserSettings`.
- Mostrar «Activar HDR» / «Desactivar HDR» en la barra y en Modo Foto. Si Windows no ofrece HDR en la pantalla de
  mayor intersección con la ventana, mostrar «HDR no disponible» con instrucciones para activar «Usar HDR» en Windows.
  El botón no cambia la configuración global de Windows.
- Consultar `RHIGetDisplaysInformation`, no asumir que cualquier monitor conectado corresponde a esta ventana.
- Usar el pico, gamut y blanco de referencia obtenidos por UE 5.8 del sistema. No forzar 250, 604 ni 1000 nits;
  `SetMaximumHDRDisplayNits(1000)` selecciona el perfil del motor, mientras `OverrideOSMaxLuminance=0` mantiene el
  pico real de los metadatos como límite del tone mapping. No aplicar una calibración inventada.
- Mantener composición HDR de Slate, nivel UI 1, blanco de referencia del sistema. En el binario del editor
  (`Run.ps1`, `-game -windowed`), UE usa scRGB FP16; la señal física HDR10 de Windows es PQ/BT.2020 de 10 bits.
  El motor elige PQ/BT.2020 para builds sin editor; ese empaquetado queda pendiente de validación.
- Capturar fotos HDR en EXR mediante la ruta HDR de `FScreenshotRequest`; SDR sigue usando PNG. Rechazar extensiones
  que no correspondan al estado real del viewport. El núcleo CAD y los formatos del documento no cambian.

## Consecuencias y validación
`Scripts/CheckHdr.ps1` registra EDID, DXGI y el estado HDR de Windows 11. `CheckHdrOutput.ps1` comprueba el ciclo
HDR→SDR, la ventana y el viewport reales, los metadatos y las imágenes. Su opción `-EnableWindowsHdrForTest` activa
HDR de Windows temporalmente y restaura el estado original y las preferencias de la aplicación en `finally`.
Detalles y límites: [HDR](../hdr.md). La comprobación demuestra formato y rango, no una calibración visual o física.
