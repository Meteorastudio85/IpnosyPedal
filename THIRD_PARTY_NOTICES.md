# Componenti di terzi

Ipnosy (Meteora Plugins) usa i seguenti componenti di terzi. Ognuno resta soggetto
alla propria licenza, indipendentemente dal file `LICENSE` del progetto.

## Airwindows Tube2

Lo stadio valvolare (`Source/Tube2Stage.h`, preso da Marley) deriva da Airwindows Tube2.

```
MIT License

Copyright (c) 2016 airwindows (Chris Johnson)

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

Sito: https://www.airwindows.com

## JUCE

Il plugin e' costruito con il framework [JUCE](https://juce.com) (scaricato in automatico
da CMake al momento della compilazione, non incluso in questo repository).

JUCE e' distribuito con doppia licenza: **AGPLv3** oppure **licenza commerciale JUCE**.
Per distribuire Ipnosy con una licenza chiusa ("tutti i diritti riservati") serve una
licenza commerciale JUCE valida (Meteora Plugins deve verificare sul sito juce.com quale
piano si applica); in alternativa il progetto andrebbe distribuito sotto AGPLv3.

VST e' un marchio di Steinberg Media Technologies GmbH. Audio Units e' un marchio di Apple Inc.

## NeuralAmpModelerCore

Le catture NAM vengono eseguite con [NeuralAmpModelerCore](https://github.com/sdatkinson/NeuralAmpModelerCore)
di Steven Atkinson (licenza MIT), scaricato in automatico da CMake al momento della compilazione.

## Catture NAM

I file in `Resources/models/` sono le catture "EQD Acapulco Gold Clone" (ore 9, ore 12, ore 15)
di markswarbrick, pubblicate su TONE3000. Prima di distribuire il plugin pubblicamente
verificare la licenza delle catture.
