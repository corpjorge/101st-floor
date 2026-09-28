# Scene Model (`Source/Floor101SceneModel`, `f101::scene`)

Escena neutral compilada desde el modelo (ADR-0003). C++20 puro. Consumidores: Unreal hoy; exportadores y vista 2D en
el futuro.

- `compileScene(project)`: un `SceneNode` por pared, en el orden del modelo. Es determinista: el mismo proyecto da la
  misma escena.
- `SceneNodeId`: `"<wall uuid>/body"`. Derivado, nunca aleatorio, de modo que el mismo muro conserva su nodo al editarse.
- `contentHash`: FNV-1a 64 sobre posiciones, normales, UV y secciones. Los consumidores lo usan para no reconstruir lo que
  no cambió.
- Metadatos de origen: `sourceKind` y `sourceId` (UUID de la entidad) y `name` legible (`Wall 1a2b3c4d`).
- Espacio: mm, dextrógiro, X este, Y norte, Z arriba. Transform de nodo identidad en MVP 0 (geometría en coordenadas de
  escena).

La escena nunca se guarda como proyecto ni se edita. Un exportador escribe contra ella con su única conversión de
ejes (glTF: `(x, z, −y) / 1000`).

Tests: `Tests/Unit/SceneTests.cpp` (ID esperado, determinismo, el hash sigue a la geometría, borrado).
