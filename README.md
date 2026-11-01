# Formula SAE Digital Dashboard (ESP32 + ILI9341 + Wokwi)

Dashboard digitale da volante per monoposto **Formula SAE / Formula Student**, sviluppata e simulata per microcontrollore **ESP32**, display TFT a colori **ILI9341 (320x240)** e barra di **24 LED NeoPixel (WS2812B)** per le Shift Lights.

Il layout rispetta fedelmente la disposizione e gli elementi della bozza fornita (`bozza.jpeg`):

```text
 🟢🟢🟢🟢🟢🟢🟢🟢 🔴🔴🔴🔴🔴🔴🔴🔴 🔵🔵🔵🔵🔵🔵🔵🔵  (24 Shift LEDs)
+-------------------------------------------------------------+
| 13 l                                                 38 cm³ |
| fuel                                                        |
+-------------------------------------------------------------+
| 21psi  21psi |                    |                         |
| [FL]    [FR] |         2          |           35            |
|              |                    |                         |
| [RL]    [RR] |      30 km/h       |         shifts          |
| 21psi  21psi |    5000 giri/m     |                         |
+-------------------------------------------------------------+
| [=====] THR  |   104°     80°     |         13.3 V          |
| [===  ] BRK  |  water     oil     |        battery          |
+-------------------------------------------------------------+
```

---

## 🏎️ Elementi Implementati

1. **Shift Lights (24 LED NeoPixel)**:
   - 8 LED Verdi (RPM bassi-medi: 2500 - 6000 RPM)
   - 8 LED Rossi (RPM medi-alti: 6000 - 9500 RPM)
   - 8 LED Blu (Shift zone / Limitatore: 9500 - 11800 RPM)
   - Lampeggio rapido di allarme limitatore sopra gli 11800 RPM.
2. **Riga Superiore (Header)**:
   - Quantità carburante: `13 l` con etichetta `fuel`.
   - Consumo / Volume: `38 cm³` con esponente 3 in apice.
3. **Sezione Centrale**:
   - **4 Gomme (FL, FR, RL, RR)** con pressione indicata (`21psi`) e cornice racing verde.
   - **Indicatore Marcia Centrale**: Cifra gigante ad alta visibilità (`N`, `1`, `2`, `3`, `4`, `5`, `6`).
   - **Velocità**: `30 km/h`.
   - **Giri Motore**: `5000 giri/m` (cambia colore in giallo/rosso all'avvicinarsi della zona rossa).
   - **Autonomia Cambiate ad Aria Compressa (Pneumatic Shifter)**:
     - Gestione bombola ad aria con capacità impostata di prova a **50 shifts**.
     - Ogni cambiata (upshift o downshift) consuma 1 shift d'aria e decrementa il contatore a display.
     - Codice colore dinamico del contatore:
       - **Bianco**: Pressione ottimale (> 10 shifts rimanenti)
       - **Giallo**: Riserva aria (1-10 shifts rimanenti)
       - **Rosso**: Aria esaurita (0 shifts) con blocco hardware della cambiata (impossibile cambiare marcia senza pressione aria).
     - Comando di ricarica: `refill` via Serial Monitor su Wokwi o pulsante `Ricarica Aria (50)` nel simulatore web.
4. **Riga Inferiore (Pedali, Temperature, Batteria)**:
   - **Barra Acceleratore (THR)**: Verde, dinamica in percentuale (0-100%).
   - **Barra Freno (BRK)**: Rossa, dinamica in percentuale (0-100%).
   - **Temperature**: `104° water` (Acqua) e `80° oil` (Olio) con simboli gradi. Allerta rossa in caso di surriscaldamento.
   - **Batteria**: `13.3 V battery`.

---

## 🛠️ Come Simulare e Testare la Dashboard

Hai a disposizione due modalità di test immediate:

### Opzione 1: Simulatore Online Ufficiale Wokwi (Consigliato)

1. Apri il browser al link del simulatore Wokwi ESP32:
   👉 **[https://wokwi.com/projects/new/esp32](https://wokwi.com/projects/new/esp32)**
2. Nel progetto che si apre su Wokwi:
   - Copia il contenuto di **`sketch.ino`** e incollalo nel tab `sketch.ino`.
   - Clicca sul tab **`diagram.json`** e sostituisci il contenuto con quello del file **`diagram.json`**.
   - Clicca sull'icona della freccetta accanto ai file, crea un nuovo file chiamato **`libraries.txt`** e incolla:
     ```text
     Adafruit GFX Library
     Adafruit ILI9341
     Adafruit NeoPixel
     ```
3. Clicca sul pulsante verde **"Start Simulation"** (Play).
4. **Interazione**:
   - La dashboard si avvia in modalità **AUTO DEMO**, simulando un giro in pista con accelerazioni, cambiate marcia, staccate e curve!
   - Puoi usare i pulsanti fisici cablati nel simulatore:
     - **SHIFT UP** (verde): sale di marcia.
     - **SHIFT DOWN** (rosso): scala marcia.
     - **AUTO/MAN** (blu): attiva la modalità manuale gestita dal potenziometro del gas!
   - Nel **Serial Monitor** di Wokwi puoi inviare comandi istantanei in tempo reale digitando `help` per vedere l'elenco (es. `refill` per ricaricare l'aria a 50, `air 35` per impostare un valore specifico, `gear 3`, `rpm 11800`, `thr 90`, `brk 40`, `water 106`, ecc.).

---

### Opzione 2: Simulatore Web Istantaneo Locale (`simulator.html`)

Per testare e verificare visivamente la dashboard all'istante sul tuo computer senza dover fare upload su internet:
1. Fai doppio click sul file **`simulator.html`** nella cartella del progetto (si aprirà direttamente in Edge o Chrome).
2. Potrai:
   - Vedere la dashboard renderizzata su Canvas con la barra di 24 LED funzionante a 60 FPS.
   - Regolare qualsiasi parametro in tempo reale con gli slider (RPM, Acceleratore, Freno, Marcia, Pressione Gomme, Temperature, Batteria, Carburante).
   - Attivare o mettere in pausa l'auto-simulazione di giro di pista.

---

### Opzione 3: Estensione Wokwi in VS Code

Se utilizzi Visual Studio Code:
1. Installa l'estensione **"Wokwi Simulator"** dal marketplace di VS Code.
2. Apri la cartella del progetto in VS Code.
3. Premi `F1` e digita: `Wokwi: Start Simulator`.

---

## 📁 Struttura dei File

- `sketch.ino`: Codice sorgente firmware Arduino/C++ per ESP32, ottimizzato per evitare flickering su display SPI tramite redraw selettivo.
- `diagram.json`: Schema circuitale con ESP32, TFT ILI9341, strip NeoPixel 24 LED, potenziometro acceleratore e pulsanti marcia.
- `libraries.txt`: Elenco delle librerie gestite in automatico da Wokwi.
- `wokwi.toml`: Configurazione per Wokwi CLI ed estensione VS Code.
- `simulator.html`: Simulatore grafico interattivo e standalone per browser.
- `bozza.jpeg`: Immagine di riferimento fornita.
