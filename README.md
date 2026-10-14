# Susurro

Noise vocoder (el efecto "whisper" del Waves Morphoder). La voz que entra se
analiza en bandas y su envolvente se imprime sobre ruido filtrado: sale un
susurro que sigue lo que se dice.

## Controles

Por defecto replica el preset **Whisper** del Morphoder: ruido pasado por un vocoder de
5 bandas anchas (258 Hz pasabajos, 689 / 1809 / 4436 Hz, 11025 Hz pasaaltos), con
pasaaltos de 620 Hz y +14 dB en 4.4 kHz. Pocas bandas = susurro ruidoso y parejo, pero que se entiende.

| Control    | Qué hace |
|------------|----------|
| Smoothing  | Qué tan rápido sigue a la voz al subir (por defecto 10 ms). |
| Release    | Qué tan rápido suelta (por defecto 120 ms). |
| Pressure   | Compresión del nivel: 0% sigue la dinámica de la voz, 100% volumen constante (por defecto 20%). |
| Noise HPF  | Pasaaltos del ruido (Noise Filter/Freq del Morphoder, por defecto 620 Hz). |
| Tracking   | 100% (por defecto) = el ruido sigue el timbre de la voz por bandas, como el Morphoder. 0% = solo sigue el volumen: puro ruido de timbre fijo. |
| 258 Hz … 11 kHz | EQ de 5 bandas sobre el ruido. Por defecto +14 dB en 4.4 kHz, como el preset. |
| Bands      | Cantidad de bandas (3–40). Con 5 usa el layout exacto del preset Whisper. Más bandas = más parecido a la voz. |
| Formant    | Corre el timbre del susurro (0.5×–2×). |
| Width      | Decorrelación del ruido entre L y R (0 = mono, 100 = ancho). |
| Mix        | Seco / susurro. |
| Output     | Nivel de salida (por defecto -10 dB, como el fader Noise del preset; queda parejo con la voz seca). |

## Compilar en Mac

Requisitos (una sola vez):

```bash
xcode-select --install        # herramientas de compilación de Xcode
brew install cmake            # o bajar CMake desde cmake.org
```

Compilar (desde esta carpeta):

```bash
cd ~/Music/Susurro
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j8
killall -9 AudioComponentRegistrar 2>/dev/null; echo listo
```

El build copia solo el plugin a `~/Library/Audio/Plug-Ins/Components/Susurro.component`
(AU, para Logic) y `~/Library/Audio/Plug-Ins/VST3/Susurro.vst3`.
La primera vez tarda unos minutos porque descarga JUCE.

## Compilar en GitHub (sin Xcode)

Al subir el proyecto a un repo de GitHub, la acción `.github/workflows/build.yml`
compila el plugin en una Mac de GitHub. En la pestaña **Actions** → último run →
**Artifacts** → `Susurro-macOS` se descargan `Susurro-AU.zip` y `Susurro-VST3.zip`.

Instalar lo descargado:

```bash
cd ~/Downloads
unzip -o Susurro-AU.zip -d ~/Library/Audio/Plug-Ins/Components/
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/Susurro.component
killall -9 AudioComponentRegistrar 2>/dev/null; echo listo
```
