# ADR-0008: Interacción de edición: controlador puro + cliente de viewport de Unreal

Estado: aceptado y verificado (MVP 1, 2026-09-28)

## Contexto
El MVP 1 (prompt maestro §19, §20, §46) necesita edición interactiva: herramienta de muro (clic, vista previa,
longitud escrita, Enter), selección, borrar, cambiar longitud/espesor/altura, arrastrar extremos, deshacer/rehacer,
guardar/abrir. Reglas que condicionan el diseño: la lógica de producto no vive en Unreal ni en Blueprints (ADR-0002),
nada se configura a mano en el editor (todo reproducible desde Git) y el snapping pertenece al núcleo (§20).

## Alternativas
| Capa | Opción | Ventajas | Riesgos |
|---|---|---|---|
| Lógica | Estado de herramientas en actores/componentes de Unreal | idiomático en Unreal | no se prueba sin el motor; la lógica se dispersa por el adaptador |
| Lógica | **`EditController` en el CAD Core** (C++ puro) | todo el flujo se prueba con doctest en segundos; Unreal solo traduce | hay que expresar la entrada como rayos en espacio de escena |
| Entrada | Enhanced Input (`UInputAction` + mapping context) | reasignable; estándar de UE5 | assets binarios o creación en runtime; no da texto (la longitud escrita) |
| Entrada | Widget Slate a pantalla completa que capture la entrada | modelo de foco de Slate | compite con el viewport por el ratón y la cámara |
| Entrada | **`UGameViewportClient` propio** | un único punto recibe teclas, caracteres, rueda y ratón; lo no usado sigue al motor | no reasignable (aceptable en el MVP) |
| UI | UMG (Widget Blueprints) | editor visual | assets binarios hechos a mano; lógica tentada a vivir en Blueprints |
| UI | **Slate en C++** (`SFloor101Overlay`) | versionado como texto; sin pasos manuales | más código que UMG |

## Decisión
- **`f101::cad::EditController`** (CAD Core) contiene toda la interacción: modos Seleccionar/Muro, selección, arrastre
  de asas de extremo, cancelación, texto escrito, propiedades de la selección, deshacer/rehacer. Recibe rayos en
  espacio de escena (`Ray3`, mm) y devuelve `Status`; expone `view()` (selección, muro oculto, vista previa, asas),
  que la aplicación convierte en `scene::ViewOverlay`. Cada cambio de modelo es un comando deshacible (ADR-0006).
- **Vista**: el overlay es estado de vista, nunca se guarda. `compileScene(project, overlay)` añade estilos
  (`Selected`, `Preview`, `Handle`, `Hidden`) y nodos `preview/wall` y `marker/<i>`. El adaptador aplica los estilos con
  instancias dinámicas del material por defecto (parámetro `Color`) y el reconciliador compara hash **y** estilo.
- **Unreal traduce, no decide**: `UFloor101GameViewportClient` convierte el cursor en rayo (deproyección +
  `ToScenePosition`), las teclas en comandos (`MapKey`, función pura) y los caracteres en texto de la herramienta.
  `UFloor101DocumentSubsystem::Edit` ejecuta cada acción, refresca la vista y guarda el mensaje de estado.
  `AFloor101EditingPawn` es una cámara orbital; `AFloor101GameMode` y el viewport client se configuran en
  `Config/DefaultEngine.ini` (también rigen en PIE).
- **Entrada**: ratón izquierdo = seleccionar/dibujar/arrastrar asa; derecho arrastrando = orbitar; clic derecho sin
  arrastre = cancelar (en PIE el editor reserva Esc para detener la sesión); central = desplazar; rueda = zoom (por el
  eje `MouseWheelAxis`, que trae el número de muescas). Teclas: W, V, Enter, Esc, Retroceso, Supr, F, Ctrl+Z,
  Ctrl+Y/Ctrl+Shift+Z, Ctrl+S, Ctrl+O. Sin unidad, las longitudes escritas son centímetros.
- **Precisión**: la longitud escrita sigue la dirección exacta del ajuste (`SnapResult::constrained`) y
  `pointAtDistance` elige, de los cuatro milímetros alrededor del punto exacto, el de longitud más cercana: en diagonal
  el error queda por debajo de 0,5 mm (un muro de "300" a 75° mide 3000 mm, no 2999).

## Consecuencias
- La interacción completa se prueba sin Unreal (`EditControllerTests`, `WallToolTests`, `SnappingTests`); Unreal
  aporta pruebas de integración finas (mapa de teclas, cámara, flujo del subsistema, estilos del adaptador).
- Cada movimiento del cursor recompila la escena (todas las paredes) y reconcilia solo lo que cambió. Suficiente para
  el MVP; con cientos de paredes habrá que compilar solo el overlay.
- Los mensajes del núcleo (errores, etiquetas de deshacer) están en inglés; la UI en español. Traducirlos es trabajo
  futuro.
- Las herramientas MCP (`Floor101DocumentToolset`) actúan sobre el documento de la sesión PIE mientras existe; si no,
  sobre el del mundo del editor.

## Verificación
- 90 casos doctest (entre ellos 12 del `EditController`, regresión de longitud en diagonal) y 18 pruebas de
  automatización de Unreal (`Floor101.*`), todas en verde.
- Prueba real con ratón y teclado en `-game`: seleccionar (tinte naranja, asas amarillas, panel de propiedades),
  arrastrar extremo (vista previa azul, etiqueta de longitud), Ctrl+Z, Supr, longitud 4.2 m desde el panel, W + clic +
  "300" + Enter (3000 mm en diagonal), guardar y abrir `project.json`, F, rueda. Esc no pudo probarse con la
  herramienta de automatización (se reserva la pulsación de Esc); su camino (`cancel`) se probó con clic derecho y
  `Floor101.Interaction.KeyMap` cubre la tecla.
- MCP: con PIE activo, `AddWall`/`Undo`/`Redo` actúan sobre `UEDPIE_0`; al detener PIE el documento del editor está
  intacto.
