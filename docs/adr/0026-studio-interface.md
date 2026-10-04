# ADR-0026: Interfaz de estudio arquitectónico

Estado: aceptado. Variante Estudio aprobada por el usuario.

## Decisión

La interfaz permanece en Slate/C++, con componentes compartidos, tipografía
local Segoe UI y Roboto como respaldo. No se redistribuyen fuentes del sistema.
Grafito y arena identifican paneles, herramientas y selección. Las ventanas
pequeñas redistribuyen controles y usan propiedades desplegables; no reducen
todo el escritorio mediante la curva DPI de Unreal.

Diseñar, Recorrer y Foto son estados de presentación. Herramientas y propiedades
usan las acciones existentes del controlador CAD. Opciones captura la entrada
para impedir cambios detrás del menú. Los perfiles gráficos aplican únicamente
escalabilidad, manteniendo Lumen, render nativo y preferencias independientes de
HDR/VSync (ADR-0022/0023).

El entorno es un actor transitorio: cuadrícula original en todo el suelo de la
plantilla al diseñar (visible por defecto y desactivable desde Vista), atmósfera
acromática, luz suave y rejilla opcional de referencia de 1 m. Recorrer y Foto
usan la base neutra; la capa de instalaciones conserva el suelo transparente.
No altera el snapping del CAD ni se exporta. Recorrer y Foto ocultan las ayudas al compilar la vista sin modificar selección, dimensiones o historial.
La selección conserva colores y texturas; sus contornos omiten diagonales
coplanares de la triangulación. Las medidas contextuales derivan del modelo.

Los catálogos comparten tarjetas, búsqueda por palabras sin distinguir tildes
y favoritos con claves estables. Las miniaturas paramétricas derivan de la
misma geometría que el mueble; la biblioteca conserva sus fotografías y créditos.
Favoritos y ocho rutas recientes viven en las preferencias locales. Inicio y
Opciones mantienen la prioridad de entrada mientras haya un menú superpuesto.
Nuevo proyecto reutiliza el reemplazo validado del documento, con un nivel
inicial y sin historial anterior; cambiar de proyecto protege cambios sin guardar.

La confirmación de cambios pendientes vive dentro del mismo overlay Slate del
juego, por encima de Inicio y Opciones. Es asíncrona: guarda la acción pendiente
y continúa solo al descartarlos explícitamente o completar el guardado. Cancelar
y Esc conservan el documento; un guardado cancelado o fallido mantiene el aviso.
No usa ventanas modales nativas, que pueden quedar inaccesibles en pantalla completa.

## Límites

Dimensiones, selección, cortes de muros, acabados e historial permanecen en sus
módulos existentes. Preferencias de interfaz son locales al equipo y no forman
parte de `project.json`. Los assets y capturas se generan de forma reproducible;
las capturas de revisión requieren un build de desarrollo.


Pantalla completa reside en Opciones/Pantalla, con F11 disponible. El menú de
plantas separa navegación, Nueva planta y Ver todas. Exportar SVG/PNG pide nombre
y destino, recuerda la carpeta elegida y no escribe al cancelar. WASD siempre
mueve la cámara; los atajos de paredes y muebles cambian a B/G (ADR-0024).
