# Aime automatico

`chuni-aime.dll` es un driver AimeIO **x64** para Segatools. Proporciona la
tarjeta configurada en `[aime] aimePath` de `segatools.ini` cuando el lector
solicita una consulta. No genera tarjetas ni envia pulsaciones de teclado.
Lee los 20 digitos de la tarjeta actual y los entrega como los mismos 10 bytes
BCD que utiliza la emulacion de Aime. La interfaz utilizada es AimeIO 1.0,
compatibilidad disponible en el hook instalado.

## Activar y desactivar

En `bin/chuni-aime.ini`:

```ini
[aime]
enabled=1
```

- `enabled=1`: tarjeta disponible automaticamente cuando el juego consulta.
- `enabled=0`: lectura manual con la tecla `[aime] scan` de `segatools.ini`,
  normalmente ENTER. El cambio se aplica en la siguiente consulta del lector.

Un archivo de configuracion ausente tambien deja el modo manual activo.
El numero de tarjeta permanece en `DEVICE/aime.txt`; no se copia al INI.
Para volver completamente al driver integrado, comenta
`path=chuni-aime.dll` en `[aimeio]` de `segatools.ini` y reinicia el juego.

## Compilar e instalar

```powershell
.\build-aime.ps1 -ZigPath C:\tools\zig\zig.exe -PythonPath C:\path\python.exe
# Juego y amdaemon cerrados:
.\install-aime.ps1 -GameBin C:\path\bin
```

El instalador conserva los demas ajustes y crea copias `*.before-aime-*.bak`.
Segatools carga el modulo a traves de su seccion `[aimeio]`; no necesita otro
`-k` en el lanzador. La ruta relativa de tarjeta se resuelve respecto del
archivo `segatools.ini`, independientemente del directorio de trabajo.
Si existe `SEGATOOLS_CONFIG_PATH`, se usa ese archivo de configuracion.

## Diagnostico

`bin/chuni-aime.log` registra inicializacion, modo AUTO/MANUAL, presencia y
errores, sin registrar el numero de tarjeta. La misma informacion se envia
a la salida de amdaemon que captura `bin/amdaemon-console.log`.
Tambien registra las rutas completas del INI y del archivo de tarjeta, y los
resultados de `GET AIME`: `00000000` significa que entrego el ID a Segatools;
`00000001` significa que todavia no hay tarjeta presente. Solo registra cambios
de resultado/parametros para evitar escribir una linea en cada consulta.

Para actualizar el diagnostico sin cambiar el modo elegido, cierra el juego
y amdaemon y ejecuta `install-aime.ps1 -GameBin C:\path\bin -PreserveMode`.
Despues inicia el juego con su lanzador habitual.
Espera en la pantalla inicial antes de elegir jugar como invitado. El log del
modulo se reinicia al inicializarse para distinguir cada arranque.

Un archivo ausente, mal formado o eliminado produce un error y no conserva
una identidad anterior. No existe fallback que genere otra tarjeta FeliCa.
El modulo no inicia sesiones por su cuenta: la entrada y los requisitos de
creditos/conexion siguen dependiendo del flujo normal del juego.

Las pruebas de integracion cargan la DLL real en procesos locales y cubren
identidad, activacion/desactivacion, archivos invalidos, lectores desconocidos,
buffers, rutas con espacios y privacidad de logs. No prueban el arranque
completo ni la respuesta del servidor de juego.
