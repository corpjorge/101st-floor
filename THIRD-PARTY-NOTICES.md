# Avisos de terceros · Third-party notices

101st Floor se publica con la licencia MIT (`LICENSE`), que cubre el código, los scripts, la documentación y los
recursos propios del proyecto. Los componentes de terceros siguientes conservan sus propias licencias.

101st Floor is released under the MIT License (`LICENSE`), which covers the project's own code, scripts,
documentation and assets. The third-party components below keep their own licenses.

## Unreal® Engine

101st Floor uses the Unreal® Engine. Unreal® is a trademark or registered trademark of Epic Games, Inc. in the United
States of America and elsewhere.

Unreal® Engine, Copyright 1998 – 2026, Epic Games, Inc. All rights reserved.

El motor (sus bibliotecas y el contenido cocinado del motor que acompañan a la aplicación) se distribuye bajo el
Acuerdo de Licencia de Usuario Final de Unreal Engine: <https://www.unrealengine.com/eula>. La licencia MIT de este
proyecto no se aplica al motor. Compilar 101st Floor desde el código requiere aceptar ese acuerdo con una cuenta de
Epic Games.

## nlohmann/json

Compilado dentro de la aplicación (lectura y escritura de `project.json`, manifiestos de la biblioteca y glTF).
<https://github.com/nlohmann/json>

```
MIT License

Copyright (c) 2013-2025 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## doctest

Solo en las pruebas unitarias del repositorio; no forma parte de la aplicación distribuida.
<https://github.com/doctest/doctest>

```
The MIT License (MIT)

Copyright (c) 2016-2023 Viktor Kirilov

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## WiX Toolset

El instalador (`101st-Floor-<versión>-Setup.exe` y el `.msi`) se genera con WiX Toolset 5.0.2; su motor Burn va dentro
de `Setup.exe`. WiX Toolset se distribuye bajo la Microsoft Reciprocal License (MS-RL).
<https://github.com/wixtoolset/wix>

## Microsoft Visual C++ Redistributable

`Setup.exe` y la carpeta portable incluyen `vc_redist.x64.exe`, el runtime de Visual C++ que Microsoft permite
redistribuir y que Unreal Engine incluye para ello. Se instala con sus propios términos de licencia de Microsoft.

## Texturas fotográficas de Poly Haven (CC0)

Las 14 texturas de `Library/textures` proceden de [Poly Haven](https://polyhaven.com) y son de dominio público (CC0).
Agradecemos a sus autores:

| Textura | Autores |
|---|---|
| Laminate Floor 02 | Dario Barresi, Charlotte Baglioni |
| Wood Floor | Dimitrios Savva |
| Herringbone Parquet | Jenelle van Heerden, Sergej Majboroda |
| Diagonal Parquet | Jenelle van Heerden, Sergej Majboroda |
| Marble 01 | Rob Tuytel |
| Floor Tiles 06 | Rob Tuytel |
| Floor Tiles 08 | Rob Tuytel |
| Granite Tile | Charlotte Baglioni |
| Tatami Mat | Charlotte Baglioni |
| Red Brick | Rob Tuytel |
| Whitewashed Brick | Charlotte Baglioni |
| White Plaster 02 | Rob Tuytel |
| Stacked Stone Wall | Dimitrios Savva |
| Synthetic Wood | Dario Barresi, Charlotte Baglioni |

## Recursos propios

Los 53 muebles de `Library/furniture` se modelan con código en Blender (`Source/make_furniture.py`,
`Scripts/GenerateLibrary.ps1`) y forman parte del proyecto (MIT). Blender no se distribuye con la aplicación.

## Fuentes

La interfaz usa Segoe UI y Bahnschrift, que se leen de la instalación de Windows del equipo; no se incluyen ni se
redistribuyen.
