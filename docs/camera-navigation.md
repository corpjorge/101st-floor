# Navegación y zoom

Activa **Navegar (N · WASD)** desde la barra o Modo Foto. También puedes pulsar **N** cuando el viewport tiene foco.

| Entrada | Acción |
|---|---|
| W / S | avanzar / retroceder según la dirección de la cámara |
| A / D | izquierda / derecha |
| Q / E | bajar / subir |
| Shift + movimiento | cuatro veces más rápido |
| Selector Velocidad | niveles 1–10; muestra la velocidad en m/s y guarda la elección |
| Derecho arrastrando | mirar desde la posición de la cámara, incluso hacia arriba |
| N / Esc | volver a editar |
| F | encuadrar el proyecto y recuperar una vista general |

En edición puedes mantener **derecho + WASD/QE** para desplazarte temporalmente. Sin navegación, W sigue siendo
Muro y A sigue siendo Amueblar. Un derecho arrastrado sin teclas de movimiento conserva la órbita habitual.
En plano 2D, WASD desplaza el plano sobre los ejes de pantalla; Q/E no cambian su altura.

La rueda permite acercarse por debajo del antiguo límite de 50 cm. Al llegar a 2 cm del foco orbital, sigue
avanzando cámara y foco 5 cm por muesca. No es una distancia mínima respecto a una pared: la cámara es libre y puede
atravesar objetos. F permite salir de una vista demasiado cercana. La rueda hacia atrás vuelve a alejarse.

**Velocidad** está junto a Navegar, también en Modo Foto. El nivel inicial es **5 (3 m/s)**. Los niveles permiten
0,1; 0,25; 0,5; 1; 3; 5; 10; 20; 40 y 80 m/s. Elige 1–3 para inspeccionar detalles o 6–10 para recorrer el edificio.
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
Compilación Win64 Development, 265 tests unitarios (12.892 aserciones) y 66 tests Unreal pasaron el 2026-10-03.
Decisión: [ADR-0024](adr/ADR-0024-camera-navigation.md).

![Navegación, nivel de velocidad y sincronización vertical](images/camera-navigation.png)
