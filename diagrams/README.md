# diagrams/

Visual reference material for the ESP32 adaptive traffic light controller.

---

## Hardware Schematic

**`Schematic_traffic1.png`**

Full circuit schematic. Shows the ESP32 pinout, the 74HC595N shift register wiring (DATA=27, CLOCK=26, LATCH=25) that drives all red and yellow LEDs, the direct-drive GPIO connections to the four green LEDs (top=2, bottom=16, left=17, right=21), the emergency indicator LED on GPIO 22, the LDR sensor on GPIO 34 (ADC1), and the RFID-RC522 SPI bus (CS=5, RST=4, MISO=19, MOSI=23, SCK=18). Use this as the authoritative wiring reference when assembling or debugging the hardware.

---

## State Diagrams (`state diagrams/`)

Three Mermaid-generated PNG state diagrams, each covering a different level of the firmware's phase logic. Source files live in `state diagrams/scripts/` and can be regenerated with `python state diagrams/export_diagrams.py`.

| File | Script | What it shows |
|------|--------|----------------|
| `overview.png` | `scripts/overview.mmd` | Top-level operating modes: Cycle (normal), Override (one road held green via API), and Emergency (all-red for 5 s on RFID scan). Shows all transitions including the API calls and the RFID trigger. |
| `fixed_cycle.png` | `scripts/fixed_cycle.mmd` | The round-robin green sequence inside Cycle when ML is off: Top → Bottom → Left → Right → repeat. Each arrow is a timer expiry (green or yellow). Useful for understanding default timing behavior and night-mode doubling. |
| `ml_cycle.png` | `scripts/ml_cycle.mmd` | The two-state loop used in ML Control mode: a model-chosen road stays green for 5–15 s, then all roads go yellow for 2 s while `mlInfer()` selects the next road and duration from the current queue levels. |

Narrative descriptions of every state and transition are in `state diagrams/STATE_DIAGRAM.md`.

---

## Performance Graphs (`graphs/`)

**`rl_learning_curve.png`**

Training curve for the PPO reinforcement-learning model (`traffic_rl_model.tflite`). Plots cumulative reward over training episodes, showing convergence. Useful for verifying that training ran to completion and for comparing future retraining runs.

**`benchmark.png`**

Side-by-side throughput comparison of the three operating modes (Normal fixed-cycle, Greedy ML supervised model, RL PPO model) under several traffic scenarios. Shows why the RL model was chosen as the default ML option: it handles imbalanced lane loads better than the supervised baseline.
