# Controles de 101st Floor

Referencia de teclas y gestos. Las guías de cada función están enlazadas desde el [README](../README.md).


**Puertas y ventanas:** catálogo de 24 familias paramétricas con miniaturas, búsqueda y filtro de apertura;
medidas ajustables antes y después de colocar. [Guía del catálogo](opening-catalogue.md).
B: muro · P: puerta · O: ventana (el tipo se elige en el panel antes de colocarla) · M: pintar (pestañas Paredes / Suelos) · G: amueblar (pestañas por categoría) · C: pilar · I: instalación (clic por punto, doble clic o Enter para terminar; tipo y altura en el panel) · X: rayos X (ver las instalaciones; la fila "Redes" elige electricidad, agua, aguas negras o gas, también en el plano y su exportación) · V: seleccionar · R: Modo Foto (path tracing; "Guardar imagen…" pide dónde y con qué nombre) · escribir `355` / `3.55 m` / `3550 mm` + Enter: muro exacto (sin unidad = cm) · Supr: eliminar ·
Ctrl+Z / Ctrl+Y: deshacer / rehacer · Ctrl+S / Ctrl+Shift+S / Ctrl+O: guardar / guardar como (eliges carpeta y nombre) / abrir · Esc o clic derecho: cancelar ·
WASD siempre para moverse, sin activar N, Q/E para bajar/subir, Shift para ir más rápido, derecho para mirar; N/Esc vuelve a editar. **Velocidad** permite elegir niveles 1–10 (inicial: 3 m/s) y guarda la elección. Números superiores 1–9: nivel de velocidad; 0: nivel 10 (excepto al escribir medidas). B: paredes; G: muebles. N solo cambia Diseñar/Recorrer. [Navegación y zoom](camera-navigation.md).
**Sincronización vertical**: activar/desactivar desde Opciones → Pantalla; la elección se guarda. [Pantalla](display.md).
**Salir**, arriba a la derecha: cierra la aplicación; con cambios sin guardar permite guardar y salir, salir sin guardar o cancelar. Disponible también en Modo Foto y pantalla completa.
**Nombre de habitación**: selecciona su suelo, escribe Nombre en Propiedades y pulsa Enter. Se guarda y aparece en rótulos y exportaciones SVG/PNG. [Nombres de habitaciones](room-names.md).

**Cotas**: pulsa H, elige dos puntos y coloca la línea. Selecciona cotas manuales o automáticas para editarlas o eliminarlas; se guardan y se exportan. [Guía](dimensions.md).

**Rodapiés**: selecciona el suelo y activa Rodapiés en Propiedades; ajusta altura, espesor y acabado. Se guardan y respetan los huecos de puertas. [Guía](baseboards.md).
arrastrar el cuerpo naranja de una pared: trasladarla con sus uniones · arrastrar un asa amarilla: mover la esquina · con un hueco seleccionado, arrastrarlo: moverlo; arrastrar sus asas:
cambiar el ancho · Espacio: bisagras al otro lado (al colocar, la puerta abre hacia donde apuntas) o girar 90° el mueble o el pilar · con un mueble, un pilar o una tubería seleccionados, arrastrarlo: moverlo (las asas de una tubería mueven un codo) · derecho arrastrando: orbitar · central: desplazar · rueda: zoom ·
F: encuadrar · T: vista cenital · Tab: plano 2D (Tab otra vez: 3D) · F11 / "Pantalla completa": resolución nativa y vuelta a ventana · "Resolución": elegir modo y comprobar pantalla/imagen/render · "Plano: SVG / PNG": exportar el plano ·
"Twinmotion (.glb)…": exportar la casa entera donde elijas (se recuerdan las últimas carpetas) (en Twinmotion: Importar → el `.glb`) ·
"Importar textura…" (en Pintar) / "Importar modelo…" (en Amueblar): traer una imagen o un modelo 3D al proyecto ·
RePág / AvPág: planta de arriba / de abajo · L: ver todas las plantas. Detalle en
[docs/unreal-integration.md](unreal-integration.md#controles).

