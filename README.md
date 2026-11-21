# Mi EQ

Ecualizador pasivo estilo Pultec (VST3, Windows 64 bits) pensado para SONAR.
Hace la misma función que un EQ pasivo clásico como el Reference 1956: curvas anchas y musicales, con *boost* y *atten* de graves a la vez ("low-end trick").

## Controles

| Sección | Control | Qué hace |
|---|---|---|
| LOW (graves) | Frecuencia | 20 / 30 / 60 / 100 Hz |
| | Boost | Sube los graves (hasta +14 dB) |
| | Atten | Baja los graves (hasta -16 dB). Usar los dos a la vez da el "low-end trick" |
| HIGH BOOST (agudos +) | Frecuencia | 3 / 4 / 5 / 8 / 10 / 12 / 16 kHz |
| | Boost | Sube los agudos (hasta +16 dB) |
| | Ancho | De estrecho a ancho |
| HIGH CUT (agudos -) | Frecuencia | 5 / 10 / 20 kHz |
| | Atten | Baja los agudos (hasta -16 dB) |
| OUTPUT | Drive | Saturación suave tipo válvula |
| | Salida | Volumen de salida (-12 a +12 dB) |
| Cabecera | MID / SIDE | Procesa el centro y los laterales en vez de L/R |
| | BYPASS | Desactiva el efecto |

La gráfica superior muestra en vivo la curva real que está aplicando el plugin.

## 1. Subirlo a tu GitHub

1. Crea en GitHub un repositorio vacío llamado `MiEQ` (sin README).
2. En una terminal, dentro de esta carpeta:

```
git init
git add .
git commit -m "Mi EQ - primera versión"
git branch -M main
git remote add origin https://github.com/TU_USUARIO/MiEQ.git
git push -u origin main
```

(También puedes arrastrar los archivos a la web de GitHub con "Add file > Upload files", incluida la carpeta oculta `.github`.)

## 2. Compilar (automático en GitHub)

Al subir el código, GitHub Actions compila solo el plugin en un Windows de 64 bits:

1. Entra en tu repositorio > pestaña **Actions** > ejecución "Compilar Mi EQ (VST3 Windows)".
2. Cuando termine con ✔ verde, baja a **Artifacts** y descarga `MiEQ-VST3-Windows`.
3. Descomprime: dentro está `Mi EQ.vst3`.

Si algún día falla la compilación, copia el mensaje de error de esa pestaña y se corrige.

## 3. Instalarlo en SONAR

1. Copia `Mi EQ.vst3` (es una carpeta) a `C:\Program Files\Common Files\VST3\`.
2. En SONAR: Preferencias > Plug-ins (VST) > Analizar / Scan de nuevo.
3. Aparece como **Mi EQ** en la categoría de efectos / EQ.

## Compilar en tu propio PC (opcional)

Necesitas Visual Studio 2022 (carga "Desarrollo para el escritorio con C++") y CMake:

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

El resultado queda en `build\MiEQ_artefacts\Release\VST3\`.

## Licencia

Está construido con [JUCE](https://juce.com), que es AGPLv3 o licencia comercial. Para uso personal y repositorio público no hay problema; si algún día lo vendes, necesitarás licencia comercial de JUCE.
