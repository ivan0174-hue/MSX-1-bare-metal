# MSX1 bare metal per Raspberry Pi 3 - Fase 1

Emulatore MSX1 che gira direttamente sull'hardware (nessun Linux), basato su [Circle](https://github.com/rsta2/circle).

## Stato

Fase 1 (provata su Raspberry Pi 3: avvio, tastiera, menu F12 e caricamento ROM funzionano): MSX1, ROM cartuccia, cassetta .CAS, tastiera USB, **menu F12** per caricare ROM / CAS / DSK da SD.
Non ancora presenti: emulazione del controller floppy (le immagini .dsk si possono scegliere dal menu e vengono caricate in memoria, ma il computer non le vede ancora), MSX2, HDD. Il floppy USB reale e' stato escluso dal progetto.

**Verificato sul PC:** compilazione di tutto il core (Z80, VDP, PSG, mapper, cassetta, tastiera, macchina, menu) e un test automatico (`tests/host_test.cpp`), che include la navigazione del menu. `kernel.cpp` e' stato controllato solo contro header Circle finti (individua errori miei, non incompatibilita' con la tua versione di Circle).
**NON verificato:** compilazione del kernel Circle (`kernel.cpp`, `main.cpp`) e avvio su Raspberry Pi 3 reale. Se qualcosa non compila, i nomi/firme delle API Circle cambiano tra versioni: incolla l'errore e correggiamo.

## Compilare

Servono: toolchain `aarch64-none-elf` (Arm GNU Toolchain) e Circle.

```
git clone --recursive https://github.com/rsta2/circle.git
cd circle
./configure -r 3 -p aarch64-none-elf-
./makeall
(cd addon/SDCard && make)
(cd addon/fatfs  && make)
(cd boot && make)            # scarica il firmware del Raspberry Pi

cd ../msx-bare/src
make CIRCLEHOME=../../circle       # produce kernel8.img
```

Se la cartella di Circle non e' `../../circle`, passa il percorso con `CIRCLEHOME=`.

## SD card (FAT32)

Copia nella radice:
- il firmware da `circle/boot` (bootcode.bin, start.elf, fixup.dat ...)
- `sdcard/config.txt` e `sdcard/msx.cfg` (da questa cartella)
- `kernel8.img`
- **msx1.rom**: BIOS MSX1 da 32 KB (tuo, oppure C-BIOS `cbios_main_msx1.rom`)
- i tuoi file `.rom` / `.cas`

`msx.cfg`:
```
bios=msx1.rom
cart=game.rom
mapper=auto          # auto | plain | konami | scc | ascii8 | ascii16
cas=tape.cas
```

## Menu F12
F12 mette in pausa l'emulazione e apre il menu sopra l'ultima immagine; F12 o Esc lo richiudono.

- Su/Giu (con ripetizione), PagSu/PagGiu, Home/Fine, Invio per scegliere.
- **Load ROM / Load tape / Load disk**: sfoglia la SD (sottocartelle incluse, `[nome]` = cartella, `..` = livello superiore). Backspace o Esc tornano indietro. Estensioni: `.rom` `.mx1`, `.cas`, `.dsk`. Nomi piu' lunghi di 47 caratteri vengono nascosti. Ogni categoria ricorda l'ultima cartella.
- Caricare una ROM esegue un reset (come inserire una cartuccia a macchina accesa). Caricare un CAS non resetta: poi scrivi `RUN"CAS:"` o `BLOAD"CAS:",R`.
- **ROM mapper**: Invio cambia tra auto / plain / konami / konami-scc / ascii8 / ascii16 (vale per le ROM caricate da quel momento). Utile quando l'autodetect sbaglia.
- **Eject** cartuccia (con reset), cassetta, disco. **Reset MSX**.
- Il testo del menu usa il font del BIOS (puntatore a 0004h). Con un BIOS atipico il menu potrebbe non essere leggibile; il log seriale avvisa.
- Dopo la chiusura i tasti ancora premuti non arrivano all'MSX finche' non vengono rilasciati.


## Audio
Il PSG (3 canali + rumore + inviluppo) esce dal Raspberry. Si sceglie in `msx.cfg`:
```
audio=hdmi     (altoparlanti del monitor/TV, predefinito)
audio=jack     (uscita jack 3,5 mm)
audio=off
```
Se con `hdmi` non senti nulla, aggiungi `hdmi_drive=2` in `config.txt` oppure prova `audio=jack`. Le scritture ai registri del PSG sono datate al ciclo, quindi anche effetti e musiche veloci suonano corretti. Non c'e' ancora il "click" della tastiera, ne' il suono SCC delle cartucce Konami.

## Cassetta: avanzamento e velocita'
- Mentre il motore della cassetta e' acceso compare in basso una barra con la percentuale: **verde** = velocita' normale, **arancione con X** = doppia velocita'.
- **F11** alterna velocita' 1x / 2x (compare per un attimo "1X" o "2X" in alto a sinistra). Il cambio vale dall'inizio del blocco successivo, perche' il BIOS misura la velocita' dalla testata di ogni blocco; a meta' blocco non viene toccata. Per averla da subito, premi F11 prima di dare il comando di caricamento.
- La barra diventa **rossa** quando il nastro e' finito: se il BIOS continua ad aspettare, il comando non era quello giusto per il tipo di file (vedi sotto). Per riavvolgere, ricarica lo stesso `.cas` dal menu F12.
- Se un caricamento a 2x da' "Device I/O error", riprova a 1x.

## Quale comando per il tipo di file `.cas`
Il BIOS stampa a schermo `Found:NOME` per un file adatto al comando e `Skip:NOME` per uno da saltare. Dopo un `Skip` continua a cercare fino alla fine del nastro.
- `RUN"CAS:"` carica solo programmi BASIC salvati in ASCII.
- `CLOAD` poi `RUN` per i programmi BASIC normali (salvati con `CSAVE`).
- `BLOAD"CAS:",R` per i programmi in codice macchina (la maggior parte dei giochi).
Se vedi solo `Skip:` e la barra arriva a 100%, riavvolgi e riprova con un altro comando.

## Risoluzione problemi (msx.cfg)
- `audio=off` spegne l'audio; `tapebar=off` toglie la barra della cassetta.
- `debug=on` fa lampeggiare il LED verde (mezzo secondo acceso, mezzo spento) finche' il ciclo principale gira: se si ferma, il programma e' bloccato. Mostra inoltre in alto a sinistra tre numeri esadecimali: contatore dei fotogrammi, indirizzo Z80 in esecuzione, e stato cassetta (cifra iniziale 1 = motore acceso, poi la percentuale in esadecimale). Se il contatore dei fotogrammi smette di salire, l'emulatore e' bloccato; se sale ma l'indirizzo non cambia, e' lo Z80 a girare in un ciclo.

## Uso
- Collega la tastiera USB **prima** di accendere: l'inserimento a caldo e' disattivato (`MSX_USB_HOTPLUG` in `src/config.h`).
- Tastiera USB: layout posizionale. Shift, Ctrl, Alt sinistro = GRAPH, Alt destro = CODE, F1-F5, F6/Pausa = STOP, F7/Fine = SELECT, F8 = CODE, Home, Ins, Canc, frecce, tastierino numerico.
- Cassetta: con BIOS normale `RUN"CAS:"` o `BLOAD"CAS:",R`; il motore viene gestito dall'emulatore.
- Schermate di errore (colore pieno, messaggio sulla seriale 115200 baud su GPIO14/15):
  rosso = SD non montabile, arancione = BIOS mancante, magenta = ROM mancante, ciano = CAS mancante.
- Se rosso e blu sono invertiti, definisci `FB_SWAP_RB` in `src/config.h`.

## Struttura
```
src/z80.*        CPU Z80 (tutte le istruzioni documentate + IX/IY, CB/ED)
src/vdp.*        TMS9918A: text, graphic 1/2, multicolor, sprite, collisioni
src/psg.*        AY-3-8910 (render campioni, non ancora in uscita)
src/cart.*       mapper: plain, Konami, Konami SCC (senza suono), ASCII8, ASCII16, autodetect
src/tape.*       .CAS -> segnale FSK 1200 baud con motore
src/keyboard.*   USB HID -> matrice tastiera MSX
src/msx.*        slot (BIOS / cartuccia / RAM 64K), PPI, porte I/O, frame loop
src/menu.*       menu F12 (logica + disegno, testabile sul PC)
src/kernel.*     Circle: framebuffer, USB, SD/FatFs, temporizzazione 60 Hz
tests/host_test.cpp  test automatico sul PC
```

## Prossime fasi
2. Controller WD2793 + ROM disco (da fornire, es. in slot 2), lettura/scrittura delle immagini .dsk scelte dal menu.
3. MSX2 (V9938, mapper RAM, RTC), HDD con Nextor, audio PSG.

## Diagnostica
Se in futuro qualcosa non parte, in `src/config.h` togli il commento a `#define MSX_DIAG`: all'avvio il LED verde lampeggia N volte per fase, compaiono barre colorate e, durante l'emulazione, riga video, indirizzo Z80 e quadratini di avanzamento.


## Note sui file .cas (v1.15)

- Le versioni "da edicola" o convertite spesso hanno i blocchi **non allineati a 8 byte**: dalla 1.15 le intestazioni vengono cercate a ogni byte, quindi `RUN"CAS:",R` con un caricatore BASIC che poi fa `BLOAD"CAS:",R` funziona.
- F11 (velocita' doppia) con caricatori in codice macchina puo' dare problemi con alcuni BIOS: se un gioco si blocca con la barra al 100% e il motore acceso (terzo numero del debug = 1xx), riprova a velocita' normale.

## ROM "di sistema" (v1.16)

Alcune ROM (es. certe conversioni di giochi) non sono cartucce normali: sono fatte per partire da 0000h, con intestazione `AB` e indirizzo INIT sotto 4000h. Con il BIOS normale restano ferme sulla schermata di avvio. Dalla 1.16 vengono riconosciute da sole (16 o 32 KB) e girano **al posto del BIOS**, con la RAM nelle pagine 2-3. Il menu F12 mostra "System ROM: runs without BIOS". "Eject ROM" ripristina il BIOS vero e fa il reset.
