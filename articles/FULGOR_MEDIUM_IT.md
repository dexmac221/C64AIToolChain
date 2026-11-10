# FULGOR: un Turrican per Commodore 64 scritto da un modello, e il difetto che solo un occhio umano ha visto

*Scrolling in otto direzioni a 50 fotogrammi al secondo, un cielo dipinto da un interrupt preciso al ciclo, e una lezione sugli strumenti di misura: guardano solo dove li metti.*

![FULGOR: una partita di prova registrata in VICE](../fulgor/fulgor_demo.gif)

*Registrazione reale dell'emulatore VICE. La GIF gira a 7 immagini al secondo, circa una ogni sei fotogrammi del gioco: illustra il gioco, non ne misura la fluidità. I cambi di scena sono teletrasporti del copione di prova; nel finale l'eroe muore nella lava.*

La richiesta è arrivata in una riga: analizzare il progetto e poi fare "un gioco per Commodore 64 di alto livello, tipo Turrican 2, da rinominare: la demo del primo livello, non il gioco intero". Il risultato è **FULGOR**, sviluppato da **Claude Fable 5** in Claude Code dentro C64AIToolChain, in una sola giornata di lavoro. Dall'altra parte c'era un umano cresciuto con il VIC-20 e il C64, che guardava lo schermo e interveniva quando qualcosa non gli tornava.

FULGOR è un livello completo da giocare: un eroe di due sprite che corre, salta e spara con tre livelli di potenza dell'arma, tre tipi di nemici (un pattugliatore, una vespa e una torretta che spara), gemme e potenziamenti, energia, vite, checkpoint e lava. C'è un HUD, una schermata del titolo, un brano originale per il SID a tre voci e sette effetti sonori. Il mondo è largo 4 096 pixel e alto 1 280, e si scorre in tutte le otto direzioni. Il motore è tutto in Assembly 6502; il C resta per l'avvio, il testo dell'HUD e i passaggi tra una vita e l'altra. Il programma pesa 46 864 byte.

Di Turrican c'è solo la classe del gioco. Codice, grafica, livello e musica sono originali.

## Che cosa vuol dire "classe Turrican II"

In questo repository c'era già IRON VEIN, un'altra demo a scorrimento in otto direzioni, scritta tre mesi prima da Claude Fable 5.1. Era la base naturale da cui partire. Ma messa accanto a Turrican mostrava i suoi limiti, e il diario di sviluppo di IRON VEIN ne spiegava le cause.

| | IRON VEIN | FULGOR |
|---|---|---|
| velocità | 1 pixel per fotogramma | corsa a 2, cadute fino a 3 |
| sfondo | nero | un cielo a bande di colore |
| eroe | uno sprite, 21 pixel | due sprite sovrapposti, circa 40 pixel |
| collisioni | blocchi da 32 pixel | per carattere, con piattaforme sottili |
| bilancio | 6 nemici, circa 5% di fotogrammi in ritardo | spazio per armi e musica, zero ritardi |

Le cause stavano nel codice: camera ed eroe scritti in C (2 000 e 1 500 cicli a fotogramma), la copia dello schermo con un indirizzamento lento, gli interrupt che passavano per il KERNAL. Quindi FULGOR ha una cartella nuova e un motore nuovo. Riprende le idee di IRON VEIN che avevano retto, ma l'intero fotogramma è in Assembly.

## La grafica nasce da un programma

Un dettaglio tecnico del C64 decide l'aspetto del gioco. Lo schermo a caratteri ha una memoria dei colori che non si può avere in due copie: si può preparare in anticipo la prossima schermata, ma non i suoi colori. Così il quarto colore di ogni carattere appartiene alla riga del mondo, non al singolo blocco. Il livello è quindi diviso in fasce: verde in superficie, giallo nelle caverne, ciano nella galleria dei cristalli. È un vincolo del motore, e la direzione artistica lo segue.

Il terreno non è disegnato blocco per blocco. Un programma Python genera una trama di roccia di 32×32 pixel che si ripete senza giunture, fatta di celle illuminate dall'alto a sinistra. Da quella ritaglia le sedici varianti dei bordi, con regole per il contorno, l'erba e l'ombra. Anche il livello è un piccolo programma: un profilo del terreno, caverne scavate come rettangoli, sporgenze e decorazioni messe secondo regole. Un visualizzatore ridisegna la mappa e il foglio degli sprite con i colori del gioco. Così la grafica è stata giudicata prima che esistesse una sola riga di 6502.

![L'intero livello di FULGOR](../fulgor/fulgor_level.png)

*Il livello intero, 128×40 blocchi da 32 pixel, come lo disegna `assetgen.py`: altipiani e rovine in superficie, la galleria dei cristalli, la fornace con la lava in basso a sinistra.*

Due correzioni sono venute dal guardare. Il primo abbinamento di colori, marrone e arancio, sembrava fango; otto combinazioni affiancate hanno scelto marrone e rosso chiaro, che si legge come roccia di canyon. Le prime gambe dell'eroe erano generate da uno scheletro di angoli e venivano fuori bastoncini spezzati. Ora sono quattro disegni fatti a mano.

## Il motore era giusto alla prima prova, ed era lento

Alla prima accensione lo scorrimento, l'eroe e le collisioni erano corretti. Poi è arrivato il contatore. Un fotogramma PAL dura 312 linee di quadro, e il lavoro di un fotogramma finiva alla linea 219 con il solo eroe in scena che correva, saltava e sparava. Era troppo, prima ancora di aggiungere un nemico.

Un profilatore a cinque punti di controllo ha dato i nomi ai costi:

- **il multiplexer degli sprite costava 1 950 cicli per mostrare il solo eroe.** Il C64 ha otto sprite hardware e il multiplexer li riusa più volte lungo lo schermo. Ogni fotogramma scorreva e ordinava tutti i sedici oggetti possibili. Ora tiene una lista degli oggetti vivi già ordinata per altezza, che cambia di un passo quando serve, e l'interrupt entra in una scala già srotolata al gradino giusto;
- **la copia dello schermo costava 4 200 cicli per ciascuna delle due metà.** Scorrere di un carattere vuol dire spostare tutto lo schermo nel buffer nascosto. Ora la copia è divisa in bande, distribuite sui fotogrammi che restano prima del passo, mai più di due per fotogramma. Se il buffer non è pronto, la camera aspetta al confine per un fotogramma: è meglio di un fotogramma perso;
- **i controlli laterali delle collisioni costavano 115 cicli l'uno.** Ora la colonna e il suo bit si calcolano una volta sola.

| | fermo | in corsa | corsa, salto e fuoco |
|---|---|---|---|
| prima versione | 64 linee | ~120, peggiore 149 | peggiore 219 |
| dopo | 42 | ~94, peggiore 120 | peggiore ~195 |

C'era anche un errore di progettazione del livello, trovato con una misura. Il salto dell'eroe sale di 49 pixel, e la prima bozza aveva un fosso profondo tre blocchi, cioè 96 pixel. Da allora le regole stanno scritte nel generatore del livello: ogni gradino è di un blocco, ogni fosso largo un blocco, i pozzi larghi tre con le sporgenze alternate.

## Il cielo: un interrupt che non può arrivare in ritardo

Il cielo di FULGOR è un gradiente di bande di colore, e le bande scorrono con il mondo. Il C64 ha un solo registro per il colore di fondo. Per avere le bande bisogna cambiarlo nel punto esatto in cui finisce una riga di caratteri, che a ogni fotogramma si trova su una linea diversa perché lo schermo scorre. Un cambio sbagliato di pochi cicli si vede come un gradino nel mezzo di una riga.

Il normale interrupt di quadro non basta, perché il multiplexer degli sprite usa gli stessi interrupt e può ritardarlo. FULGOR usa l'interrupt non mascherabile (NMI) del secondo chip CIA. Un timer è lungo esattamente un fotogramma PAL (19 656 cicli), sincronizzato una volta all'avvio con il pennello elettronico. Da quel momento scatta sempre nello stesso punto dello schermo. Il suo gestore legge il timer per sapere di quanti cicli è in ritardo, salta dentro una sequenza di istruzioni nel punto che compensa esattamente quel ritardo, e fa partire un secondo timer che scatta ogni 504 cicli, cioè ogni otto linee, una riga di caratteri.

La macchina ha corretto la carta tre volte, una misura per correzione:

- un salto condizionale che doveva essere incondizionato disarmava la catena ogni volta che il primo ritardo era inferiore a 256 cicli;
- con lo scorrimento verticale a 5, la linea di partenza diventa una "bad line", una linea in cui il chip video ferma il processore per 40 cicli. L'interrupt arrivava in ritardo. Quel caso ha ora un gestore suo;
- con lo scorrimento a 7, la prima banda cade 46 cicli dopo l'avvio e sulla carta poteva sovrapporsi all'uscita del gestore precedente. La soluzione ha richiesto un ordine preciso delle conferme e gestori tutti nella stessa pagina di memoria.

Lo stesso NMI ha risolto un vecchio difetto di IRON VEIN. Dove l'HUD incontra il campo di gioco c'è una striscia frastagliata, perché la prima riga compare e scompare con lo scorrimento. FULGOR accende lì una modalità video non valida, che il chip mostra nera, e l'NMI la spegne su una linea fissa. Il bordo superiore del campo ora è pulito in tutte le otto fasi di scorrimento. Misurato al ciclo: le scritture del colore cadono tra il ciclo 52 e il 62 dell'ultima linea della riga, cioè nel bordo destro o nel ritorno del pennello, dove non si vedono.

## Il gioco sopra il motore

Poi sono arrivati i nemici, e il bilancio è saltato di nuovo: linea 253 su 312. Il profilatore ha detto che la tabella degli sprite costava 340 cicli per sprite, non i 230 stimati, perché faceva tre passate su variabili in memoria normale. Ora è una passata sola in pagina zero, circa 225 cicli. La copia dello schermo è passata da tre a quattro bande e le cadute sono state limitate a 3 pixel per fotogramma, così il fotogramma più pesante copia due bande da dieci colonne. Con la musica, il fotogramma peggiore in gioco arriva alla linea 213–233 su 312, con zero fotogrammi in ritardo.

Un dettaglio di gioco è venuto dalla geometria. I primi nemici camminavano a terra, alti 21 pixel. L'arma dell'eroe è a 24 pixel da terra, quindi i colpi passavano sopra le loro teste. Ora tutti i nemici stanno a mezz'aria, all'altezza dell'arma.

## Il difetto sul lato destro

A questo punto l'umano, guardando il gioco, ha scritto: *"ho visto un problema di scrolling sul lato destro"*.

Gli strumenti del modello non vedevano niente. Le schermate prese a caso erano pulite. Un verificatore che fermava l'emulatore e confrontava lo schermo con la mappa era pulito anche lui. Il motivo, capito dopo, è che il monitor di VICE ferma la macchina alla fine del fotogramma, prima che il codice di scorrimento abbia lavorato: si fermava sempre nell'istante sbagliato per vedere il difetto.

Il verificatore è stato spostato. Ora `check.py` mette un punto di arresto all'ingresso della routine che consegna il fotogramma, quando il ciclo principale ha finito di scrivere. Lì legge quale buffer sta mostrando il chip video e confronta tutte le 840 celle del campo con quello che dice il livello. Risultato: **14 campioni su 60 sbagliati, sempre nelle colonne da 30 a 39**, e solo nei fotogrammi in cui la camera faceva un passo.

La causa era nel nuovo pianificatore del motore. Lo scambio tra il buffer visibile e quello preparato si decide un fotogramma prima di vederlo. In quello stesso fotogramma il motore cominciava già a preparare il passo successivo, scrivendo nel buffer che era ancora sullo schermo. La prima banda copiata è quella delle dieci colonne di destra. Per un istante, a ogni passo, il quarto destro dello schermo mostrava il mondo spostato di un carattere. A 50 Hz l'occhio lo vede come uno sfarfallio; una schermata ferma non lo vede mai. IRON VEIN non aveva questo problema perché preparava prima di decidere: l'errore è nato con il pianificatore nuovo.

La correzione: **tre buffer dello schermo a rotazione** invece di due. La preparazione legge il buffer appena diventato visibile e scrive in quello che non è né il nuovo né quello ancora sullo schermo. Costa 24 routine di copia invece di 16, un chilobyte di codice in più e un chilobyte di memoria video, nessun ciclo. Dopo la correzione: 0 campioni sbagliati su 70, su 49 posizioni della camera, correndo a destra e saltando. A fine giornata le stesse prove verso sinistra e in caduta: 0 su 110, su altre 71 posizioni.

## Lo strumento che rallentava il gioco

Subito dopo, l'umano ha scritto di nuovo: *"quando il personaggio principale si muove ora va a scatti, tipo un fotogramma al secondo"*.

Il gioco non c'entrava. Il verificatore stava girando mentre l'umano guardava. Ogni campione è un punto di arresto e una lettura di 3 KB di memoria, e un'esecuzione interrotta aveva lasciato il punto di arresto attivo. Una misura senza nessuno davanti ha confermato che il gioco andava a 50 fotogrammi.

La regola c'era già, nelle note del progetto, imparata con un gioco precedente: non toccare il monitor dell'emulatore mentre qualcuno guarda lo schermo. Il modello l'ha imparata di nuovo. Per le prove di chiusura ha usato una seconda copia di VICE su un'altra porta, lasciando intatta quella in cui l'umano stava giocando.

## Che cosa ha visto il modello visivo

Le regole del progetto chiedono di far guardare il gioco anche a un modello visivo (un VLM). Tre fotogrammi della GIF sono stati dati a gemma4:31b, con la richiesta di descrivere la scena e cercare difetti. Ha letto correttamente l'HUD in tutti e tre, punteggio compreso; ha descritto eroe, nemici, colpi, cielo e terreno; non ha trovato difetti. In un fotogramma ha descritto la lava come "piastrelle rosa e gialle".

È un buon risultato, ma va letto per quello che è. Il difetto del lato destro esisteva per una frazione di fotogramma, e solo in certi fotogrammi. Nessuna immagine ferma lo avrebbe mostrato, a un modello o a una persona. Lo ha visto un occhio che guardava il gioco muoversi.

## Dove siamo arrivati

FULGOR si gioca dal titolo alla fine del livello. Le misure sulla versione finale: 1 046 fotogrammi (una ventina di secondi) di corsa, salto e fuoco con zero fotogrammi in ritardo e il fotogramma peggiore alla linea 213 su 312; fino a circa 233 nelle scene più piene con la musica. Lo schermo coincide con la mappa in 180 campioni presi nell'istante in cui il difetto si sarebbe visto, in tutte le direzioni provate.

Restano aperte diverse cose:

- **la musica**: il modello non può ascoltare. Ha controllato che le note di ogni battuta fossero quelle scritte guardando lo spettro di una registrazione WAV dell'emulatore. Il giudizio sull'ascolto è dell'umano;
- **il finale**: non c'è un boss, il livello finisce raggiungendo l'uscita. Mancano anche il raggio rotante e la trasformazione in ruota che fanno Turrican;
- **le cadute lunghe**: la camera a volte aspetta al confine e resta indietro di qualche pixel (39 attese in 11 secondi di una prova con una caduta lunga);
- **il logo del titolo** si legge poco sul cielo chiaro.

## Che cosa insegna

L'articolo su IRON VEIN si chiudeva dicendo che ogni problema serio era stato trovato da un numero, non dallo sguardo. FULGOR aggiunge una precisazione. Un numero vede solo l'istante in cui lo prendi. Il verificatore non era sbagliato: guardava nel momento sbagliato, e per questo diceva "tutto a posto". Lo sguardo dell'umano vede cinquanta immagini al secondo e le somma, e ha trovato il sintomo che gli strumenti non vedevano. Poi lo strumento, spostato nel punto giusto, ha trovato la causa. Nessuno dei due da solo sarebbe arrivato in fondo.

Vale anche il contrario: misurare costa. Lo stesso verificatore, acceso mentre l'umano guardava, ha trasformato un gioco fluido in una sequenza di scatti, e il nuovo sintomo era stato causato dallo strumento stesso.

---

**Materiali:** [gioco, sorgenti e comandi](../fulgor/README.md), [diario di sviluppo completo](../fulgor/DEVLOG.md), [il verificatore schermo/mappa](../fulgor/check.py), [risposte del modello visivo](../fulgor/evidence/vlm_review.md).

*Gioco sviluppato da Claude Fable 5 in Claude Code. Verifiche di chiusura, nuova registrazione e testo preparati da Claude Opus 5.5 a partire dal diario e dalle prove della sessione, per la revisione e la pubblicazione dell'utente. Nessuna pubblicazione è stata effettuata.*
