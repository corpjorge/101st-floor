# ADR-0024: navegación de cámara y acercamiento continuo

Estado: aceptado.

## Contexto
La rueda se detenía a 50 cm del foco orbital, situado en el centro del proyecto. No permitía continuar hacia un
detalle o recorrer una habitación. WASD también coincide con W (Muro) y A (Amueblar), atajos existentes de edición.

## Decisión
- Añadir «Navegar (N · WASD)» en la barra y Modo Foto. N/Esc vuelven a editar; los clics y caracteres no construyen
  mientras se navega. Elegir una herramienta desde la barra sale de navegación.
- Permitir temporalmente derecho + WASD/QE en edición. Un derecho usado para desplazarse no cancela la herramienta
  al soltarlo. Mantener la órbita habitual cuando el derecho no se utiliza para navegar.
- WASD desplaza respecto a la cámara, Q/E en vertical global, Shift multiplica la velocidad por cuatro. La mirada
  de navegación gira desde la posición de la cámara, incluyendo arriba del horizonte. En plano 2D, WASD desplaza
  según los ejes de pantalla; Q/E y la mirada no alteran el plano.
- Calcular movimiento por tiempo y normalizar diagonales. Ofrecer niveles de velocidad 1–10 (0,1–80 m/s), con
  nivel inicial 5 (3 m/s), independientes del zoom. Guardar la elección en `Floor101.Camera` de las preferencias
  locales y cargarla al iniciar la cámara. La reducción automática cerca del foco hacía demasiado lento WASD.
  Solo se aceptan
  pulsaciones nuevas con foco y navegación activos. Soltar, perder foco, abrir consola o usar Ctrl/Alt/Command
  descarta las teclas aceptadas; un guardado no puede convertirse en movimiento al soltar Ctrl.
- Bajar la distancia mínima orbital a 2 cm y el plano cercano a 1 cm. Al alcanzar el foco, la rueda sigue avanzando
  cámara y foco 5 cm por muesca (máximo 50 cm extra por evento). En 2D solo cambia la escala. F conserva el encuadre.

## Consecuencias y verificación
Es una cámara libre sin colisión, no un personaje con gravedad. Puede atravesar geometría; F recupera una vista
general. Todo pertenece a `Floor101`; el documento CAD y sus formatos no cambian.

Unreal Automation comprueba zoom cercano/avance continuo, mirada, movimiento, diagonales, tiempo, plano 2D,
conflictos de atajos, teclas mantenidas y pérdida de foco. Detalles: [navegación](../camera-navigation.md).


## Actualización — navegación siempre disponible (2026-10-04)

Por solicitud del usuario, WASD/QE mueve la cámara con foco en el viewport en
Diseñar, Recorrer y Foto, sin activar N ni mantener el botón derecho. Pared pasa
a B y Muebles a G. N conserva el cambio de presentación; los menús, campos de
texto, consola y modificadores de atajos bloquean movimiento y reinician las
teclas aceptadas. Los caracteres de movimiento no entran en las medidas del muro;
los dígitos y m/cm/mm conservan la entrada exacta.


## Acceso directo a niveles de velocidad (2026-10-04)

La fila superior 1–9 selecciona el nivel correspondiente y 0 selecciona el 10.
El teclado numérico, los modificadores y la escritura de medidas en una pared
activa conservan su función. Los campos de texto, menús y consola mantienen la
prioridad de entrada. Cambiar el nivel desde el teclado conserva las pulsaciones
WASD aceptadas; hacerlo desde un menú exige una pulsación nueva al volver a la vista.
El selector muestra el nivel también en Diseñar; usa las mismas preferencias locales.

## Memoria independiente 2D/3D (0.1.7)

Quitar el encuadre automático al alternar plano y perspectiva. El pawn conserva foco y distancia de cada modo;
la orientación 3D permanece intacta porque el plano no admite órbita. La primera entrada al plano hereda foco y
escala, y las siguientes restauran su estado previo. El encuadre F solo afecta al modo activo; entrar en otro
proyecto descarta el estado inactivo. Los cambios de planta desplazan la altura de ambos focos. Se trata de estado
de sesión de la cámara; el documento CAD y las preferencias de velocidad no cambian.

## Mirada independiente de Recorrer (0.1.8)

Añadir Shift + arrastre derecho para la mirada desde la posición de cámara en Diseñar y Foto, conservando la
órbita con el arrastre derecho habitual. Reutilizar `Look`, que mantiene la posición y distancia y cambia el foco
según la nueva orientación. El gesto activa la misma bandera que la navegación con derecho + WASD: se mantiene
hasta soltar el botón y evita que un giro corto se interprete como cancelar. Menús y pérdida de foco reinician el
gesto; el plano 2D no gira. Mostrar los controles diferenciados en Opciones y en la ayuda de Foto.
