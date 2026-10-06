# Navegación y zoom

**WASD funciona siempre** con el foco en la vista del proyecto, tanto en Diseñar como en Recorrer y Foto.
**N / Recorrer** cambia la presentación y oculta las ayudas de construcción; no hace falta para moverse.
Los atajos de Pared y Muebles son **B** y **G**.

| Entrada | Acción |
|---|---|
| W / S | avanzar / retroceder según la dirección de la cámara |
| A / D | izquierda / derecha |
| Q / E | bajar / subir |
| Shift + movimiento | cuatro veces más rápido |
| Números superiores 1–9 / 0 | elegir nivel 1–9 / 10; el teclado numérico no cambia la velocidad |
| Selector Velocidad | niveles 1–10; muestra la velocidad en m/s y guarda la elección |
| Shift + derecho arrastrando | mirar desde la posición actual sin desplazarse, en Diseñar, Recorrer y Foto |
| Derecho arrastrando | orbitar en Diseñar/Foto; mirar desde la posición actual en Recorrer |
| Central arrastrando | deslizar la vista sin girarla |
| N / Esc | volver a editar |
| F | encuadrar el proyecto y recuperar una vista general |
| Tab | alternar 2D/3D conservando el encuadre y zoom de cada vista |

En edición, **WASD/QE** mueve la cámara sin mantener el botón derecho. **B** dibuja paredes y **G** abre muebles. Un derecho arrastrado sin teclas de movimiento conserva la órbita habitual.
En plano 2D, WASD desplaza el plano sobre los ejes de pantalla; Q/E no cambian su altura.

La rueda permite acercarse por debajo del antiguo límite de 50 cm. Al llegar a 2 cm del foco orbital, sigue
avanzando cámara y foco 5 cm por muesca. No es una distancia mínima respecto a una pared: la cámara es libre y puede
atravesar objetos. F permite salir de una vista demasiado cercana. La rueda hacia atrás vuelve a alejarse.

**Velocidad** está visible al diseñar, recorrer, en Foto y en Opciones → Controles. El nivel inicial es **5 (3 m/s)**. Los niveles permiten
0,1; 0,25; 0,5; 1; 3; 5; 10; 20; 40 y 80 m/s. Elige 1–3 para inspeccionar detalles o 6–10 para recorrer el edificio.
Las teclas superiores **1–9** eligen esos niveles y **0** elige el nivel **10**; puedes cambiar mientras mantienes WASD. Al trazar una pared, los números siguen escribiendo su medida. Los campos de texto y menús conservan la prioridad; Ctrl/Alt/Shift y el teclado numérico no activan este atajo.
La elección se guarda localmente en `GameUserSettings.ini`, sección `Floor101.Camera`, y se recupera al arrancar.
Acercarte, encuadrar o cambiar a plano 2D conserva la velocidad elegida; Shift la multiplica por cuatro.

El movimiento diagonal no es más rápido. Al escribir en un campo,
abrir consola, usar un atajo con Ctrl/Alt o cambiar de aplicación, el desplazamiento se detiene. Se requiere una
pulsación nueva al recuperar el foco. En Modo Foto, mover o mirar reinicia las muestras, como el zoom y la órbita.

## Validación

```powershell
.\Scripts\Build.ps1
.\Scripts\Test.ps1 -Integration
```

Las regresiones `CameraCloseZoom`, `CameraMovement` y `Floor101.Navigation.*` comprueban acercamiento, avance después
del foco, cámara centrada al mirar, direcciones, diagonales, tiempo, plano 2D y prioridad de los atajos.
`CameraSpeedPreference` comprueba el guardado y la recarga de la velocidad; `CameraMovement` verifica niveles,
Shift y que el zoom no reduzca la velocidad.
Compilación Win64 Development, 265 tests unitarios (12.892 aserciones) y 67 tests Unreal pasaron el 2026-10-03.
Decisión: [ADR-0024](adr/ADR-0024-camera-navigation.md).

![Navegación, nivel de velocidad, sincronización vertical y Salir](images/camera-navigation.png)

## Memoria de las vistas (0.1.7)

Tab conserva dos encuadres independientes: posición y zoom del plano 2D, y posición, zoom y orientación de la
cámara 3D. La primera entrada en 2D usa el centro y escala actuales; volver a una vista recupera el último
encuadre de esa vista. **F** sigue encuadrando explícitamente la vista activa. Abrir o crear otro proyecto descarta
el encuadre anterior; cambiar de planta actualiza también la altura del encuadre guardado.

La regresión `Floor101.Interaction.PlanViewMemory` cubre alternancias repetidas, desplazamiento, zoom, orientación,
encuadre explícito, cambio de altura y reinicio. La comprobación portable pulsa Tab en el ejecutable Shipping.

Validación 0.1.7: compilación Win64 Development sin advertencias nuevas, 315 casos C++ / 14.064 aserciones y 87
pruebas Unreal aprobadas. Reporte: `Saved/Automation/20261005-192519`.

Portable Windows **0.1.7 Shipping** aprobado, generado desde `4e30d1f` con árbol limpio. El test pulsa Tab y verifica
`tab_restores_3d_view=PASS` y `tab_restores_2d_view=PASS` con encuadres y zoom distintos. Reporte:
`Saved/PackageChecks/20261005-193542-functional`. Ejecutable: `Packaged/0.1.7/Windows/Floor101.exe`.
La validación corresponde a este equipo; sigue pendiente probar en otro equipo sin Unreal instalado.

## Mirar desde la posición actual (0.1.8)

En 3D, mantén **Shift + botón derecho** y arrastra para mirar a los lados o arriba/abajo sin desplazar la cámara.
Funciona en Diseñar y Foto sin activar N. El botón derecho solo conserva la órbita; en Recorrer sigue mirando.
Al comenzar a mirar, el gesto mantiene ese comportamiento hasta soltar el botón, incluso si sueltas Shift primero.
Un giro pequeño tampoco cancela la herramienta de construcción. Abrir un menú o perder el foco termina el gesto.
En 2D se conserva la orientación del plano; usa el botón central o WASD para desplazarlo.

Validación: Win64 Development compilado sin advertencias nuevas; 315 casos C++ / 14.064 aserciones y 88 pruebas
Unreal Automation aprobadas. Reporte: `Saved/Automation/20261006-175123`. `Floor101.Navigation.LookInPlaceGesture`
comprueba posición fija, dirección, orden de liberación, órbita, cancelación de construcción, 2D y pérdida de foco/menús.

La ayuda se revisó en capturas reales a 1280×720 y 800×600.

![Controles: mirar desde la posición actual](images/camera-look-controls.png)
![Controles en ventana pequeña](images/camera-look-controls-small.png)

Portable Windows **0.1.8 Shipping** aprobado, generado desde `ef54060` con árbol limpio. El ejecutable comprueba
`look_in_place_without_n=PASS` y `look_latches_until_release=PASS`; integridad SHA-256, dependencias locales y
regresiones funcionales aprobadas. Reporte: `Saved/PackageChecks/20261006-180006-functional`.
Ejecutable: `Packaged/0.1.8/Windows/Floor101.exe`. ZIP: `Packaged/0.1.8/101st-Floor-portable.zip`.
La comprobación se realizó en este equipo; la prueba en otro equipo sin Unreal sigue pendiente.
