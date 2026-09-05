# Architettura e strumenti

Il codice di gioco, il renderer, i caratteri, gli sprite e la mappa sono originali.
Si riusano cc65, VICE e il client monitor della toolchain; nessun motore di IRON
VEIN è copiato. `assetgen.py` genera i bitmap con la sola libreria standard.

`game.c` gestisce modalità, inizializzazione, checkpoint, musica, effetti e
telemetria. `fast.s` implementa fisica, collisioni, nemici, proiettile, demo,
interazioni, camera e proiezione degli sprite. `engine.s` gestisce input, IRQ
e i due renderer srotolati. La mappa è 128×64 caratteri, cioè 1024×512 pixel.

La camera usa scrolling fine VIC-II e ridisegna 40×25 caratteri soltanto al
cambio della coordinata grossolana, oppure quando cambia un ripetitore. Due
renderer, ciascuno con destinazioni assolute, spendono circa 12 KB per risparmiare
cicli. Color RAM resta costante. L'IRQ raster alla linea 250 applica solo un
buffer completamente preparato; il main aspetta il consumo del flag `ready`.
Due sprite HUD fissi evitano un secondo split raster ma riducono il budget
oggetti: eroe, quattro droni, proiettile e due pannelli occupano tutti gli slot.

| Intervallo | Uso |
|---|---|
| $0801–$3FFF | Header BASIC e codice, ingresso SYS 2061 |
| $4000–$47FF | Due buffer schermo |
| $4800–$4FFF | Charset |
| $5000–$5FFF | Regione sprite riservata |
| $6000–$67FF | Costanti, dati, BSS |
| $6800–$6FFF | Trace ring diagnostico riservato |
| $7000–$7FFF | Spazio libero e stack cc65 dall'alto |
| $8000–$9FFF | Mappa modificabile |
| $A000–$CF57 | Renderer |

ROM disabilitate, I/O visibile (`$01=$35`), vettori IRQ/NMI in RAM. Nessuna
allocazione dinamica. Configurazione esplicita in `deep_signal.cfg`.

## Interfaccia diagnostica

Non serve per giocare. Gli input diagnostici passano dalle stesse regole di gioco:
`$033C` impulso consumato, `$033E` input mantenuto. Bit: su 1, giù 2, sinistra 4,
destra 8, fuoco 16, demo 32, restart 64. `$033F=1` abilita il percorso camera
in otto direzioni, indipendente dal giocatore. `$033D=1` abilita il trace ring.

La telemetria a `$0340` ha firma `DS`, versione a `$0342`, stato a `$0343`
(0 titolo, 1 partita, 2 sconfitta, 3 vittoria). Le word sono little endian:

| Indirizzo | Dato |
|---|---|
| $0344 / $0346 | Tick IRQ / aggiornamenti partita |
| $0348 / $034A / $034C | Scadenze mancate / massimo cicli / redraw |
| $034E / $0350 | Giocatore X / Y |
| $0352 / $0354 | Camera preparata X / Y |
| $0356 / $0357 / $0358 | Energia / bitmask ripetitori / demo (byte) |
| $0359 / $035A / $035B | Sprite enable / camera coarse X / Y (byte) |
| $035C | Ultimo costo profilato |
| $035E / $035F | Waypoint / grounded (byte) |
| $0360 / $0362 | Camera applicata dall'IRQ X / Y |
| $0370 | Ultimo costo renderer |

Il ring ha 256 record da 8 byte: X word, Y word, input, grounded, waypoint,
velocità verticale signed. L'indice è il byte basso degli aggiornamenti.
Il tracing introduce lavoro aggiuntivo: disabilitato nelle misure finali.

`check.py` legge il buffer selezionato da $D018 e confronta tutti i 1000 caratteri
con la mappa viva, usando la camera applicata, non quella ancora in preparazione.
Verifica anche i registri fine X/Y. Le catture fermano temporaneamente VICE:
i tempi a parete degli screenshot non sono una misura di FPS. La scansione
copre le otto direzioni ma non certifica ogni possibile combinazione di fase,
collisioni e input. Il profiling CIA misura un tratto del main; le scadenze
mancate sono rilevate dall'IRQ. Nessuna prova su Commodore 64 fisico svolta.
