# ADR-0039: distribución pública y actualizaciones firmadas

Estado: aceptado y verificado con el ejecutable Shipping 0.3.0, en local y contra el release publicado (2026-10-22).

## Contexto
101st Floor se entregaba como carpeta portable y como instalador (ADR-0028), generados y probados en este equipo, desde
un repositorio privado. El usuario quiere que cualquiera pueda descargarlo e instalarlo sin herramientas de desarrollo,
publicar el código con una licencia adecuada, sin datos personales, y que la aplicación detecte una versión nueva, la
descargue y la instale.

## Alternativas
- **Licencia:** MIT (elegida); Apache-2.0 (equivalente, con patentes y NOTICE); solo uso no comercial. La GPL no es
  compatible con enlazar Unreal Engine, cuyo código no puede publicarse bajo ella.
- **Actualizaciones:** actualizador propio sobre GitHub Releases y el instalador WiX existente (elegido); solo avisar
  con un enlace a la descarga; Velopack, que sustituiría el instalador y añadiría un framework de terceros; MSIX con
  App Installer, que exige firmar con un certificado de confianza pública.
- **Autenticidad de la actualización:** solo HTTPS y SHA-256 publicados junto al instalador (no protegen si alguien
  controla la cuenta que publica); firma Authenticode del ejecutable (requiere un certificado de pago); **manifiesto
  firmado con una clave del proyecto** (elegido: la clave privada nunca está en GitHub).

## Decisión
- **Licencia MIT** para el código y la documentación del proyecto (`LICENSE`); `THIRD-PARTY-NOTICES.md` recoge Unreal
  Engine (EULA de Epic, aviso de marca y copyright obligatorio), nlohmann/json y doctest (MIT), WiX (MS-RL, el motor
  Burn va dentro de `Setup.exe`), el runtime de Visual C++ y las texturas CC0 de Poly Haven. Ambos archivos van en el
  paquete y en el instalador.
- **Release en GitHub** por versión (`v<versión>`): `101st-Floor-<versión>-Setup.exe` (recomendado: Burn con los
  prerrequisitos y el MSI por usuario), el `.msi`, el ZIP portable, `SHA256SUMS.txt` y el manifiesto de actualización.
  `Scripts/Release.ps1` lo prepara desde un paquete ya comprobado y lo publica con `gh`.
- **Manifiesto firmado:** `101st-Floor-update.json` (producto, versión, nombre, tamaño y SHA-256 del `Setup.exe`) y
  `101st-Floor-update.json.sig`, firma ECDSA P-256 con SHA-256 (r‖s, 64 bytes, Base64) de los bytes exactos del
  manifiesto. La clave pública (X‖Y) está en el código; la privada la crea `Scripts/UpdateSigningKey.ps1` fuera del
  repositorio y solo la usa `Release.ps1`. Perderla obliga a publicar a mano una versión con una clave nueva.
- **Actualizador (`Floor101::Update`)**, en la aplicación, con el módulo HTTP del motor y la criptografía de Windows
  (CNG): al arrancar, si la preferencia local lo permite (por defecto sí), y desde Opciones → Acerca de, pide
  `releases/latest` del repositorio configurado (`[Floor101.Update] Repository` en `DefaultGame.ini`), descarga el
  manifiesto y su firma, la comprueba y solo ofrece versiones mayores que la propia. "Actualizar" descarga el
  `Setup.exe` en la carpeta temporal del usuario, comprueba tamaño y SHA-256, pide guardar si hay cambios (como Salir),
  lanza el instalador en modo pasivo y cierra la aplicación; al terminar el instalador se vuelve a abrir. La instalación
  por usuario no pide permisos de administrador salvo para el runtime de Visual C++ si falta. Una copia portable no se
  actualiza sola: ofrece abrir la página de la versión.
- **Sin tráfico oculto:** solo se consulta GitHub; no se envía nada del usuario ni del proyecto. Los fallos de red no
  interrumpen: se dicen en Opciones y se reintenta en el siguiente arranque.
- **Pruebas sin red:** `-F101UpdateFeed=<archivo>` lee un "release" local con rutas de archivo; `-F101UpdateVersion=`
  finge la versión actual; `-F101UpdateDryRun` comprueba todo sin lanzar el instalador; `CheckPackage.ps1 -Check update`
  lo ejecuta sobre el ejecutable Shipping.
- **Datos personales:** el historial se reescribe con el correo anónimo de GitHub antes de publicarlo. Las capturas de
  `docs/` se revisaron: solo muestran proyectos de prueba bajo la carpeta del repositorio.

## Consecuencias
Sin firma Authenticode, Windows SmartScreen avisa al ejecutar el instalador por primera vez ("Windows protegió su
equipo" → Más información → Ejecutar de todas formas); el README lo explica. Cada actualización descarga el instalador
completo (~400 MB); las actualizaciones parciales quedan para más adelante. Las versiones anteriores a la 0.3.0 no
tienen actualizador: se instalan a mano una vez. El repositorio público consume cuota de Git LFS al clonarse; las
descargas de Releases no.

## Verificación
Pruebas de Unreal: versiones, manifiesto, firma con un vector generado por .NET (la misma API que firma las releases)
y su alteración, elección de recursos de un release y flujo completo con un release local (`Floor101.Update.*`).
`CheckPackage.ps1 -Check update` con el `Setup.exe` real firmado; instalación, actualización y desinstalación con
`CheckInstaller.ps1`; tras publicar, la consulta real a GitHub desde la versión instalada.
