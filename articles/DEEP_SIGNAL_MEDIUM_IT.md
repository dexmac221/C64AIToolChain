# Un gioco per C64 scritto da Codex: le difficoltà dietro DEEP SIGNAL

*Una demo che gioca da sola, un processore da circa un megahertz e diversi errori da correggere prima di arrivare in fondo.*

![DEEP SIGNAL: la demo automatica registrata in VICE](../deep_signal/deep_signal.gif)

*Registrazione reale dell'emulatore, campionata a 8 immagini al secondo. La GIF illustra il gioco: non è una misura del suo frame rate.*

La richiesta era semplice: costruire da zero un nuovo gioco per Commodore 64, nello stesso terreno di IRON VEIN, e tenere un diario abbastanza onesto da poterci scrivere un articolo. Il risultato è **DEEP SIGNAL**, un primo livello platform shooter sviluppato da **OpenAI Codex**, con la direzione e il feedback dell'utente, dentro C64AIToolChain.

L'astronauta deve risalire una stazione abbandonata, affrontare quattro droni, collegare tre ripetitori e raggiungere l'uscita. Codice C e Assembly 6502, mappa, caratteri e sprite sono originali. Gli strumenti di partenza sono cc65, VICE e la toolchain del repository. La demo automatica usa gli stessi input e la stessa fisica della partita manuale.

La parte interessante, però, è quanto ha richiesto farla funzionare.

## Compilare non significava avere abbastanza tempo

La prima versione consumava fino a **37.745 cicli nel tratto profilato**. Un frame PAL ne mette a disposizione 19.656. Il renderer disegnava la mappa correttamente, ma la logica non riusciva a preparare sempre l'aggiornamento successivo in tempo.

Spostare fisica, nemici e proiezione degli sprite in Assembly ha portato il picco osservato a 24.132 cicli. Ancora troppo. Sono seguiti il controllo della demo, la camera e infine le interazioni con i ripetitori: il risultato finale è stato **18.057 cicli nel tratto misurato**.

Questi valori raccontano iterazioni con percorsi e durate diversi, non un benchmark statistico controllato. Inoltre il cronometro esclude telemetria e commit dell'interrupt. Per questo è stato affiancato da un contatore delle scadenze mancate, che nella missione finale è rimasto a zero.

Anche scrivere Assembly ha prodotto errori: salti condizionali oltre la distanza consentita e troppi temporanei nella piccola regione zero-page riservata. Sono problemi che il compilatore e il linker hanno costretto a risolvere concretamente.

## Risparmiare cicli costa memoria e libertà grafica

La camera combina scrolling fine hardware e due buffer dello schermo. Quando supera il confine di un carattere, il motore ridisegna il viewport. Due routine srotolate occupano circa 12 KB: una spesa di memoria deliberata per ridurre il lavoro della CPU.

La palette del fondale è condivisa e la Color RAM resta costante. Due sprite fissi mostrano energia e ripetitori, evitando un ulteriore split raster. Restano sei slot: astronauta, quattro droni e un proiettile.

Sono scelte visibili nel gioco. La prima grafica era troppo spoglia; travi e pannelli hanno dato più struttura alla stazione. Non abbiamo però aggiunto un boss o un multiplexer solo per allungare l'elenco delle funzionalità. Prima serviva una base misurabile.

## Il bot cadeva dalla piattaforma giusta

Uno degli errori più istruttivi riguardava la demo. Il bot arrivava vicino a un waypoint e poi scendeva dalla piattaforma che avrebbe dovuto attraversare.

Un piccolo registro circolare di posizioni, velocità e comandi ha mostrato il motivo: il bot premeva GIÙ quando mancavano 14 pixel, mentre il waypoint veniva considerato raggiunto solo sotto i 12. Il comando di discesa arrivava prima del cambio di obiettivo.

La correzione ha richiesto di aspettare l'atterraggio e distinguere un normale waypoint da un ripetitore o dall'uscita. Era un errore nel codice appena scritto da Codex. Gli screenshot, da soli, non spiegavano quella sequenza.

## Anche gli strumenti di verifica possono sbagliare

Durante il lavoro precedente sulla toolchain, Codex aveva introdotto un problema nel collegamento al monitor VICE: chiudere una connessione lasciando l'emulatore in pausa poteva innescare un ciclo di errori di rete e reset. L'utente aveva segnalato un comportamento anomalo e un'accelerazione percepita. La correzione ha mantenuto caricamento, verifica e avvio nella stessa connessione. Non era stata misurata la velocità durante il guasto: attribuirlo con certezza al warp sarebbe stato scorretto.

Anche la verifica visiva ha mostrato i propri limiti. Il modello locale ha riconosciuto schermate leggibili e non ha segnalato corruzioni evidenti, ma in una risposta ha descritto erroneamente il platform come un gioco visto dall'alto o isometrico. L'automazione della tastiera con X11 è risultata inconcludente: alcune pressioni si perdevano o arrivavano in ritardo. Le regole sono state quindi provate tramite input diagnostici, senza dichiarare certificata la tastiera fisica.

## Dove siamo arrivati

La demo completa la missione con tre ripetitori attivi, **450 aggiornamenti, 163 ridisegni e zero scadenze mancate nel percorso osservato**. Una prova separata della camera ha coperto otto direzioni: nei 24 campioni, i 1.000 caratteri del buffer visibile corrispondevano alla mappa e i registri dello scrolling fine erano coerenti.

È un primo livello funzionante. Restano bilanciamento con giocatori umani, audio da ascoltare, joystick e hardware reale da verificare. Non è ancora dimostrato che sia migliore di IRON VEIN: i due giochi hanno carichi e funzionalità diversi.

Il valore di questo esperimento sta nel poter seguire il passaggio dal codice che compilava ma non rispettava il budget a una demo che raggiunge l'uscita. Il diario conserva anche i fallimenti, perché senza quelli i numeri finali racconterebbero soltanto metà dello sviluppo.

---

**Materiali:** [gioco, sorgenti e comandi](../deep_signal/README.md), [diario completo](../deep_signal/DEVLOG.md), [misure della missione](../deep_signal/evidence/final/results.json), [scansione della camera](../deep_signal/evidence/directions/results.json), [incidente del monitor](codex-evidence/incident-before-fix.json).

*Testo preparato da OpenAI Codex a partire dal diario e dalle prove della sessione, per la revisione e pubblicazione dell'utente su Medium / Towards AI. Nessuna pubblicazione su Medium è stata effettuata.*
