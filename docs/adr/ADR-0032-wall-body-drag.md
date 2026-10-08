# ADR-0032: arrastre del cuerpo de una pared

Estado: aceptado.

## Contexto
Seleccionar una pared la resalta en naranja, pero solo sus asas amarillas iniciaban un arrastre. Se necesita mover
la pared completa sin cambiar su longitud ni romper las uniones de sus extremos.

## Decisión
- Añadir `TranslateWallCommand{wall, delta}` al núcleo. Trasladar los dos extremos originales y cada extremo unido
  a ellos en la misma planta, comparando siempre con el estado previo. Registrar todas las paredes en una única
  transacción validada; conservar huecos, acabados e identidades y usar la división existente de ADR-0025.
- `WallBodyDrag` retiene la pared original, el punto de agarre en el plano del suelo y el desplazamiento cuantizado.
  Usar ese mismo plano durante todo el gesto, evitando saltos al cruzar otra pared en perspectiva. No ajustar a
  cuadrícula ni ángulos: el desplazamiento debe conservar el punto agarrado y la forma del muro.
- Mantener la prioridad de asas, huecos, muebles y otros objetos. El cuerpo inicia el arrastre al seleccionarlo;
  previsualizar todas sus uniones sin modificar el proyecto. Conservar la última vista previa válida ante paredes
  degeneradas o huecos que no caben. Soltar ejecuta un comando; cancelar o no mover no registra historial.
- Mantener el esquema JSON 13: solo cambia interacción y comandos, sin nuevos datos persistentes.

## Verificación
Regresiones C++ para traslación horizontal, vertical y diagonal, agarre exacto, perspectiva atravesando otras
paredes, uniones en ambos extremos, plantas, huecos, acabados, límites, rechazo atómico, cancelación, guardado,
división automática y deshacer/rehacer. Unreal Automation verifica vista previa, actores, habitación cerrada,
nombre, rodapiés e historial. La comprobación Shipping prueba el gesto y su deshacer en la aplicación empaquetada.
