# Rediseño Estudio — implementación

Rama `codex/ui-redesign`. Estudio aprobado por el usuario. Cada etapa se guarda
en un commit independiente; no fusionar hasta recibir la solicitud.

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
