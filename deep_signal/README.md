# DEEP SIGNAL

Sviluppato da **OpenAI Codex**, con direzione e feedback dell’utente. Codice,
grafica e mappa originali. [Articolo sulle difficoltà dello sviluppo](../articles/DEEP_SIGNAL_MEDIUM_IT.md).

Un platform shooter originale per Commodore 64 PAL, sviluppato da zero in C
(cc65) e Assembly 6502. Prima versione giocabile: una stazione, tre ripetitori,
quattro droni e una via di fuga.

![Demo registrata in VICE](deep_signal.gif)

Dalla radice del repository:

```sh
./deep_signal/build.sh
./deep_signal/run_vice.sh
```

Il programma pronto è [deep_signal.prg](deep_signal.prg). Servono cc65 e Python 3
per ricostruirlo, VICE (`x64`) per eseguirlo. Il launcher usa normalmente velocità
100%, warp e autostart-warp disattivati; `VICE_SPEED` permette un override esplicito.

Premi **Spazio** nella schermata iniziale per giocare. Sali sulle piattaforme,
collega i tre ripetitori e raggiungi l'uscita in alto a destra. Un ripetitore
attivato diventa anche un checkpoint. Hai cinque punti energia.

| Tastiera | Azione | Joystick porta 2 |
|---|---|---|
| A / D | Sinistra / destra | Sinistra / destra |
| W | Salto | Su |
| Spazio | Fuoco / avvio / riprova | Fuoco |
| S | Collega ripetitore, usa uscita; altrove scendi dalla piattaforma | Giù |
| F1 | Avvia o alterna la demo automatica | — |
| R | Ricomincia durante la partita | — |

Dopo circa dieci secondi sulla schermata iniziale parte la demo. Il percorso
automatico usa gli stessi comandi e la stessa fisica della partita manuale.

Prove riproducibili dalla radice:

```sh
python3 -B deep_signal/check.py --seconds 12 --tag repeat --require-win
python3 -B deep_signal/check.py --seconds 12 --tag scan-repeat --diagnostic
python3 -B deep_signal/exercise.py
```

Gli script aprono una propria istanza VICE sulla porta 6521, che deve essere
libera, e la chiudono al termine. `exercise.py` richiede anche ffmpeg,
verifica gli input diagnostici e rigenera la GIF. Il controllo visivo locale si ripete con
`vision.py` e Ollama, modello `gemma4:e4b-it-qat`.

La demo misurata completa la missione con **zero frame persi**, 450 aggiornamenti
di gameplay e 163 ridisegni. Il massimo del tratto profilato è **18.057 cicli**;
il budget di un frame PAL è 19.656. Il conteggio esclude la telemetria e il commit
IRQ, oltre a caricamento, schermate e primi otto aggiornamenti di riscaldamento:
non rappresenta da solo il costo completo del frame. Il contatore IRQ delle
scadenze mancate è zero nel percorso osservato. La scansione nelle otto direzioni
ha anche verificato mappa, buffer attivo e registri di scrolling in 24 campioni.

Prove e limiti: [diario](DEVLOG.md), [dettagli tecnici](TECHNICAL.md),
[partita completa](evidence/final/results.json),
[otto direzioni](evidence/directions/results.json),
[controlli](evidence/controls/results.json),
[analisi visiva](evidence/final/vision.json).

Questa è una prima versione con un solo livello, palette comune, un proiettile
alla volta e quattro nemici. Non ci sono boss o multiplexer degli sprite.
Il bilanciamento richiede partite umane; la tastiera GUI richiede una prova umana (automazione X11 inconcludente);
audio e joystick fisico non sono stati verificati con ascolto o hardware reale. La GIF campiona a 8 immagini al secondo
e non misura la fluidità. Per stabilire se sia migliore di IRON VEIN serve
ancora un confronto con scenari, carichi e criteri comuni.
