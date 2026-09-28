# ADR-0001: Milímetros enteros como unidad persistente

Estado: aceptado (Fase D, 2026-09-28)

## Contexto
101st Floor necesita precisión dimensional real: una pared de 3550 mm debe ser exactamente 3550 mm después de guardar,
cargar, editar y exportar. Unreal trabaja en centímetros `double` (LWC); glTF en metros `float`; el usuario escribe en
mm, cm o m, con punto o coma decimal.

## Alternativas
| Opción | Ventajas | Riesgos |
|---|---|---|
| `double` en metros | natural para trigonometría | 3.55 no es representable: aparecen 3549.9999; las igualdades y los diffs se vuelven frágiles; deriva al editar |
| `double` en mm | valores enteros exactos hasta 2^53 | nada impide guardar 3550.0000001; la exactitud no se garantiza por tipo |
| **`int64_t` en mm** | exacto, comparable, serializable sin pérdida, diff estable | hay que definir cuándo y cómo se redondea lo calculado |
| enteros en µm | submilimétrico | innecesario para arquitectura; complica la entrada y la UI |

## Decisión
- Coordenadas y dimensiones de la fuente de verdad: `int64_t` en milímetros. Resolución del modelo: 1 mm.
  Rango admitido: ±10^9 mm por coordenada, de sobra para arquitectura y exacto en `double`.
- Entrada del usuario: parser decimal **exacto**. Los dígitos se convierten a un racional entero sin pasar por `double`.
  Unidades `mm`, `cm` y `m`; separador decimal `.` o `,`; sin separador de miles. Sin unidad se aplica la unidad de entrada
  del proyecto, por defecto `cm` (`355` → 3550 mm, como en el prompt maestro). Redondeo a 1 mm, mitad lejos de cero
  (`3.5555 m` → 3556 mm).
- Cálculo geométrico en `double`. Cuando un valor calculado pasa a formar parte del modelo (intersección, snapping,
  rotación), se cuantiza **de forma explícita** en ese punto con la misma regla de redondeo. La geometría derivada
  (`MeshData`, escena) no se cuantiza.
- Conversión a consumidores en sus adaptadores: Unreal `mm / 10` → cm; glTF `mm / 1000` → m.

## Consecuencias
- Tests exactos (`== 3550`) en el modelo; tests con tolerancia solo sobre geometría derivada.
- Las operaciones que generan puntos nuevos deben pasar por una función de cuantización común, revisable en code review.
- Mostrar valores al usuario es formateo, no conversión del modelo.

## Verificación
Tests unitarios del parser (`355`, `355 cm`, `3.55 m`, `3,55 m`, `3550 mm` → 3550; redondeos; entradas inválidas) y
de la conversión a Unreal (3550 mm → 355 cm, 120 → 12, 2400 → 240).
