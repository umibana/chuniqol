# NativeTitleLevel — SDEZ 1.66

Mod para **maimai DX / SDEZ 1.66**, cargado con MelonLoader. Esta carpeta es independiente de los mods de CHUNITHM / SDHD del resto del repositorio.

Muestra la dificultad interna dentro del título nativo de la canción: `Título [11.4]`. Conserva la tipografía y el desplazamiento del juego. El decimal cambia con la canción, la dificultad y la carta Standard/DX. También se aplica a las cartas pequeñas y a UTAGE cuando tiene un nivel válido.

Lee `ScoreData[difficulty].level` y `levelDecimal` de la carta que el juego está generando. No altera el catálogo, los nombres enviados al servidor ni los resultados. Las dificultades deshabilitadas o con datos inválidos conservan su título original.

## Compatibilidad

- **Probado en SDEZ 1.66**, Unity 2018.4.7f1, MelonLoader 0.6.4, runtime `net35`.
- Coexiste con AquaMai; no requiere activar `[UX.SelectionDetail]`.
- No se garantiza compatibilidad con otras versiones. Si cambia la estructura esperada del método, el mod registra que no pudo activarse.
- La apariencia fue comprobada y aprobada por el usuario en el juego.

## Compilar

En Windows con .NET Framework 4.x, PowerShell y la instalación existente de MelonLoader:

```powershell
.\build.ps1 -GamePackage 'C:\ruta\SDEZ_1.66\Package'
```

El script ejecuta ocho pruebas de comportamiento y genera `build/NativeTitleLevel.dll`. Las dependencias se leen de la instalación del usuario. Los archivos del juego y las DLL de terceros no se incluyen en este repositorio.

## Instalar o desactivar

1. Cierra el juego.
2. Copia `build/NativeTitleLevel.dll` a `Package/Mods/NativeTitleLevel.dll`.
3. Inicia el juego con tu `start.bat` habitual.

Para desactivarlo, cierra el juego y retira esa DLL de `Package/Mods`.

## Implementación y validación

La modificación de `MusicSelectChainList.SetChainData` añade el sufijo antes de que el título llegue a `CustomTextScroll.SetData`. Usa la dificultad efectiva que el propio juego ya ha calculado. Cada actualización parte del nombre original, por lo que no acumula sufijos.

Las pruebas cubren decimales, cambios de dificultad para la misma canción, títulos japoneses, configuración regional española y datos inválidos. Se comprobó en ejecución el registro del mod y la lectura de notas reales: LOVE ＆ JOY mostró `[5.0]`, `[7.1]` y `[9.9]`; `[宴]jelly` mostró `[12.7]`.