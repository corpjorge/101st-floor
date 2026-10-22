# ADR-0038: Cubiertas translúcidas por habitación y acabado independiente del vidrio

Estado: implementado y verificado (2026-10-20).

## Contexto

El usuario requiere tejas translúcidas en patio y garaje, una puerta acristalada al patio y un portón con vidrio
reflectivo. Una losa opaca con pintura translúcida en su cara inferior sigue bloqueando la luz. El vidrio de los huecos
necesita un acabado propio, separado del marco y de la hoja.

## Decisión

- Esquema 17: `Project::roomRoofs`, por la misma clave derivada de habitación que nombres y rodapiés. Una entrada
  `RoomRoof{room,riseMm,fallQuarterTurns}` sustituye la losa superior por una cubierta ligera de teja ondulada opal.
  Ausencia significa losa normal. Desnivel entero de 0 a 1500 mm; caída 0 sur, 1 este, 2 norte, 3 oeste.
- El alero bajo se sitúa en la coronación de la planta. La geometría pura genera tejas de 3 mm, onda de paso 177 mm
  y relieve de 24 mm, perfiles blancos y cierres translúcidos del perímetro. Las superficies se recortan contra los
  triángulos de la habitación, por lo que se respetan los entrantes de polígonos cóncavos.
- Solo se sustituye la cubierta de la última planta. Si se añade una planta por encima se conserva el forjado que la
  soporta. La cubierta sigue el régimen de visibilidad de los forjados: oculta al editar su planta, visible en Ver todas,
  Recorrer y Foto. No hay losa opaca superpuesta sobre la teja.
- Comando `SetRoomRoofCommand` y controles en el inspector de habitación: activar teja translúcida, desnivel y girar la
  caída 90°. Deshacer, rehacer, notificación de cambios y transferencia de claves al dividir muros conservan el ajuste.
- `Opening::glazing`: id opcional de un acabado del catálogo para vidrio; vacío conserva `glass-clear`. Se valida,
  serializa y usa en el slot de vidrio; cambiarlo no modifica el acabado del marco. Se edita en el inspector del hueco.
- `glass-reflective` usa una aproximación PBR gris reflectiva, opacidad 0,35 y factor metálico 0,65; `polycarbonate-opal`
  opacidad 0,28. El factor metálico del catálogo se transmite al material generado y al GLB. Materiales regenerables
  mediante `GenerateAssets.ps1`; los modelos importados mantienen su interpretación anterior.
- Se añade una baldosa gris mate de 30 × 30 cm para representar pavimento de garaje. El proyecto describe un diseño:
  no certifica resistencia mecánica, antideslizamiento ni dimensionado de la estructura de cubierta.

## Límites

No es un sistema general de cubiertas ni un cálculo estructural o de drenaje. No incluye huecos de forjado ni cambia
las plantas. El efecto de privacidad del vidrio reflectivo depende de que el exterior esté más iluminado; no se
promete visión unilateral permanente, especialmente de noche con luces interiores. La UI explica esa condición.
Referencia técnica: [3M, privacidad y equilibrio de iluminación](https://multimedia.3m.com/mws/media/1160043O/3m-safety-security-window-film-privacy-segment-card.pdf).

## Verificación

Pruebas puras de persistencia, versiones anteriores, rechazo de valores, transacciones de cubierta, geometría cóncava
en cuatro orientaciones, normales, triángulos no degenerados, sustitución exclusiva de la última losa y materiales
GLB. Unreal Automation verifica translucidez y parámetros del material generado; Shipping comprueba guardado y
reapertura y los materiales realmente cocinados.

Resultado: 355 pruebas unitarias, 987.971 aserciones; 94 pruebas Unreal Automation; 117 comprobaciones del
ejecutable Shipping 0.2.6, todas correctas. Informes en `Saved/InteriorDesign/20261020/roof-integration.log`,
`Saved/Automation/20261020-230508` y `Saved/PackageChecks/20261020-231049-functional`. La vivienda real se abrió
en Shipping, y su distribución pasó 710 comprobaciones sin fallos. Imágenes Blender del GLB exportado, con las
cubiertas reales en las vistas interiores y las paredes cortadas solo en las vistas de distribución.
