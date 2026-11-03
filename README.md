# Formula SAE Digital Dashboard (STM32 Nucleo-L031K6 / Nucleo-32)

Dashboard digitale da volante per monoposto **Formula SAE / Formula Student**, ottimizzata per **STM32 Nucleo-L031K6** (form factor **Nucleo-32**, identico alla scheda fisica **Nucleo-G431KB**) e simulabile direttamente su **Wokwi** con display SPI **ILI9341 320x240**.

Il layout rispetta fedelmente la disposizione e gli elementi della bozza fornita (`bozza.jpeg`):

```text
 🟢🟢🟢🟢🟢🟢🟢🟢 🔴🔴🔴🔴🔴🔴🔴🔴 🔵🔵🔵🔵🔵🔵🔵🔵  (24 Shift LEDs)
+-------------------------------------------------------------+
| 13 l                                                 38 cm³ |
| fuel                                                        |
+-------------------------------------------------------------+
| 21psi  21psi |                    |                         |
| [FL]    [FR] |         2          |           50            |
|              |                    |                         |
| [RL]    [RR] |      30 km/h       |         shifts          |
| 21psi  21psi |    5000 giri/m     |                         |
+-------------------------------------------------------------+
| [=====] THR  |   104°     80°     |         13.3 V          |
| [===  ] BRK  |  water     oil     |        battery          |
+-------------------------------------------------------------+
```

---

## 📌 Mappatura Pin (STM32 Nucleo-32: L031K6 & G431KB)

Entrambe le schede condividono la medesima piedinatura compatibile Arduino Nano (header CN3 e CN4):

| Segnale Hardware | Pin Header Nucleo-32 | MCU Pin (L031K6) | MCU Pin (G431KB) | Connessione Wokwi |
| :--- | :--- | :--- | :--- | :--- |
| **SPI SCK** | `D13` | `PB3` | `PB3` | `lcd1:SCK` (Clock SPI hardware) |
| **SPI MOSI** | `D11` | `PB5` | `PB5` | `lcd1:MOSI` (Dati SPI hardware) |
| **TFT CS** | `D3` | `PB0` | `PB0` | `lcd1:CS` (Chip Select) |
| **TFT DC** | `D2` | `PA12` | `PA10` | `lcd1:D/C` (Data / Command) |
| **Alimentazione** | `3V3` / `GND` | `3V3` / `GND` | `3V3` / `GND` | VCC, LED e Ground del display |
| **Shift UP** | `D4` | `PB7` | `PB7` | Pulsante Marcia UP (Active LOW, pull-up int.) |
| **Shift DOWN** | `D5` | `PB6` | `PB6` | Pulsante Marcia DOWN (Active LOW, pull-up int.)|
| **REFILL Aria (50x)**| `D7` | `PC14` | `PC14` | Pulsante Ricarica Bombola Aria Compressa |
| **Acceleratore (Pot)**| `A0` | `PA0` | `PA0` | Potenziometro Gas THR (Ingresso Analogico ADC) |
| **Freno (Pot)**| `A1` | `PA1` | `PA1` | Potenziometro Freno BRK (Ingresso Analogico ADC) |

---

## ⚡ Ottimizzazioni per la Memoria Flash di Wokwi (32 KB Limit)

La scheda simulata **Nucleo-L031K6** (`board-st-nucleo-l031k6`) possiede **32 KB di Flash** e **8 KB di RAM**. 

Per garantire che la compilazione rientri perfettamente nei limiti di memoria del simulatore:
1. **Eliminato l'overhead di HardwareSerial**: disattivato il driver UART su Wokwi (risparmia oltre 7 KB di Flash). Sulla scheda fisica Nucleo-G431KB (128 KB Flash) puoi riattivarlo senza problemi.
2. **Rendering geometrico puro**: rimosse le routine pesanti dei cerchi (`drawCircle`, `fillRoundRect`), sostituite da rendering rettangolare da motorsport ad alte prestazioni (risparmia circa 3 KB di Flash).
3. **Zero formattazione float**: eliminata la libreria `_vfprintf_float` (risparmia circa 25 KB di Flash).
4. **Shift Lights a schermo**: le 24 luci di cambiata (8 verdi, 8 rosse, 8 blu) sono disegnate direttamente nell'header del display con animazione e limitatore lampeggiante.
5. **Stringhe statiche disegnate una volta sola**: tutte le etichette fisse (`km/h`, `giri/m`, `shifts`, `THR`, `BRK`, ecc.) sono stampate in fase di `setup()`, azzerando i trasferimenti di stringhe ridondanti.

---

## 🛠️ Come Simulare su Wokwi Online

1. Apri il simulatore **STM32 Nucleo-L031K6** su Wokwi:
   👉 **[https://wokwi.com/projects/new/st-nucleo-l031k6](https://wokwi.com/projects/new/st-nucleo-l031k6)**
2. Assicurati che **`libraries.txt`** contenga:
   ```text
   Adafruit GFX Library
   Adafruit ILI9341
   ```
3. Incolla il codice da [sketch.ino](file:///C:/Users/lampa/Desktop/Simulazione%20Dashboard/sketch.ino) nel tab **sketch.ino**.
4. Incolla il circuito da [diagram.json](file:///C:/Users/lampa/Desktop/Simulazione%20Dashboard/diagram.json) nel tab **diagram.json**.
5. Clicca su **"Start Simulation"** (tasto Play verde).
