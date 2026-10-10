# ADR-0033: sistema de diseño Cota Cero

Estado: aceptado por el usuario el 2026-10-09 (dirección, Salir discreto, Segoe UI + Bahnschrift, texto base de
14 px y salto directo de planta). Implementado en todo el producto: Diseñar, Recorrer, Foto, catálogos, Opciones,
Inicio, aviso de cambios y arranque. Amplía ADR-0026 sin sustituirlo.

## Contexto
La interfaz Estudio (ADR-0026) organizó modos, herramientas y propiedades, pero cada control se configuraba en su
sitio: 53 colores hexadecimales y 15 `FLinearColor` literales, doce tamaños de letra con un «+2» oculto en `Font()`,
estados activos por tinte multiplicado y menús, casillas y ayudas con el estilo por defecto del motor. Las barras
ocupaban el 13 % de la altura a 1920 × 1080 y el 36 % a 800 × 600. Diagnóstico completo y maquetas en
`docs/ui-cota-cero.md` y `docs/design/cota-cero/index.html`.

## Alternativas
- **Retocar Estudio** (colores y botones): poco riesgo, pero no resuelve jerarquía, composición ni deuda de estilos.
- **UMG o una capa web**: más herramientas de diseño, pero rompe ADR-0026 (Slate/C++), añade dependencias y
  duplica la entrada y el foco ya resueltos.
- **Sistema propio en Slate** (elegido): tokens y componentes en C++, sin assets nuevos, misma arquitectura.

## Decisión
- `Floor101Style` es la única fuente de colores (`Palette()`), tipografía (`Text(EText)`), medidas (`Size`,
  `Breakpoint`), pinceles redondeados cacheados (`Box`) y estilos de widget. `Font()` conserva su firma, sin el «+2».
- Fuentes de Windows leídas en su sitio, sin redistribuir: Segoe UI (Semilight, Regular, Semibold, Bold) para el
  texto y Bahnschrift para cifras y marca; si falta Bahnschrift, Segoe UI Semibold; si falta Segoe UI, Roboto del
  motor. Texto base 10,5 pt (14 px a 100 %).
- `Floor101Ui` reúne los componentes: isla flotante con sombra por capas y aparición de 160 ms, botones (principal,
  secundario, discreto, peligro, salir), botón de icono, seleccionable, opción, segmentado, interruptor animado,
  tecla, sección, fila de propiedad, nota, campo, filas y botones de menú. Cada uno recibe la llamada que ejecuta.
- `SFloor101Icon` dibuja unos 50 iconos de línea en retícula de 20; las herramientas usan la notación de planta.
- Composición de Diseñar: barra única de 52 px; islas a 12 px de la vista (dock agrupado, barra de vista, Plantas con
  cotas, inspector por secciones, píldora de ayuda con teclas, estado). Recorrer y Foto conservan barra, vista,
  plantas y ayuda. Bajo 1000 px el inspector se abre desde la barra de vista; con poca altura Plantas pasa a selector.
- Salir es discreto y se vuelve rojo bajo el puntero; el único color intenso en reposo es el latón de la selección.
- Las cifras de solo lectura (áreas, cotas de planta, velocidad) usan coma decimal como el plano 2D; los campos
  editables muestran el texto del núcleo CAD, sin cambios.

## Consecuencias
- Ningún control cambia la función que llama, ni atajos, Esc o bloqueo de entrada. La API pública de
  `SFloor101Overlay` se mantiene; `CaptureUI.ps1` añade pantallas de menús para revisión.
- Pulsar una planta de la isla llama a `EditController::setActiveLevel`, ya existente: comodidad aprobada.
- En el aviso de cambios, Descartar queda a la izquierda, separado de Cancelar y Guardar; las tres respuestas y
  `ResolveUnsavedPrompt` no cambian.
- `SFloor101ToolIcon` y `SFloor101FavoriteIcon` desaparecen: sus dibujos viven en `SFloor101Icon`. La pantalla de
  arranque (`Floor101Startup`, sin dependencia de `Floor101`) copia los tres colores que usa.

## Verificación
Compilación, `Test.ps1 -Integration` y capturas reales con `CaptureUI.ps1` a 1920 × 1080, 1280 × 720 y 800 × 600 en
Diseñar (sin selección, muro, habitación, cotas, muebles, materiales, favoritos), Recorrer, Foto, Opciones (todas las
secciones), Inicio, aviso de cambios y menús abiertos. Selección en `docs/images/ui-cota-cero/`.
