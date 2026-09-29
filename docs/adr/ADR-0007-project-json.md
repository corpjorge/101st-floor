# ADR-0007: Formato `project.json` con nlohmann/json

Estado: aceptado (MVP 1, 2026-09-28)

## Contexto
El nivel de Unreal no es almacenamiento (prompt maestro §24). El proyecto necesita un formato propio, legible y
versionable (`project.json`) con `schemaVersion`, unidades e ID persistentes. El código vive en el CAD Core puro, que
compila sin excepciones (ADR-0002), y los milímetros deben leerse de forma exacta (`int64`).

## Alternativas
| Opción | Ventajas | Riesgos |
|---|---|---|
| Json de Unreal (`FJsonObject`) | ya disponible en el motor | prohibido en el núcleo puro |
| Parser JSON propio | control total | reinventar y probar un parser; riesgo de errores de escape y números |
| yyjson (C) | rápido, sin excepciones | ficheros C en un módulo de Unreal; menos habitual |
| **nlohmann/json 3.12.0** (cabecera única, MIT) | estándar de facto; `int64` exacto; objetos con orden de inserción (`ordered_json`) | sin excepciones, `throw` se convierte en `abort()`: solo se usa `parse` sin excepciones y se comprueba el tipo antes de cada lectura |

## Decisión
- `F101/Cad/ProjectFile.h`: `writeProjectJson`, `readProjectJson`, `saveProjectFile` y `loadProjectFile`. nlohmann
  solo se incluye en `ProjectFile.cpp`, con los avisos de terceros silenciados. Está en `ThirdParty/nlohmann`.
- Formato v1:
  ```json
  {
    "format": "101st-floor-project",
    "schemaVersion": 1,
    "units": "mm",
    "walls": [ { "id": "<uuid>", "start": [x, y], "end": [x, y], "thicknessMm": 120, "heightMm": 2400 } ]
  }
  ```
  Salida determinista (orden de claves fijo, sangría de 2, salto de línea final), cómoda para diffs en Git.
- Lectura estricta: formato y unidades obligatorios; se rechaza un `schemaVersion` mayor (en lugar de leerlo mal);
  los mm deben ser enteros dentro del rango del modelo; UUID válidos y únicos; cada pared válida. Los campos
  desconocidos se ignoran.
- Guardado atómico: se escribe `project.json.tmp` y se renombra, para que un fallo nunca trunque un proyecto.
  Rutas en UTF-8 (`path::string()` abortaría con caracteres no ANSI).
- Niveles, habitaciones, huecos y materiales llegarán en versiones de esquema posteriores, con migración desde v1.
  **Esquema 2 (MVP 3, ADR-0010):** cada muro puede llevar `openings` (puertas y ventanas), escrito solo si hay
  alguno; los archivos v1 se leen sin cambios.
  **Esquema 3 (MVP 4, ADR-0011):** `levels` y la planta (`level`) de cada muro; los archivos v1 y v2 se leen en una
  sola planta.
  **Esquema 4 (MVP 5, ADR-0012):** `finishes` (`left`, `right`) en los muros y `roomFinishes` por clave de habitación,
  solo si hay alguno.
  **Esquema 5 (MVP 6, ADR-0013):** `finish` (acabado de la carpintería) en cada hueco que lo tenga; los esquemas
  anteriores se leen con la carpintería por defecto.
  **Esquema 6 (MVP 7, ADR-0014):** `operation`, `hinge`, `opensTo` y `openPercent` en cada hueco; los esquemas anteriores
  se leen como puertas abatibles cerradas y ventanas fijas.

## Verificación
`Tests/Unit/ProjectFileTests.cpp`: salida exacta documentada, ida y vuelta sin pérdida (incluidos ±999.999.999 mm),
rechazos (JSON inválido, otro formato, versión nueva, decimales, texto, rango, ID duplicados o inválidos, paredes
degeneradas) y guardado y carga en una ruta con "ñ".
