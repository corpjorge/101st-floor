# ADR-0002: CAD Core, Geometry y Scene Model en C++ estándar, independientes de Unreal

Estado: aceptado (Fase D, 2026-09-28)

## Contexto
El CAD Core es el activo principal y debe sobrevivir a cambios de motor, de MCP y de exportadores. La geometría crítica
tiene que poder testearse en segundos sin arrancar Unreal. Al mismo tiempo, la app de Unreal debe compilar ese mismo
código sin fricción.

## Alternativas
| Opción | Ventajas | Riesgos |
|---|---|---|
| Módulos de Unreal con tipos de Unreal (`FVector`, `UObject`) | cero fricción en Unreal | acopla el núcleo al motor; tests solo con Unreal (lentos); contrario al prompt maestro |
| Librería externa (CMake) precompilada y enlazada como `.lib` en Unreal | independencia total | dos sistemas de build en la ruta de Unreal; desajustes de CRT y flags; iteración lenta |
| Low Level Tests de UE para tests sin editor | framework oficial | requiere compilar targets de programa contra el motor; lento; acoplado a la instalación |
| **Módulos de Unreal con código C++ puro + CMake solo para tests** | una única ubicación de código; UBT lo compila con normalidad; CMake da tests en segundos y **hace cumplir** la independencia (no conoce cabeceras de Unreal) | hay que respetar las restricciones de compilación de Unreal |

## Decisión
- `Floor101CadCore`, `Floor101Geometry` y `Floor101SceneModel` son módulos de Unreal cuyo código es C++20 estándar.
  La única excepción es un fichero `…Module.cpp` con `IMPLEMENT_MODULE`, que usa `Core` y CMake excluye.
- Restricciones, alineadas con los defaults de UE 5.8 (confirmados en UBT): C++20, **sin excepciones**, **sin RTTI**.
  Errores → `Result<T>`/`Status`; errores de programación → `F101_ASSERT`. Nada de identificadores que Unreal define
  como macro (`check`, `verify`, `ensure`, `TEXT`).
- Tests unitarios: CMake + Ninja de VS Build Tools 2026 (localizados con `vswhere`, no por `PATH`), MSVC 14.50 como
  UnrealBuildTool, **doctest** incluido en el repositorio (una cabecera, MIT, versión fijada). Flags de los módulos
  puros en CMake: `/W4 /WX /permissive- /EHs-c-`.
- Dirección de dependencias: CadCore ← Geometry ← SceneModel ← UnrealAdapter ← Floor101. Ningún módulo puro depende
  de Unreal, UI, MCP, Twinmotion ni RealityScan.
- (MVP 8, ADR-0015) `Floor101Drawing` (planos 2D y SVG) es otro módulo puro con las mismas reglas: CadCore ← Geometry ←
  Drawing ← Floor101.

## Consecuencias
- Dos compilaciones del mismo código (UBT y CMake): una incompatibilidad se detecta pronto y en un solo sitio.
- El adaptador convierte tipos puros a tipos de Unreal. Es un coste pequeño y localizado.
- Se permite `std::` (containers, `<cmath>`, `<optional>`, `<span>`). Las librerías de terceros en el núcleo requieren
  compilar sin excepciones.

## Verificación
`Test.ps1`: compila y ejecuta los tests CMake; una prueba de arquitectura falla si un fichero puro incluye una cabecera
de Unreal o si el `Build.cs` de un módulo puro declara dependencias distintas de `Core`. `Build.ps1` compila los mismos
ficheros con UBT.
