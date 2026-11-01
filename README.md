# Formula SAE Digital Dashboard (STM32 Nucleo-G431KB + ILI9341 + Wokwi)

Dashboard digitale da volante per monoposto **Formula SAE / Formula Student**, ottimizzata per **STM32 Nucleo-G431KB** (form factor Nucleo-32) e per il simulatore **Wokwi** (display SPI **ILI9341 320x240**).

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

## 📌 Mappatura Pin (STM32 Nucleo-G431KB / Nucleo-32)

| Segnale Hardware | Pin Nucleo-32 (G431KB) | STM32 MCU Pin | Connessione Wokwi |
| :--- | :--- | :--- | :--- |
| **SPI SCK** | `D13` | `PB3` | `lcd1:SCK` (Clock SPI hardware) |
| **SPI MOSI** | `D11` | `PB5` | `lcd1:MOSI` (Dati SPI hardware) |
| **TFT CS** | `D3` | `PB0` | `lcd1:CS` (Chip Select) |
| **TFT DC** | `D2` | `PA10` | `lcd1:D/C` (Data / Command) |
| **Alimentazione** | `3V3` / `GND` | `3V3` / `GND` | VCC, LED e Ground del display |
| **Shift UP** | `D4` | `PB7` | Pulsante Marcia UP (Active LOW, pull-up int.) |
| **Shift DOWN** | `D5` | `PB6` | Pulsante Marcia DOWN (Active LOW, pull-up int.)|
| **REFILL Aria (50x)**| `D7` | `PC14` | Pulsante Ricarica Bombola Aria Compressa |
| **Acceleratore (Pot)**| `A0` | `PA0` | Potenziometro Gas (Ingresso Analogico ADC) |

---

## ⚡ Ottimizzazioni per la Memoria Flash di Wokwi (32 KB Limit)

La tua scheda fisica (**Nucleo-G431KB**) possiede **128 KB di Flash**. Il simulatore cloud Wokwi compila invece per `Nucleo-C031C6`, che ha il limite hardware di soli **32 KB**. 

Per far compilare il progetto su Wokwi senza errori di overflow della memoria Flash:
1. **Eliminato l'overhead di HardwareSerial**: disattivato il driver UART su Wokwi (risparmia oltre 7 KB di Flash). Sulla scheda fisica Nucleo-G431KB puoi riattivarlo impostando `#define ENABLE_SERIAL 1`.
2. **Rendering geometrico puro**: rimosse le routine pesanti dei cerchi (`drawCircle`, `fillRoundRect`), sostituite da rendering rettangolare da motorsport ad alte prestazioni (risparmia circa 3 KB di Flash).
3. **Zero formattazione float**: eliminata la libreria `_vfprintf_float` (risparmia circa 25 KB di Flash).
4. **Shift Lights a schermo**: le 24 luci di cambiata (8 verdi, 8 rosse, 8 blu) sono disegnate direttamente nell'header del display con animazione e limitatore lampeggiante.

---

## 🛠️ Come Simulare su Wokwi Online

Link diretto al progetto: https://wokwi.com/projects/479505957334903809

1. Apri il simulatore STM32 Nucleo su Wokwi:
   👉 **[https://wokwi.com/projects/new/st-nucleo-c031c6](https://wokwi.com/projects/new/st-nucleo-c031c6)**
2. Assicurati che **`libraries.txt`** contenga solo:
   ```text
   Adafruit GFX Library
   Adafruit ILI9341
   ```
3. Incolla il codice ottimizzato da [sketch.ino](file:///C:/Users/lampa/Desktop/Simulazione%20Dashboard/sketch.ino) nel tab **sketch.ino**.
4. Clicca su **"Start Simulation"** (Play verde): la compilazione passerà senza errori e la dashboard si avvierà subito a schermo!
