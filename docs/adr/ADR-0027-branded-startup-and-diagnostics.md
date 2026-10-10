# ADR-0027: Arranque propio y diagnóstico local

Estado: aceptado por solicitud del usuario (2026-10-04).

## Decisión

`Content/Splash/Splash.png` reemplaza el splash nativo de la aplicación antes de
inicializar Slate. `Floor101Startup`, cargado en `PreLoadingScreen`, registra un
`EngineLoadingScreen` con el mismo diseño y una barra de actividad animada. La
pantalla termina cuando Unreal indica que acabó de cargar, sin porcentajes
inventados ni demoras artificiales. Editor, commandlets y ejecución sin carga
visual conservan sus flujos. El módulo temprano depende de Slate/PreLoadScreen;
no carga el modelo CAD ni assets UObject. El PNG se distribuye como NonUFS para
estar disponible antes de montar los paquetes.

Logo vectorial y PNG derivan de `Scripts/GenerateBranding.ps1`; no se distribuyen
archivos de fuente. La marca usa el grafito y arena del estilo Estudio.

Opciones → Diagnóstico controla por separado el detalle de `LogFloor101` y la
visibilidad de la consola de logs. Las preferencias locales usan
`[Floor101.Diagnostics]` en `GameUserSettings.ini`; no alteran proyectos ni GPU.
Los cambios se aplican en vivo. La desactivación restaura la verbosidad normal,
conservando el registro de errores en archivo. `Run.ps1` deja de forzar `-log` y
lee esas preferencias antes del arranque. `-Quiet`, `-DebugMode` y `-ShowLogs`
permiten recuperar el inicio desde PowerShell sin sobrescribir preferencias.

## Validación

Pruebas de persistencia y aplicación de verbosidad, comprobación del lanzador y
arranque DX12 real con diagnóstico activo e inactivo. El control de carga se
captura mediante el mismo widget en el overlay de revisión; los marcadores del
arranque confirman que el preload real se mostró y terminó. La animación no
estima cuánto falta de una carga no cuantificable.
