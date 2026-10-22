# Publicar una versión

Cómo se publica 101st Floor en GitHub Releases para que cualquiera lo descargue y las copias instaladas se
actualicen solas ([ADR-0039](adr/ADR-0039-public-release-and-updates.md)). Todo desde PowerShell 7, con el editor y la
aplicación cerrados y `gh` autenticado con permiso para crear releases.

## La clave de firma

Las copias instaladas solo aceptan versiones cuyo manifiesto (`101st-Floor-update.json`) está firmado con la clave del
proyecto. Su mitad pública está en `Source/Floor101/Private/Floor101Update.cpp` (`UpdatePublicKey`); la privada, fuera
del repositorio:

```powershell
.\Scripts\UpdateSigningKey.ps1        # muestra la clave pública de la privada existente
.\Scripts\UpdateSigningKey.ps1 -New   # solo la primera vez: crea %APPDATA%\101st Floor Release\update-signing-key.pem
```

**Guarda una copia de seguridad del `.pem`** en un lugar seguro y nunca la subas al repositorio. Sin ella, ninguna copia
instalada aceptará una versión nueva: habría que publicar una versión con otra clave e instalarla a mano. `Release.ps1`
se niega a firmar con una clave distinta de la que confía la aplicación.

## Pasos

1. Sube la versión en `Config/DefaultGame.ini` (`ProjectVersion`), haz commit y deja el árbol limpio.
2. Pruebas: `.\Scripts\Test.ps1 -Integration`.
3. Paquete y comprobaciones del ejecutable Shipping real:
   ```powershell
   .\Scripts\Package.ps1 -Output Packaged/<versión> -Zip
   .\Scripts\CheckPackage.ps1 -Package Packaged/<versión>/Windows
   .\Scripts\CheckPackage.ps1 -Package Packaged/<versión>/Windows -Check home -ResX 1920 -ResY 1080
   ```
4. Instalador y su prueba (instala, comprueba accesos directos y desinstala en carpetas aisladas):
   ```powershell
   .\Scripts\Installer.ps1 -Package Packaged/<versión>/Windows -RestoreTools
   .\Scripts\CheckInstaller.ps1 -Msi Packaged/<versión>/Installer/101st-Floor-<versión>.msi -Setup Packaged/<versión>/Installer/101st-Floor-<versión>-Setup.exe
   ```
5. Prepara el release: firma el manifiesto y escribe `SHA256SUMS.txt`, las notas y `local-release.json` en
   `Packaged/<versión>/Release`:
   ```powershell
   .\Scripts\Release.ps1 -Package Packaged/<versión>
   ```
6. Comprueba la actualización con el ejecutable real y ese release local: se hace pasar por la versión 0.0.1, encuentra
   la nueva, comprueba la firma, descarga el instalador y verifica tamaño y SHA-256, sin instalar nada:
   ```powershell
   .\Scripts\CheckPackage.ps1 -Package Packaged/<versión>/Windows -Check update -UpdateFeed Packaged/<versión>/Release/local-release.json
   ```
7. Publica (crea la etiqueta `v<versión>` y sube instalador, MSI, ZIP portable, manifiesto, firma y sumas):
   ```powershell
   .\Scripts\Release.ps1 -Package Packaged/<versión> -Publish
   ```
   `-Target main` si el commit del paquete ya no existe en GitHub.
8. Comprueba la publicación con la consulta real a GitHub:
   ```powershell
   .\Scripts\CheckPackage.ps1 -Package Packaged/<versión>/Windows -Check update
   ```
9. Anota la entrega en [distribución Windows](distribution.md).

El repositorio en el que busca la aplicación es `[Floor101.Update] Repository` de `Config/DefaultGame.ini`; un fork
cambia ese valor y su propia clave.

## Firma Authenticode

Los ejecutables no llevan firma Authenticode, por eso Windows SmartScreen avisa la primera vez. Firmarlos (un
certificado de firma de código, o un servicio como Azure Trusted Signing) quitaría ese aviso; el manifiesto firmado
del actualizador no depende de ello.
