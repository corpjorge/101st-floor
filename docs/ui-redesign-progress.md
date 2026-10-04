# Rediseño Estudio — implementación

Rama `codex/ui-redesign`. Estudio aprobado por el usuario. Cada etapa se guarda
en un commit independiente; no fusionar hasta recibir la solicitud.

Etapas 1–6 completadas. La propuesta está en `ui-redesign.md`; las capturas
siguientes corresponden a la aplicación real. Pendiente: revisión del usuario
y cualquier publicación o fusión que solicite posteriormente.

## Etapa 2 — organización

- Barra superior con proyecto, Diseñar/Recorrer/Foto, Guardar, Deshacer/Rehacer,
  Pantalla completa y Salir. Documento, Vista, plantas y Opciones agrupados.
- Construcción a la izquierda; propiedades a la derecha. En ventanas menores
  de 950 unidades Slate se abren desde «Propiedades». Barras que se distribuyen
  en filas y paneles desplazables para no taparse entre sí.
- Conserva acciones existentes: exportación, redes, plantas, propiedades,
  acabados y atajos. Los menús ejecutan las mismas llamadas del programa.
- Añadido `Scripts/CaptureUI.ps1`, exclusivo de comprobación en builds de
  desarrollo, y un proyecto sin assets externos para capturas reproducibles.

Validación: compilación UE 5.8.3 correcta, 275 pruebas unitarias/13035 aserciones
y 67 pruebas de integración correctas. Captura real del motor inspeccionada;
sin errores de ejecución en el registro de esa sesión.

![Organización](images/ui-redesign/stage2-organization.png)

Pendiente: menú Opciones dedicado, estilo de componentes, entorno, catálogos
con búsqueda/favoritos e inicio (etapas 3–6).

Para reproducir una captura después de compilar:

```powershell
.\Scripts\CaptureUI.ps1 -Project Tests/Data/ui-studio-project.json -Output Saved/UI/design.png
```

`-Screen options`, `walk`, `photo`, `furniture`, `materials` o `home` permiten
comprobar otras pantallas. `-ResX 800 -ResY 600` comprueba ventanas pequeñas.
La sesión abre únicamente el fixture indicado, captura la UI real y termina;
no guarda ni sobrescribe el proyecto.

## Etapa 3 — Opciones

- Pantalla, Gráficos y Controles en un menú centrado. Resolución, HDR y VSync
  salen del panel Foto. Pantalla/Imagen/Render y monitor en Información avanzada.
- Perfiles Fluida/Equilibrada/Alta con preferencia local. Mantienen Lumen y
  render al 100 %, aplicando únicamente escalabilidad; no reaplican HDR/VSync.
- Velocidad accesible al recorrer, en Foto y en Controles. El menú bloquea
  movimiento/edición detrás de él. Esc cierra primero Opciones y restaura el foco.

Validación: compilación correcta, 275 pruebas unitarias y 69 de integración.
Las dos nuevas pruebas comprueban aislamiento de ajustes y prioridad de entrada
del menú. Captura real inspeccionada. Pendiente: estilo, entorno y catálogos/inicio.

![Opciones](images/ui-redesign/stage3-options.png)

## Etapa 4 — componentes Estudio

- Paneles opacos en grafito, acento arena, botones planos con estados de foco,
  texto Segoe UI del sistema (respaldo Roboto) y campos de edición consistentes.
- Iconos geométricos propios para construcción, con nombres y atajos en ventanas
  grandes y ayudas al pasar el cursor en ventanas pequeñas.
- Texto legible a 800 × 600: la interfaz redistribuye filas y propiedades en lugar
  de encoger toda la pantalla. Estado e indicación de unidades se ajustan al ancho.
- Corregidas colisiones de nombres de pruebas y una macro Windows que afloraban
  al reorganizar la compilación agrupada; no se desactivaron comprobaciones.

Validación: compilación correcta, 275 pruebas unitarias y 69 de integración.
Capturas reales inspeccionadas a 1920 × 1080 y 800 × 600. Pendiente: entorno,
resaltado/medidas y catálogos/inicio (etapas 5–6).

![Estudio](images/ui-redesign/stage4-style.png)
![Ventana pequeña](images/ui-redesign/stage4-small-design.png)
![Opciones pequeñas](images/ui-redesign/stage4-small-options.png)

## Etapa 5 — entorno y edición

- Base neutra sin ajedrezado, atmósfera neutra, luz con sombras suaves y fondo
  atmosférico sin dominante azul. Rejilla de referencia opcional de 1 m en «Vista»;
  independiente del ajuste de precisión del CAD y oculta en Recorrer/Foto.
- Selección con contorno arena que omite diagonales de triangulación y conserva
  el color y las texturas originales. Medidas de longitud, espesor y altura
  junto al muro; la etiqueta queda dentro de la ventana.
- Recorrer y Foto ocultan selección, previews, marcadores y símbolos. Al volver
  a Diseñar se restauran; el proyecto y su historial permanecen intactos.
- Panel Foto con calidad que se redistribuye, sin recortar «Borrador»; campos
  de acabados utilizan también el estilo común.

Validación: compilación correcta, 275 pruebas unitarias y 69 de integración.
Regresiones de color/textura y del cambio entre modos. Capturas reales de
selección, Recorrer, Foto y rejilla inspeccionadas. Pendiente: catálogos e inicio.

![Selección y medidas](images/ui-redesign/stage5-selection.png)
![Recorrer](images/ui-redesign/stage5-walk.png)
![Foto](images/ui-redesign/stage5-photo.png)
![Rejilla](images/ui-redesign/stage5-grid.png)

## Etapa 6 — catálogos e inicio

- Muebles paramétricos con miniaturas derivadas de su geometría; modelos de la
  biblioteca con sus imágenes y créditos. Materiales con muestras grandes y
  nombres. Los modelos importados sin fotografía muestran un icono de modelo;
  se mantienen importación, medidas, acabados y eliminación.
- Búsqueda por palabras, sin distinguir mayúsculas ni tildes. Buscar muebles
  consulta todas las categorías; materiales conserva la separación Paredes/Suelos.
  Filtro Solo favoritos, estrella propia y estado sin resultados; preferencias
  persistentes independientes del proyecto y de Deshacer/Rehacer.
- Inicio con Nuevo proyecto, Continuar, Abrir, ocho recientes, Opciones y Salir.
  Pulsar la marca abre Inicio; Esc vuelve al escritorio. Archivos ausentes se
  identifican y deshabilitan. Nuevo/Abrir ofrece guardar, descartar o cancelar
  cuando hay cambios pendientes. Un proyecto nuevo conserva un nivel válido.
- Tarjetas legibles con fondos neutros; paneles y entrada modal consistentes.
  Cerrar Opciones sobre Inicio mantiene bloqueados el CAD y la cámara.
- Escribir en buscadores o propiedades no activa herramientas, añade medidas
  ni mueve la cámara. La barra de progreso de Foto utiliza también el acento arena.

Validación: compilación correcta, 275 pruebas unitarias (13 035 comprobaciones)
y 74 de integración, sin fallos. Cinco nuevas regresiones cubren búsqueda y
favoritos, recientes, documento nuevo, menús superpuestos y foco de escritura.
Capturas inspeccionadas a 1920 × 1080 y 800 × 600. Comprobación de GPU correcta:
DX12/SM6, ray tracing, Lumen HWRT, sombras virtuales, Nanite y Foto a 64 muestras.
Log local: `Saved/RenderCheck/RenderCheck-20261003-234920.log`.

![Inicio](images/ui-redesign/stage6-home.png)
![Muebles](images/ui-redesign/stage6-furniture.png)
![Materiales](images/ui-redesign/stage6-materials.png)
![Búsqueda de muebles](images/ui-redesign/stage6-search.png)
![Favoritos](images/ui-redesign/stage6-favorites.png)
![Búsqueda de materiales](images/ui-redesign/stage6-materials-search.png)
![Inicio pequeño](images/ui-redesign/stage6-small-home.png)
![Catálogo pequeño](images/ui-redesign/stage6-small-furniture.png)
![Opciones pequeñas](images/ui-redesign/stage6-small-options.png)


## Corrección — cambios sin guardar dentro del juego (2026-10-04)

- Sustituido el cuadro nativo de Windows por un aviso Slate centrado sobre la
  propia pantalla del juego. Mantiene el estilo Estudio y se ajusta al tamaño
  disponible; Guardar, Descartar y Cancelar quedan accesibles en pantalla completa.
- Nuevo, Abrir, recientes y Salir conservan su acción pendiente hasta resolver el
  aviso. Cancelar y Esc mantienen el documento; el guardado debe completarse antes
  de continuar. Un guardado fallido o cancelado mantiene disponibles las opciones.
- La confirmación tiene prioridad sobre Inicio/Opciones y bloquea edición y
  movimiento detrás del panel. Peticiones repetidas no sustituyen la acción ni
  ejecutan dos veces la continuación; al cerrar se restaura el menú correspondiente.

Validación: compilación correcta, 275 pruebas unitarias y 76 de integración,
ningún fallo. Dos nuevas regresiones ejercitan el overlay real, verifican que no
se crea otra ventana, comprueban Cancelar/Esc, Descartar, guardado correcto,
guardado fallido y recuperación de entrada. Capturas reales del flujo de cambio
con modificaciones, en pantalla completa a 2560 × 1440 y ventana de 800 × 600.
Rama `codex/ui-redesign`, sin fusionar.

![Aviso en pantalla completa](images/ui-redesign/fix-unsaved-fullscreen.png)
![Aviso en ventana pequeña](images/ui-redesign/fix-unsaved-small.png)


## Ajustes tras la revisión del usuario (2026-10-04)

- Pantalla completa queda en Opciones → Pantalla; F11 continúa disponible en la
  vista del proyecto. Se retira el botón duplicado de la barra superior.
- Menú de plantas con encabezado, fila Subir/Bajar y botones Nueva planta/Ver
  todas separados. Salir usa alineación horizontal/vertical centrada y tamaño
  de texto consistente en la pantalla de inicio.
- Documento → SVG/PNG permite elegir nombre y carpeta antes de escribir. La
  carpeta elegida se recuerda; cancelar no exporta. Se conservan planta activa,
  medidas y formatos. `Scripts/CheckPlanExport.ps1` verifica la ruta elegida
  con el renderer real además de las comprobaciones sin GPU.
- WASD/QE mueve la cámara en Diseñar, Recorrer y Foto sin activar N. Pared pasa
  a B y Muebles a G; N solo cambia Diseñar/Recorrer. Menús, texto, consola,
  modificadores y pérdida de foco mantienen la protección de entrada. Las teclas
  de movimiento no se añaden a las medidas; m/cm/mm y Ctrl+S siguen funcionando.
- Cuadrícula original restaurada en todo el suelo al diseñar, visible por defecto
  y desactivable desde Vista. Recorrer/Foto mantienen la base neutra. La vista
  de instalaciones conserva su suelo transparente; no se alteran acabados del
  proyecto. Se evita superponer otra rejilla en la planta a cota cero.

Validación: compilación correcta, 275 pruebas unitarias (13 035 comprobaciones)
y 78 de integración sin fallos. Regresiones del nuevo mapa de teclas, elección y
cancelación de destinos y transición entre cuadrícula, base neutra y suelo
transparente. Exportación SVG/PNG con DX12 correcta
(`Saved/Automation/PlanExportGPU-20261004-135122`); informe general
`Saved/Automation/20261004-135651`. Capturas reales inspeccionadas a
1920 × 1080 y 800 × 600. Rama sin fusionar.

![Diseño con cuadrícula](images/ui-redesign/feedback-design-grid.png)
![Menú de plantas a 800 × 600](images/ui-redesign/feedback-levels-small.png)
![Inicio con Salir centrado](images/ui-redesign/feedback-home-small.png)
![Pantalla completa en Opciones](images/ui-redesign/feedback-options-small.png)
![Recorrer mantiene la base neutra](images/ui-redesign/feedback-walk.png)
