# Ipnosy – Meteora Plugins

Pedale distorsore VST3 (Windows e macOS): tre catture NAM di un clone dell'EarthQuaker Acapulco Gold,
più lo stadio valvolare di **Marley** (Airwindows Tube2) che si accende da ore 15.

## Controlli

| Controllo | Cosa fa |
| --- | --- |
| **Manopola centrale** (logo M) | Da 0 (silenzio) al massimo della distorsione. Tra zero e ore 9 sale il volume; da ore 9 a ore 12 e da ore 12 al massimo sfuma tra le tre catture. **Da ore 15 si accende la valvola**, che spinge sempre di più fino al massimo. Il logo ruota con la manopola. |
| **BLEND** (in basso a destra) | Mescola il segnale pulito con il pedale: tutto a sinistra solo pulito, tutto a destra solo pedale. Pulito e pedale sono allineati al campione: niente effetto phaser. |
| **Footswitch** | Accende/spegne il pedale. LED verde acceso = pedale attivo. |
| Input / Output | Regolazioni in dB disponibili solo come parametri nella DAW. |

Doppio clic su una manopola: torna al valore iniziale (Gain a ore 12, Blend al 100 %).

## Valvola

- È lo stadio di Marley: Tube2 in oversampling 4x a fase lineare (2x da 88,2 kHz) con autogain
  K-pesato, quindi accendendola il volume non salta.
- Quando la manopola arriva a ore 15 la valvola **si scalda in circa 1,6 secondi**: sia il suono sia
  la luce (prima il filamento, poi tutto il vetro) salgono insieme. Sotto ore 15 si raffredda in circa 1 secondo.
- La luce cresce con la manopola e con quanto suoni forte, con un leggero tremolio del filamento.
- Come in un pedale vero, la valvola resta accesa anche con il footswitch in bypass.

## Foglia

Con la manopola centrale al massimo la foglia diventa multicolore e i colori scorrono in continuazione.

## Compilazione (GitHub Actions)

Carica questa cartella su GitHub: il workflow `.github/workflows/build.yml` compila Windows e macOS
(Universal) a ogni push. I plugin sono in **Actions → ultima esecuzione → Artifacts**
(`Ipnosy-VST3-Windows`, `Ipnosy-VST3-macOS`).

## Installazione

- **Windows**: `Ipnosy.vst3` in `C:\Program Files\Common Files\VST3`
- **macOS**: `Ipnosy.vst3` in `~/Library/Audio/Plug-Ins/VST3`, poi nel Terminale:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Ipnosy.vst3
```

## Struttura

```
Source/PluginProcessor.*      catena audio: NAM -> valvola -> Blend -> footswitch
Source/PluginEditor.*         grafica: manopole, LED, valvola, foglia
Source/TubeEngine.h           valvola di Marley in versione mono, con riscaldamento
Source/Tube2Stage.h           da Marley (Airwindows Tube2)
Source/LinearPhaseOversampler.h, Source/AutoGain.h   da Marley
Source/Resampler.h            conversione a 48 kHz per le catture NAM
Resources/models/             le tre catture .nam (incorporate nel plugin)
Resources/gui/                foto del pedale e livelli animati
```

Latenza: 71 campioni a 48 kHz, 126 a 44,1 kHz (comunicata alla DAW). Pulito e pedale sono allineati al campione a qualsiasi frequenza.

## Licenza

Copyright (c) 2026 Meteora Plugins. Vedi `LICENSE` e `THIRD_PARTY_NOTICES.md`.
