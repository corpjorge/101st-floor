# Biblioteca de 101st Floor

La biblioteca integrada (ADR-0019): muebles y texturas que la aplicación ofrece en Amueblar y en Pintar. Al usar un
elemento se añade al proyecto como un asset (se copia junto a `project.json`, igual que una importación), así que un
proyecto no depende de esta carpeta.

Cada subcarpeta tiene un `manifest.json` que describe sus archivos (formato en `Source/Floor101Assets/Public/F101/Assets/Library.h`).

| Carpeta | Contenido | Origen | Licencia |
|---|---|---|---|
| `furniture/` | 53 muebles (`.glb`, glTF 2.0 en metros) con su imagen (`.png`) | modelados en código por `Source/make_furniture.py` con Blender; se regeneran con `Scripts/GenerateLibrary.ps1` | del proyecto |
| `textures/` | 14 texturas fotográficas (mapa de color 1K, `.jpg`) con su tamaño real | [Poly Haven](https://polyhaven.com), descargadas el 2026-09-30; autores en el manifiesto | CC0 (dominio público) |

Los `.glb`, `.png` y `.jpg` van por Git LFS (`.gitattributes`).
