# ADR-0018: Modo Foto con path tracing

Estado: aceptado y verificado (MVP 11, 2026-09-30)

## Contexto
El usuario pidió para el MVP 11 que el programa "tenga path tracing": una imagen fotorrealista del proyecto tal como
está, sin salir de la aplicación. La vista de edición usa Lumen (ADR-0004), rápida pero aproximada; y encima pinta
marcas de edición (selección, vistas previas, tiradores, arcos de apertura) que no pertenecen a una foto. Hay que decidir
qué renderizador produce la imagen, cómo se entra y se sale, cómo se sabe que la imagen está terminada y cómo se guarda,
sin lógica de producto en Blueprints ni pasos manuales en el Editor.

## Alternativas
| Opción | Ventajas | Riesgos |
|---|---|---|
| Movie Render Queue con path tracer | calidad de producción, supermuestreo | solo en el Editor (necesita una Level Sequence); no existe en `-game`; lento para una vista rápida |
| Captura de la vista Lumen a alta calidad | inmediata | no es path tracing: GI y reflejos aproximados |
| Exportar a Twinmotion | presentación completa | fuera de la aplicación; Twinmotion solo presenta (CLAUDE.md), no es el camino de una foto rápida |
| **Path tracer de Unreal en el viewport del juego (`VMI_PathTracing`)** | la misma escena, progresivo, con denoiser, en editor y en `-game` | necesita DX12 + ray tracing por hardware (ya obligatorios, ADR-0004); la imagen tarda segundos y vuelve a empezar al mover la cámara |

## Decisión
- **Modo Foto** (tecla R, botón "Foto (R)"): `UFloor101GameViewportClient::SetPhotoMode` sale de la vista 2D, fija las
  muestras por píxel (`r.PathTracing.SamplesPerPixel`) y cambia el modo de vista a `VMI_PathTracing` (volver a editar:
  `VMI_Lit`). El documento pasa a `UFloor101DocumentSubsystem::SetPhotoMode`, que compila la escena sin marcas de edición
  (`ViewOverlay` vacío y `symbols = false`: sin selección, vistas previas, tiradores ni arcos de apertura).
- **Entrada**: en Modo Foto el clic izquierdo no edita y el puntero no se actualiza; solo valen R/Esc (volver), F
  (encuadrar), T (cenital) y la cámara (orbitar, desplazar, zoom), que reinicia la acumulación.
- **Calidad**: Borrador 64, Media 256 (por defecto) y Alta 1024 muestras por píxel. El denoiser es el del motor
  (NNEDenoiser, activo por defecto).
- **Progreso**: la extensión de vista `FProgress` (`FSceneViewExtensionBase::SetupView`) lee de `View.State` el índice y
  el total de muestras (el estado de la vista es privado del jugador local); la foto está "Lista" cuando la acumulación
  lleva 3 fotogramas parada en el objetivo (al principio la exposición automática puede reiniciarla).
  `r.PathTracing.ProgressDisplay` se pone a 0: el progreso lo muestra el panel, nunca la imagen.
- **Shaders**: los del path tracer se compilan la primera vez que se usan; mientras tanto el panel dice "Preparando
  materiales (N shaders)…" y, al terminar la compilación, `FSceneInterface::InvalidatePathTracedOutput` reinicia la
  acumulación para que la imagen no conserve materiales provisionales.
- **Guardar**: "Guardar imagen" escribe con `FScreenshotRequest` (sin la UI) un PNG junto al proyecto,
  `<nombre>-foto-<fecha>-<hora>.png`; otra extensión se rechaza.
  *Actualización (2026-10-22):* "Guardar imagen…" abre el diálogo de Windows para elegir carpeta y nombre; propone ese
  nombre en la última carpeta usada para fotos (`[Floor101.Paths] LastPhotoFolder`), si no junto al proyecto. Cancelar
  no guarda nada. PNG, o EXR mientras la pantalla está en HDR (ADR-0022).
- **UI**: en Modo Foto se ocultan la barra, el panel de propiedades, las etiquetas de habitaciones y el rótulo del cursor;
  el panel "Foto (path tracing)" muestra muestras y barra de progreso, calidad, "Guardar imagen", "Encuadrar (F)",
  "Cenital (T)" y "Volver a editar (R)".
- **Validación del render**: `RenderCheck` añade las fases `PathTrace` y `PathTraceImage` (64 muestras, 300 s como máximo,
  `-F101RenderCheckPathTraceSamples=`); `Scripts/CheckRendering.ps1` comprueba "Path tracing (photo mode)": muestras
  ≥ objetivo y una imagen no negra (brillo medio > 0,05).

## Consecuencias
- La foto tiene la resolución del viewport (sin supermuestreo ni tamaño elegido); para más, Movie Render Queue sigue
  siendo una opción futura (otro ADR).
- Cada movimiento de cámara reinicia la imagen; con muchas muestras la espera es de segundos.
- El path tracer ignora los ajustes de Lumen; vidrio y luz se calculan físicamente, así que la foto puede verse más oscura
  o más clara que la vista de edición.
- Sin ray tracing por hardware el Modo Foto no funciona; es coherente con ADR-0004 (un fallback debe fallar).

## Verificación
- Unreal Automation: `Floor101.Interaction.PhotoMode` (la foto quita el tinte de selección y el arco de apertura y los
  devuelve al salir; nombre del PNG; formato rechazado) y `Floor101.Interaction.KeyMap` (R).
- `Scripts/CheckRendering.ps1 -Demo`: "Path tracing (photo mode)" PASS, 64/64 muestras en 1,5 s en la RTX 5070 Ti, brillo
  0,62, imagen sin barra de progreso.
- Prueba real en el juego: ver `docs/mvp11-report.md`.
