# Inicio y diagnóstico

101st Floor arranca con su monograma 101, nombre y colores Estudio. Primero
aparece el splash nativo; después una barra de actividad animada acompaña la
inicialización y desaparece al entrar al juego. La barra indica actividad,
no un porcentaje de progreso.

## Opciones → Diagnóstico

- **Modo depuración: registro detallado**: añade trazas de edición y entrada;
  actívalo antes de reproducir un problema.
- **Mostrar ventana de logs**: muestra u oculta la consola inmediatamente.
  Funciona independientemente del modo depuración.
- **Abrir carpeta de registros**: abre la ubicación real del log de la sesión;
  la ruta también aparece en el menú.

Ambos ajustes se recuerdan al reiniciar. Los errores siguen guardándose aunque
la ventana esté oculta. En la versión de desarrollo actual, los archivos están
en `Saved/Logs/`; la ubicación de una aplicación empaquetada puede variar. Esta
validación corresponde al ejecutable Development; una distribución Shipping
debe habilitar y validar el soporte de registro en su proceso de empaquetado.

## Lanzador

```powershell
.\Scripts\Run.ps1                         # respeta preferencias; logs ocultos inicialmente
.\Scripts\Run.ps1 -DebugMode -ShowLogs    # diagnóstico de este arranque
.\Scripts\Run.ps1 -Quiet                 # inicio normal, sin cambiar preferencias
.\Scripts\Run.ps1 -DryRun                # muestra el ejecutable y los argumentos
```

Los modificadores del lanzador solo afectan ese arranque; los cambios desde
Opciones sí se guardan. `Quiet` no admite `DebugMode` ni `ShowLogs` a la vez.

## Comprobaciones

```powershell
.\Scripts\GenerateBranding.ps1           # regenera logo.svg y Splash.png
.\Scripts\CheckLauncher.ps1              # preferencias y argumentos sin abrir el juego
.\Scripts\Build.ps1
.\Scripts\Test.ps1 -Integration
.\Scripts\CheckStartup.ps1               # splash + preload real + captura del componente
.\Scripts\CheckStartup.ps1 -DebugMode
```

Decisión: [ADR-0027](adr/0027-branded-startup-and-diagnostics.md).


![Componente de carga con marca propia](images/startup-loading.png)
![Opciones de diagnóstico](images/startup-diagnostics.png)

![Carga a 800 × 600](images/startup-loading-small.png)
![Diagnóstico a 800 × 600](images/startup-diagnostics-small.png)

Validado el 2026-10-04: compilación Development correcta, 275 pruebas unitarias
(13 035 comprobaciones) y 81 de integración sin fallos, incluyendo persistencia
y cambio de verbosidad (`Saved/Automation/20261004-180645`). Lanzador comprobado
sin abrir el juego. Arranques DX12 reales con normal/depuración y logo visible
(`Saved/StartupChecks/20261004-181205-normal` y `20261004-180902-debug`).
Generación de marca reproducible por hash; capturas del componente de carga y
del menú inspeccionadas a 1280 × 720, 1920 × 1080 y 800 × 600.
