# DEEP SIGNAL — diario del gioco

## 2026-09-05 — Prima implementazione

Richiesta: nuovo gioco da zero, confrontabile con IRON VEIN, con diario per
Towards AI. Motore, art generator, livelli e logica qui sono originali;
si riusano cc65, VICE e gli strumenti della toolchain consolidata.

Ipotesi da misurare: spendere circa 12 KB per due renderer Assembly srotolati
permette un redraw completo solo ai confini dei caratteri, senza preparazione
su più frame. Mappa 128x64 caratteri, viewport 40x25, scrolling hardware X/Y.
Color RAM costante e palette comune; HUD in due sprite per evitare un raster
split tra due diversi YSCROLL. È un compromesso grafico e di budget sprite,
non una superiorità già dimostrata. Restano sei sprite per giocatore, quattro
droni e un proiettile. Renderer a $A000, stack sotto $8000, nessuna sovrapposizione.

Gioco: salire nella stazione, collegare tre ripetitori con GIÙ, raggiungere
l'uscita in alto a destra. Piattaforme attraversabili dal basso, salto, fuoco,
energia, droni, checkpoint, demo e condizioni di vittoria/sconfitta.

Primo stato: file in costruzione, nessun risultato di performance ancora acquisito.

## 2026-09-05 — Implementazione, errori e misure

Il primo prototipo compilabile usava C per buona parte della logica e Assembly
per il redraw. Durante la costruzione sono stati corretti branch 6502 fuori
portata e un eccesso di temporanei zero-page: i renderer usano il temporaneo
cc65 `ptr1`, non nuove allocazioni oltre la regione riservata.

La prima partita non rispettava il budget: picco di 37.745 cicli, avanzamento
vicino a un aggiornamento ogni due frame nei tratti onerosi. La demo inoltre
non completava il percorso. Il fatto che lo schermo corrispondesse alla mappa
non bastava a dichiarare il motore riuscito.

| Passaggio | Massimo osservato | Esito |
|---|---:|---|
| Prima versione, molta logica C | 37.745 | Fuori budget, demo incompleta |
| Fisica, attori e sprite in Assembly | 24.132 | Ancora 118 scadenze mancate nel test |
| Demo in Assembly | 21.572 | 10 scadenze mancate, percorso non affidabile |
| Controllo atterraggio | 21.253 | 5 scadenze mancate, bug di discesa ancora presente |
| Camera in Assembly e percorso corretto | 20.565 | Missione completata, 3 scadenze mancate |
| Interazioni in Assembly | 18.057 | Missione completata, zero scadenze mancate |

Questi sono passaggi di sviluppo con durate e percorsi diversi, non benchmark
comparabili statisticamente. I risultati intermedi sono conservati nelle
sottocartelle di `evidence/` (`first`, `asm`, `fast-demo`, `landing`, `camera`,
`budget`). I picchi misurano il tratto profilato del main, non l'intero frame.

Il bug della demo è stato localizzato con un ring di posizioni e input. Vicino
al waypoint X=360, Y=316, la demo premeva GIÙ a distanza 14 pixel, mentre la
soglia per considerare raggiunto il waypoint era 12. Scendeva quindi attraverso
la piattaforma giusta prima di avanzare al waypoint successivo. Correzione:
aspettare l'atterraggio e usare GIÙ alla stessa quota solo presso ripetitori o
uscita. Prova originale: `evidence/trace/trace.json`. È un errore introdotto nel
nuovo codice, non una limitazione di VICE o del modello visivo.

Sono stati aggiunti pannelli e travi alla grafica iniziale, troppo spoglia.
Il VLM locale descrive titolo e vittoria come leggibili, l'astronauta allineato
alla piattaforma e nessuna corruzione evidente nei campioni finali. Giudica
moderata la leggibilità della scena di gioco. Gli screenshot non provano
assenza di flicker, fluidità, qualità dell'audio o divertimento.

### Risultato riproducibile della prima versione

- PRG di 51.033 byte, compresi due byte di indirizzo di caricamento.
- VICE PAL, speed 100, warp e autostart-warp disabilitati.
- Demo completa: 450 aggiornamenti, 163 redraw, tre ripetitori attivi,
  energia finale 4, stato vittoria entro il campione a circa 10 secondi.
- Zero scadenze mancate nel percorso osservato; massimo profilato 18.057 cicli.
  Telemetria e commit IRQ sono esterni al tratto cronometrato; titolo, transizioni
  e primi otto aggiornamenti sono esclusi. Budget PAL totale 19.656 cicli.
- Scansione camera: tutte le otto direzioni coperte, 24 campioni, zero differenze
  sui 1000 caratteri del buffer visibile e zero differenze nei registri fine X/Y.
  Massimo profilato 16.508 cicli, zero scadenze mancate. In questa prova il
  giocatore è fermo e la camera segue un percorso diagnostico indipendente.
- Zero errori di rete VICE in queste prove. Ogni script termina solo il proprio
  processo e ha un watchdog contro blocchi o crescita incontrollata del log.

Prove finali: [partita](evidence/final/results.json),
[scansione](evidence/directions/results.json), [VLM](evidence/final/vision.json).

### Materiale per l'articolo e limiti

L'esperimento mostra un ciclo concreto: implementazione, misura negativa,
strumentazione, diagnosi, ottimizzazione e verifica. Non dimostra che Codex
produca automaticamente un gioco migliore di IRON VEIN. Qui si è scelto un
carico inferiore e diverso: quattro droni, un proiettile, due sprite HUD,
nessun boss o multiplexer, palette condivisa. Il confronto futuro deve fissare
carico, percorso, criteri visivi e condizioni di misura comuni.

La demo è breve e deterministica. Restano da valutare bilanciamento con giocatori
umani, ascolto SID, joystick fisico, hardware reale e casi di input non percorsi
dalle prove. La GIF è una presentazione campionata a 8 Hz e non una prova di FPS.

### Verifica dei controlli: limite dell'automazione GUI

Tentata la prova dei tasti con xdotool sulla finestra VICE posseduta dal test.
L'invio diretto non avviava il gioco; attivando la finestra alcune pressioni
venivano recepite, altre risultavano perse o ritardate, anche dopo pressioni
separate. Non è stato isolato con certezza il punto di perdita degli eventi.
Non si dichiara superata la prova della tastiera né si attribuisce il fenomeno
con certezza al gioco. La mappatura del codice coincide con quella simbolica
VICE letta; serve comunque una verifica manuale.

Il test riproducibile `exercise.py` usa quindi il byte input mantenuto $033E
per verificare le regole di movimento, salto, fuoco e restart. La sconfitta
è provocata impostando energia a uno e posizione oltre il fondo: l'aggiornamento
successivo deve applicare la normale logica di danno. Non è una prova di
raggiungibilità naturale di quella posizione. Gli esiti sono registrati in
[evidence/controls/results.json](evidence/controls/results.json).

Esito del test diagnostico completato: X 64→120 con destra, 120→84 con
sinistra; salto dalla quota Y=460 alla piattaforma Y=412; proiettile attivo;
stato sconfitta con energia zero; riprova e R ripristinano stato partita,
energia cinque e ripetitori a zero. Zero errori di rete. La registrazione
conclude di nuovo la missione con 450 aggiornamenti e zero scadenze mancate.
GIF prodotta: [deep_signal.gif](deep_signal.gif), 104 catture a 8 Hz nominali.

L'ultima risposta VLM (`evidence/controls/vision.json`) non segnala corruzioni
ma descrive erroneamente il gioco come top-down/isometrico e riassume solo due
delle tre immagini fornite. È quindi una verifica visiva parziale e fallibile,
non una certificazione delle scene o delle meccaniche. Il confronto automatico
dei buffer e la telemetria restano prove distinte.

## Confezionamento per il repository e Medium

Verificata la GIF animata della missione; aggiunta attribuzione esplicita a
OpenAI Codex e istruzioni per la demo nel README. Preparato
[articolo in italiano](../articles/DEEP_SIGNAL_MEDIUM_IT.md), centrato su budget
CPU, compromessi, errori del bot e limiti delle verifiche. Il testo è un file
revisionabile nel repository, non un articolo già pubblicato su Medium.

Verifica prima del push: copia isolata dei file staged, ricompilazione identica
al PRG consegnato e missione nuovamente completata con zero scadenze mancate.
GIF verificata con ffprobe: 104 frame, 384×272, 13,01 secondi.
Prove in [evidence/publish/results.json](evidence/publish/results.json) e
[artifact-check.json](evidence/publish/artifact-check.json).
