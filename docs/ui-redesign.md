# 101st Floor — propuesta de interfaz

Estado: **etapa 1, pendiente de revisión**. Rama: `codex/ui-redesign`.
Esta propuesta no modifica la aplicación ni sus preferencias. Las imágenes son
maquetas conceptuales, no capturas de Unreal. Las etapas 2–6 comenzarán después
de revisar la propuesta con el usuario; no fusionar la rama sin su solicitud.

## Identidad: un estudio de arquitectura

Priorizar el proyecto sobre los controles. Superficies opacas en grafito,
tipografía clara, iluminación neutra y un acento arena para selección y acciones.
La vista conceptual representa el ambiente deseado; la iluminación real deberá
comprobarse en Unreal sin alterar la apariencia de los acabados.

| Elemento | Propuesta |
|---|---|
| Fondo de interfaz | Grafito `#1E2328` |
| Paneles | `#252B30`; separadores `#3B4247` |
| Texto principal / secundario | `#EEF0EC` / `#B3BAB9` |
| Acento | Arena `#D6AD78`; no recolorear los materiales seleccionados |
| Entorno | Base clara `#E5E2D9`, sin ajedrezado ni horizonte azul dominante |
| Tipografía | Segoe UI en la propuesta; verificar disponibilidad/distribución antes de integrarla |
| Texto / títulos / ayudas | 13–14 / 18–22 / 11–12 px; ajustar con la escala de interfaz |
| Iconos | Línea fina de 16–18 px, con etiqueta o ayuda y nombre accesible |
| Controles / espaciado | 34–38 px de alto; pasos de 4, 8, 12, 16 y 24 px |
| Paneles | Herramientas 154 px, propiedades 234 px; márgenes interiores 16–20 px |

La maqueta usa iconos Lucide. La implementación necesita recursos locales
reproducibles y su licencia; no depender de una conexión a Internet.

## Pantalla de diseño

**Estudio, recomendado:** herramientas con nombres a la izquierda y propiedades
contextuales a la derecha. **Compacto:** columna de iconos y propiedades que se
abren a petición, dejando más espacio al proyecto. Ambas comparten los modos
Diseñar, Recorrer y Foto, y el acceso a Guardar, Deshacer/Rehacer, Opciones,
Pantalla completa y Salir.

La segunda franja contiene la planta activa y los controles de vista. «Vista»
agrupa Superior, Rayos X, Todas las plantas y visibilidad de redes. El selector
de planta conservará Subir/Bajar, Nueva planta y sus propiedades. «Documento»
conservará Abrir, Guardar como y las exportaciones SVG, PNG, GLB y HDR.

Las propiedades cambian con paredes, puertas, ventanas, habitaciones, muebles,
columnas, instalaciones o planta activa. Conservar Invertir, Eliminar, giro de
muebles y acabados por cara. Una pared dividida continúa siendo seleccionable
y editable como un tramo independiente.

Recorrer oculta paneles y ayudas de construcción, mostrando velocidad 1–10 y
m/s. Foto oculta esas ayudas y presenta calidad y captura PNG/EXR. La velocidad
también debe estar disponible cuando se activa navegación durante Foto.

## Opciones

- **Pantalla:** resolución admitida, identificación de la nativa, pantalla
  completa dedicada, VSync y HDR. Mantener detección real de soporte/estado HDR;
  la maqueta ilustra HDR10 con HDR desactivado en Windows.
- **Información avanzada:** conservar Pantalla/Imagen/Render, porcentaje de
  render y monitor; los valores de la maqueta son ejemplos.
- **Gráficos:** calidad de trabajo y muestras de fotografía. Validar los
  perfiles propuestos con el motor antes de ofrecerlos.
- **Controles:** atajos existentes y velocidad. No interpretar WASD como
  movimiento durante edición normal ni interferir con entradas de medidas.
- **Entorno:** rejilla fina opcional e iluminación. Las ayudas se ocultan por
  modo, sin eliminar su configuración.

Esc cierra primero un menú abierto y conserva su comportamiento de cancelar
herramientas o abandonar modos. Restaurar el foco al viewport al cerrar.

## Ventanas pequeñas, catálogos e inicio

La maqueta pliega las etiquetas de herramientas por debajo de 1000 px y oculta
las propiedades bajo 760 px, manteniendo un botón para abrirlas. Bajo 480 px
reorganiza barras y herramientas en filas. Son puntos de partida: validar
escala DPI y tamaño real del viewport en Windows durante la implementación.

Catálogos: miniaturas reales, búsqueda y favoritos locales por identificador
estable. Las muestras de muebles usan iconos provisionales. Inicio: Nuevo
proyecto, Continuar y recientes verificados; tratar archivos ausentes y cambios
sin guardar antes de sustituir el documento activo.

## Entrega por etapas

| Etapa | Resultado y comprobación necesaria |
|---|---|
| 1 · Propuesta | Maquetas y especificación; revisión visual antes de implementar |
| 2 · Organización | Slate reorganizado; comprobar herramientas, atajos y foco |
| 3 · Opciones | Preferencias reales; comprobar persistencia, resoluciones, HDR y VSync |
| 4 · Estilo | Componentes compartidos; comprobar contraste, DPI y ventanas pequeñas |
| 5 · Entorno | Base, rejilla, luz, selección y cotas; comprobar modos y acabados |
| 6 · Catálogos/inicio | Miniaturas, búsqueda, favoritos y recientes; comprobar guardado y recuperación |

Cada etapa implementada tendrá las comprobaciones pertinentes, captura real
del motor y commit independiente. Mantener CAD en milímetros, comandos
deshacibles y la separación de módulos descrita en `architecture.md` y ADR-0008.

## Validación de la propuesta

Se comprobaron ambas variantes en ventanas de 1320, 1024, 736, 390 y 320 px:
diseño, propiedades y Opciones sin controles fuera de los límites. Se verificaron
categorías de Opciones, cierre con Esc, modos, ocultación de ayudas, velocidad,
búsqueda/favoritos e inicio. La consola del navegador quedó sin errores.
No se ejecutó compilación ni pruebas de Unreal: esta entrega no cambia código
del juego, configuración ni assets del motor.

Fuente interactiva: `design/ui-redesign-prototype.html` (fragmento para el visor
de propuestas de Codex). Capturas de revisión:

- [Diseño Estudio](images/ui-redesign/design-proposal.png)
- [Opciones](images/ui-redesign/options-proposal.png)
- [Distribución Compacto](images/ui-redesign/compact-layout-proposal.png)
- [Diseño en ventana estrecha](images/ui-redesign/design-compact-proposal.png)
- [Opciones en ventana estrecha](images/ui-redesign/options-compact-proposal.png)
