# Catálogo de puertas y ventanas

El catálogo contiene 12 familias de puertas y 12 de ventanas. Se elige desde **Puerta (P)** o **Ventana (O)**,
con miniaturas del diseño, búsqueda por nombre y filtro por apertura. Las miniaturas representan la hoja cerrada.
Sin coincidencias se muestra un mensaje para cambiar la búsqueda o el filtro.

## Ejecutable portable 0.2.4

Para validar sin Unreal Editor, abre `Packaged/0.2.4/Windows/Floor101.exe`. Mantén completa la carpeta `Windows`,
con `Engine` y `Floor101` junto al ejecutable. El ZIP para trasladarla es `Packaged/0.2.4/101st-Floor-portable.zip`.
En ventanas pequeñas, abre **Propiedades** para ver los diseños y desplázate por el catálogo.

La entrega Shipping se verificó el 2026-10-20 con 103 comprobaciones funcionales correctas, incluido colocar y
guardar/reabrir familias de puertas y ventanas. También se abrieron los dos catálogos con el ejecutable portable
y se verificaron sus hashes y las dependencias locales. Detalle: [distribución Windows](distribution.md#entrega-024-2026-10-20).

## Familias

| Puertas | Ventanas |
|---|---|
| Abatible lisa | Fija panorámica |
| Abatible de dos paneles | Fija de dos paños |
| Abatible de cuatro paneles | Fija de seis paños |
| Abatible de seis paneles | Practicable sencilla |
| Abatible acristalada | Practicable con travesaño |
| Abatible acristalada con retícula | Practicable con retícula |
| Doble lisa | Doble practicable |
| Doble panelada | Doble con travesaños |
| Doble acristalada | Doble con retícula |
| Corredera empotrada lisa | Corredera sencilla |
| Corredera empotrada panelada | Corredera con travesaños |
| Corredera empotrada acristalada | Corredera con retícula |

## Uso

1. Activa Puerta o Ventana y elige un diseño. Se cargan sus medidas sugeridas.
2. Ajusta **Ancho**, **Alto** y, para ventanas, **Antepecho**. Puedes escribir `90`, `90 cm` o `0,9 m`.
3. Colócalo con un clic en un muro. Espacio invierte las bisagras; la cara apuntada determina hacia dónde abre.
4. En Seleccionar, elige el hueco para cambiar familia, medidas, carpintería y porcentaje de apertura.

Cambiar la familia de un hueco existente conserva sus medidas y acabado. El cambio se puede deshacer.
Las correderas empotradas necesitan un tramo libre al lado de igual ancho que la puerta. Si no cabe el nuevo
mecanismo, se rechaza el cambio y se conserva el anterior.

Los paneles y retículas se regeneran con las medidas, sin escalar una malla completa. El vidrio y los herrajes
siguen a la hoja al abrirla. El diseño aparece en 3D, Foto y exportación GLB; el plano y las exportaciones SVG/PNG
conservan la simbología de apertura y las divisiones pertinentes al corte en planta.

## Guardado y automatización

Se guarda el id de familia en `project.json` esquema 15. Los proyectos anteriores se abren con su aspecto original
y pueden recibir una familia desde el inspector. MCP: `GetOpeningFamilies`, `SetOpeningFamily`; los huecos
informan `Family`. [Decisión técnica](adr/ADR-0036-opening-catalogue.md).

## Validación

Verificación del 2026-10-19:

- `Scripts/Build.ps1`: Floor101Editor Win64 Development compilado con UE 5.8.3 y MSVC 14.50.
- `Scripts/Test.ps1 -Integration`: arquitectura correcta, 345 casos unitarios y 92 pruebas Unreal Automation correctos, sin fallos.
- Las pruebas nuevas comprueban las 24 familias a anchos de 300, 900 y 2400 mm y aperturas de 0, 45 y 100 %,
  el número de paños de vidrio, triángulos no degenerados, guardado y lectura, vista previa, colocación,
  cambio de familia, deshacer/rehacer y rechazo de bolsillos sin espacio. La integración verifica el mismo actor
  al abrir y cambiar el diseño, reapertura del archivo y exportación SVG, además de la API MCP.
- Informe de Unreal: `Saved/Automation/20261019-223503/index.json`.
- Capturas revisadas a 1920×1080, 1280×720 y 800×600: miniaturas derivadas de la geometría, nombres,
  selección, campos de medida, búsqueda, filtros de apertura, desplazamiento y mensaje sin resultados.

| Comprobación | Captura |
|---|---|
| Puertas, 1920×1080 | [Catálogo de puertas](images/opening-catalogue/doors-1920.png) |
| Ventanas, 1920×1080 | [Catálogo de ventanas](images/opening-catalogue/windows-1920.png) |
| Ventana pequeña, 800×600 | [Puertas en panel desplegable](images/opening-catalogue/doors-800.png) |
| Filtro, 1280×720 | [Ventanas correderas](images/opening-catalogue/windows-filter-1280.png) |
| Nombres largos, 1280×720 | [Puertas correderas](images/opening-catalogue/doors-filter-1280.png) |
| Búsqueda, 1280×720 | [Retícula y hueco en 3D](images/opening-catalogue/doors-search-1280.png) |
| Sin resultados, 800×600 | [Mensaje de búsqueda](images/opening-catalogue/doors-empty-800.png) |
| Ejecutable Shipping, 1920×1080 | [Puertas en el portable 0.2.4](images/opening-catalogue/portable-doors-1920.png) |
| Ejecutable Shipping, 800×600 | [Ventanas en el portable 0.2.4](images/opening-catalogue/portable-windows-800.png) |
