# ml/ — Traffic Light ML Pipeline

This folder contains the machine learning pipeline for the adaptive traffic light system. It trains a supervised imitation model and exports it to TFLite for deployment on an ESP32 microcontroller.

## Files

| File | Description |
|------|-------------|
| `traffic_light.ipynb` | Main notebook: simulation, supervised training, TFLite export |
| `main.py` | Minimal entry point / placeholder |
| `traffic_model.tflite` | Exported supervised model (~4.7 KB) |
| `pyproject.toml` | Python dependencies (TensorFlow, NumPy, Matplotlib, Jupyter) |
| `.python-version` | Pins Python 3.13 |
| `uv.lock` | Dependency lock file |

## Simulation Engine

The `TrafficSim` class simulates a 4-road intersection. Each road has a normalized queue value in [0, 1]. On every 500 ms tick, the active (green) road drains at −0.03 per tick, while the other three accumulate at +0.015 × intensity per tick. Intensity levels are 0.3 (low), 0.6 (medium), and 1.0 (high). Queue values are clamped to [0, 1]. The simulator is used to generate training data for the supervised model.

## Supervised Model

The supervised model is trained via greedy imitation. A greedy policy runs for 2,000 steps, always selecting the road with the highest `queue × intensity` product and scaling the green duration proportionally to queue length. This deterministic behavior produces a clean dataset with 100% road-selection consistency.

The model takes 8 floats as input — the four queue values and four intensities — and produces two outputs: a road index (argmax of a 4-class softmax) and a green duration in seconds (linear output, clamped to 5–15 s). The architecture uses three shared dense layers (8 → 16 → 8) before splitting into the two output heads. It trains for 50 epochs with categorical cross-entropy for road selection and MSE for duration, achieving 100% road selection accuracy on the test split.

## Benchmark Results

**Balanced traffic (all medium intensity):**

| Policy | Avg Total Queue |
|--------|----------------|
| Round-robin | 0.65 – 1.62 |
| Supervised | ~1.20 |

**Imbalanced traffic (top road high, others low):**

| Policy | Avg Total Queue |
|--------|----------------|
| Round-robin | 1.09 – 1.28 |
| Supervised | 0.68 – 1.21 |

The supervised model achieves lower queue totals than round-robin on imbalanced traffic, demonstrating that it learned to prioritize high-load roads.

## TFLite Export

TensorFlow 2.20 has a known bug with direct conversion of multi-output models. The workaround is to trace a `tf.function` concrete function and convert from that rather than calling `model.save()` directly. The model is exported to a `.tflite` file in this folder and copied to the `data/` directory for upload to the ESP32's LittleFS filesystem.

## Running the Pipeline

This project uses `uv` for dependency management.

```
uv sync
jupyter notebook traffic_light.ipynb
```

Run all cells in order. Simulation, training, evaluation, and export are executed sequentially within the notebook.
