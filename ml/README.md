# ml/ — Traffic Light ML Pipeline

This folder contains the machine learning pipeline for the adaptive traffic light system. It trains two models — a supervised imitation model and a reinforcement learning model — and exports both to TFLite for deployment on an ESP32 microcontroller.

## Files

| File | Description |
|------|-------------|
| `traffic_model.ipynb` | Main notebook: simulation, supervised training, RL (PPO) training, TFLite export |
| `main.py` | Minimal entry point / placeholder |
| `traffic_model.tflite` | Exported supervised model (~4.7 KB) |
| `traffic_rl_model.tflite` | Exported RL model (~4.8 KB) |
| `pyproject.toml` | Python dependencies (TensorFlow, NumPy, Matplotlib, Jupyter) |
| `.python-version` | Pins Python 3.13 |
| `uv.lock` | Dependency lock file |

## Simulation Engine

The `TrafficSim` class simulates a 4-road intersection. Each road has a normalized queue value in [0, 1]. On every 500 ms tick, the active (green) road drains at −0.03 per tick, while the other three accumulate at +0.015 × intensity per tick. Intensity levels are 0.3 (low), 0.6 (medium), and 1.0 (high). Queue values are clamped to [0, 1]. The simulator is used both to generate training data for the supervised model and as the environment for RL training.

## Supervised Model

The supervised model is trained via greedy imitation. A greedy policy runs for 2,000 steps, always selecting the road with the highest `queue × intensity` product and scaling the green duration proportionally to queue length. This deterministic behavior produces a clean dataset with 100% road-selection consistency.

The model takes 8 floats as input — the four queue values and four intensities — and produces two outputs: a road index (argmax of a 4-class softmax) and a green duration in seconds (linear output, clamped to 5–15 s). The architecture uses three shared dense layers (8 → 16 → 8) before splitting into the two output heads. It trains for 50 epochs with categorical cross-entropy for road selection and MSE for duration, achieving 100% road selection accuracy on the test split.

## RL Model (PPO)

The RL model is trained with Proximal Policy Optimization. The network shares a trunk (8 → 32 → 16) and branches into an actor (road softmax + duration linear) and a critic (16 → 1 value head). PPO is configured with a clipping epsilon of 0.2 and 3 gradient update epochs per episode. Advantage estimation uses GAE with lambda = 0.95.

The reward signal is the reduction in total queue per phase: `(prev_queue_sum - new_queue_sum) / 4`. Training runs for up to 15,000 episodes across a three-stage curriculum — easy (one dominant road), medium (mixed intensities), hard (random) — with early stopping at patience 1,500 episodes and an improvement threshold of 0.5.

## Benchmark Results

**Balanced traffic (all medium intensity):**

| Policy | Avg Total Queue |
|--------|----------------|
| Round-robin | 0.65 – 1.62 |
| Supervised | ~1.20 |
| RL | 0.61 – 1.62 |

The RL model matches round-robin under balanced load, having learned a fair rotation without being explicitly instructed to do so.

**Imbalanced traffic (top road high, others low):**

| Policy | Avg Total Queue |
|--------|----------------|
| Round-robin | 1.09 – 1.28 |
| Supervised | 0.68 – 1.21 |
| RL | 0.42 – 1.17 |

The RL model achieves up to 40% lower queue totals than round-robin and around 15% lower than the supervised model, demonstrating that it learned to prioritize high-load roads more aggressively.

## TFLite Export

TensorFlow 2.20 has a known bug with direct conversion of multi-output models. The workaround is to trace a `tf.function` concrete function and convert from that rather than calling `model.save()` directly. Both models are exported to `.tflite` files in this folder and copied to the `data/` directory for upload to the ESP32's LittleFS filesystem.

## Running the Pipeline

This project uses `uv` for dependency management.

```
uv sync
jupyter notebook traffic_model.ipynb
```

Run all cells in order. Simulation, training, evaluation, and export are executed sequentially within the notebook.
