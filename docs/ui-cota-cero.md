# Cota Cero — interfaz v2

Estado: **implementada** en la rama `design/studio-ui-redesign` (fases 3–5). Dirección aprobada el 2026-10-09;
decisión en ADR-0033. Documento visual de la propuesta (diagnóstico anotado, maquetas a escala real, sistema y plan):
`design/cota-cero/index.html`. Capturas de la auditoría generadas con `Scripts/CaptureUI.ps1`
sobre el build de desarrollo del 2026-10-09 (`Tests/Data/ui-studio-project.json`).

## Diagnóstico (resumen)

- Barras en dos filas: 13 % de la altura a 1920 × 1080 y 36 % a 800 × 600.
- Sin jerarquía: modos, deshacer, guardar y menús comparten botón; el color más intenso es Salir.
- Paneles pegados a los bordes, herramientas sin agrupar, inspector con ayuda más larga que los campos.
- Tipografía: 12 tamaños; `Font()` suma 2 a los tamaños ≤ 12 (inversión 12/13). Cifras sin alineación tabular.
- Estados activos por tinte multiplicado (13 casos); menús, casillas y ayudas con el estilo por defecto del motor.
- 53 colores hex y 15 `FLinearColor` literales; seis estilos compartidos; `SFloor101Overlay.cpp` con 4.067 líneas.
- Defectos visibles: etiqueta «Nombre de plant» cortada, medida del muro sobre la etiqueta de habitación,
  barra de estado partida a 1280 px.

## Dirección

Se conserva grafito + acento cálido (ADR-0026). Cambian composición, jerarquía y dibujo:
vista a sangre con islas flotantes a 12 px, barra única de 52 px, dock agrupado, isla de Plantas con cotas,
inspector por secciones, píldora de ayuda con teclas dibujadas, cifras en Bahnschrift, iconos de línea con
notación de planta. Latón solo para selección, herramienta activa y acción principal.

## Tokens

| Rol | Valor | Rol | Valor |
|---|---|---|---|
| Lienzo | `#121518` | Texto 1 / 2 / 3 | `#ECEEEA` / `#A9B1AE` / `#88918E` |
| Barra | `#1A1E22` | Latón / suave | `#D6A65F` / `#393630` |
| Isla | `#1F2428` | Sobre latón | `#17140F` |
| Elevado / hundido | `#262C31` / `#15181B` | Peligro / relleno | `#EF7F6E` / `#B8443A` |
| Línea / borde de control | `#2C3238` / `#697177` | Correcto / aviso / info | `#74C095` / `#F2994A` / `#7FA6D6` |

Espaciado en múltiplos de 4; radios 4/6/7/12/16; controles 30–32 px; barra 52 px; dock 200/52 px;
inspector 320/296 px. Texto base Segoe UI 10,5 pt (14 px); cifras Bahnschrift con respaldo Segoe UI Semibold.

## Plan

1. **3a Cimientos:** `Floor101Theme`, fuentes, `FSlateStyleSet` propio, sombras generadas, `SFloor101Icon`,
   componentes compartidos.
2. **3b Diseñar:** barra, dock, barra de vista, Plantas, inspector (planta, muro, hueco, habitación),
   etiquetas y ayuda.
3. **4 Evaluación:** capturas a 1920 × 1080, 2560 × 1440, 1280 × 720 y 800 × 600; `Build.ps1`, `Test.ps1`,
   `Test.ps1 -Integration`. Aprobación de la primera pantalla.
4. **5 Expansión:** catálogos, paletas, Foto, Recorrer, Opciones, Inicio, aviso de cambios, arranque, ADR-0033.

Invariantes: cada control llama a la misma función que hoy; atajos, Esc y bloqueos de entrada iguales;
API pública de `SFloor101Overlay` intacta; sin cambios en módulos puros, `project.json`, escena 3D ni plano 2D.

## Resultado

Los cimientos quedaron en `Floor101Style` (tokens), `Floor101Ui` (componentes) y `SFloor101Icon` (iconos); el
estilo de Slate propio previsto en el plan no hizo falta. Se retiraron `SFloor101ToolIcon` y `SFloor101FavoriteIcon`.

- **Diseñar:** barra única, dock agrupado con teclas, barra de vista, isla de Plantas con cotas y salto directo,
  inspector por secciones con pie de Eliminar, etiquetas de habitación en chip, medida junto al cursor, píldora de
  ayuda que separa teclas y gestos, estado y unidades.
- **Expansión:** catálogos con búsqueda, filtro de favoritos, fichas con anillo de latón y estado vacío; Foto con
  progreso, calidad segmentada y acción principal; Opciones como diálogo con navegación lateral, interruptores y
  tabla de atajos; Inicio con la torre de 101 plantas, «Continuar» y recientes; aviso de cambios con la acción
  destructiva separada; pantalla de arranque con los colores del sistema.
- **Revisión visual** (`Saved/UI/v2`, selección en `images/ui-cota-cero/`): defectos corregidos durante la
  evaluación — etiquetas de habitación ilegibles (el pincel no aplicaba su tinte), icono de nota magenta, píldora
  de ayuda partida a 800 px, tecla de pie de Plantas cortada, tecla de Navegar estirada, botones del aviso apilados,
  marca de Inicio sin el «st» elevado, ruta del registro con barras de Unix.
- **Validación:** `Build.ps1` sin avisos; `Test.ps1 -Integration`: arquitectura correcta, 320 casos unitarios
  (14 132 aserciones) y 88 pruebas de integración en verde.

![Inicio](images/ui-cota-cero/home-1920.jpg)
![Diseñar con muro seleccionado](images/ui-cota-cero/selection-1920.jpg)
![Habitación](images/ui-cota-cero/room-1920.jpg)
![Materiales](images/ui-cota-cero/materials-1920.jpg)
![Muebles](images/ui-cota-cero/furniture-1920.jpg)
![Recorrer](images/ui-cota-cero/walk-1920.jpg)
![Foto](images/ui-cota-cero/photo-1920.jpg)
![Opciones](images/ui-cota-cero/options-1920.jpg)
![Cambios sin guardar](images/ui-cota-cero/unsaved-1920.jpg)
![1280 × 720](images/ui-cota-cero/selection-1280.jpg)
![800 × 600: Plantas como selector](images/ui-cota-cero/levels-800.jpg)
![Inicio a 800 × 600](images/ui-cota-cero/home-800.jpg)
