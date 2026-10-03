# ADR-0023: pantalla completa nativa y resolución de render verificable

Estado: aceptado; comprobación en ASUS VG27AQ1A, 2026-10-03.

## Contexto
Se solicita un botón dedicado de pantalla completa y poder comprobar que la aplicación utiliza los píxeles reales
del monitor. Igualar el tamaño del viewport no basta: TSR puede reconstruir una imagen desde una resolución interna
menor. La selección debe convivir con HDR y Modo Foto.

## Alternativas
Una ventana sin bordes ampliada conserva la resolución del escritorio y no permite seleccionar directamente otros
modos de pantalla. Mostrar únicamente la resolución solicitada oculta reducciones internas y cambios aún pendientes.

## Decisión
- Usar `UGameUserSettings` con `EWindowMode::Fullscreen`; entrar selecciona la resolución nativa del monitor con
  mayor intersección con la ventana. F11 o el botón vuelven al tamaño de ventana anterior.
- Obtener dimensiones nativas mediante `FDisplayMetrics` (EDID en Windows); enumerar modos del monitor mediante
  `EnumDisplaySettingsW`, sin opciones superiores a la resolución nativa ni inferiores a 1280 × 720.
- Mantener escala primaria y secundaria al 100 %, sin resolución dinámica. La política también se aplica al inicio.
- Mostrar «Pantalla», «Imagen» y «Render»: dimensiones nativas, viewport efectivo y rectángulo de SceneColor antes
  del escalado temporal. Una extensión de vista observa ese pase sin modificar sus colores. En plano 2D se indica
  «plano 2D», porque esa vista se dibuja con Slate sin render de mundo 3D.
- Guardar preferencias locales; `Run.ps1` las respeta y admite opciones explícitas para sobrescribirlas al arrancar.
  Una instalación nueva mantiene la ventana inicial de 1920 × 1080.

## Consecuencias y verificación
El coste de render aumenta frente al escalado automático; bajar la resolución es una elección visible. HDR conserva
su control independiente. La lógica permanece en `Floor101`, sin dependencias Unreal en el núcleo CAD.

`CheckHdrOutput.ps1 -Display` comprueba dimensiones efectivas al entrar, seleccionar una resolución menor, recuperar
la nativa y volver a la ventana anterior. `-EnableWindowsHdrForTest` verifica también HDR en pantalla completa y
restaura Windows y preferencias. Detalles: [pantalla y resolución](../display.md).
