# data/

This folder is the LittleFS filesystem image for the ESP32. At build time, PlatformIO packs every file here into a flash partition that the firmware mounts as a read-only filesystem. The web dashboard and both TFLite model binaries are served from this partition.

---

## Files

| File | Size (approx.) | Purpose |
|---|---|---|
| `dashboard.html` | — | Single-page web dashboard app (mobile-first, max-width 480 px) |
| `style.css` | — | Dark cyberpunk stylesheet; neon cyan/orange/green accents, radial gradient background |
| `script.js` | — | Polling loop and all client-side API helpers |
| `traffic_model.tflite` | ~4.7 KB | Supervised (greedy) TFLite model; loaded as **Greedy ML** mode |
| `traffic_rl_model.tflite` | ~4.8 KB | RL (PPO) TFLite model; loaded as **RL** mode |

---

## Dashboard

The dashboard is a self-contained SPA delivered by the ESP32's HTTP server. It polls `/api/status` every 100 ms and renders live intersection state without a page reload.

### Panels

1. **System Status** — current phase name, time remaining, and a mode badge (`NORMAL` / `GREEDY` / `RL` / `OVERRIDE` / `EMERGENCY`).
2. **Mode Control** — three-tab segmented control. Each tab calls `POST /api/ml` with `{"mode": "..."}`.
   - Normal — baseline fixed-cycle; avg ~30% waiting time.
   - Greedy ML — supervised model; avg ~24% waiting time.
   - RL — PPO model; avg ~20% waiting time.
   - Percentage figures are benchmark-derived under an imbalanced traffic scenario.
3. **Brightness** — LDR sensor reading bar, night mode badge, and an auto-dim toggle.
4. **Intersection Visualizer** — four roads with RGB LED indicators showing live signal state; queue percentage overlaid on each road arm.
5. **Manual Override** — force a specific road green, or restore the automatic cycle.
6. **Road Intensity & Queue Depth** — per-road `LOW` / `MED` / `HIGH` intensity buttons and a color-coded queue depth bar (green < 30%, yellow 30-70%, red > 70%).

### script.js API surface

| Function | Description |
|---|---|
| `poll()` | Fetches `/api/status` every 100 ms |
| `onStateUpdate(state)` | Renders all live state: LEDs, queue bars, mode tabs, badge, brightness |
| `setMode(mode)` | `POST /api/ml` with `{"mode": "normal" \| "greedy" \| "rl"}` |
| `setIntensity(road, level)` | `POST /api/intensity` with road and level |
| `override(dir)` | `POST /api/override` to force one road green |
| `clearOverride()` | `POST /api/override` to restore automatic cycle |
| `toggleDim()` | `POST /api/dim` to toggle auto-brightness |

---

## TFLite Models

Both models share the same 8-float input tensor:

```
[q_top, q_bottom, q_left, q_right, i_top, i_bottom, i_left, i_right]
```

where `q_*` is normalized queue depth and `i_*` is traffic intensity per road.

The firmware loads both files from LittleFS at boot via `mlInit()` in `src/ml.cpp`. The supervised model (`traffic_model.tflite`) is required. The RL model (`traffic_rl_model.tflite`) is optional; if absent, the firmware continues operating in supervised mode without the RL tab.

Source notebooks and training scripts for retraining either model are in `ml/traffic_model.ipynb`.

---

## Uploading to the ESP32

After any change to a file in this folder, repack and flash the LittleFS image:

```bash
pio run -t uploadfs
```

This command rebuilds the entire filesystem image from the `data/` directory and uploads it to the designated flash partition. Individual files cannot be updated in isolation; the full image is always replaced.
