# data/

This folder is the LittleFS filesystem image for the ESP32. At build time, PlatformIO packs every file here into a flash partition that the firmware mounts as a read-only filesystem. The web dashboard is served from this partition.

---

## Files

| File | Size (approx.) | Purpose |
|---|---|---|
| `dashboard.html` | — | Single-page web dashboard app (mobile-first, max-width 480 px) |
| `style.css` | — | Dark cyberpunk stylesheet; neon cyan/orange/green accents, radial gradient background |
| `script.js` | — | Polling loop and all client-side API helpers |

---

## Dashboard

The dashboard is a self-contained SPA delivered by the ESP32's HTTP server. It polls `/api/status` every 100 ms and renders live intersection state without a page reload.

### Panels

1. **System Status** — current phase name, time remaining, and a mode badge (`NORMAL` / `GREEDY` / `OVERRIDE` / `EMERGENCY`).
2. **Mode Control** — two-tab segmented control. Each tab calls `POST /api/ml` with `{"mode": "..."}`.
   - Normal — baseline fixed-cycle; avg ~30% waiting time.
   - Adaptive — greedy heuristic; serves the highest-queue road first.
   - Percentage figures are benchmark-derived under an imbalanced traffic scenario.
3. **Intersection Visualizer** — four roads with RGB LED indicators showing live signal state; queue percentage overlaid on each road arm.
4. **Simulation Speed** — three buttons (×1 / ×2 / ×5) that call `POST /api/speed`. Scales both queue tick rate and phase durations so Adaptive mode can warm up faster.
5. **Manual Override** — force a specific road green, or restore the automatic cycle.
6. **Road Intensity & Queue Depth** — per-road `LOW` / `MED` / `HIGH` intensity buttons and a color-coded queue depth bar (green < 30%, yellow 30-70%, red > 70%).
7. **Brightness** — LDR sensor reading bar, night mode badge, and an auto-dim toggle.

### script.js API surface

| Function | Description |
|---|---|
| `poll()` | Fetches `/api/status` every 100 ms |
| `onStateUpdate(state)` | Renders all live state: LEDs, queue bars, mode tabs, badge, brightness |
| `setMode(mode)` | `POST /api/ml` with `{"mode": "normal" \| "greedy"}` |
| `setIntensity(road, level)` | `POST /api/intensity` with road and level |
| `override(dir)` | `POST /api/override` to force one road green |
| `clearOverride()` | `POST /api/override` to restore automatic cycle |
| `toggleDim()` | `POST /api/dim` to toggle auto-brightness |
| `setSpeed(speed)` | `POST /api/speed` with `{"speed": 1\|2\|5}` |

---

## Adaptive Mode

In Adaptive mode, `mlInfer()` in `src/ml.cpp` runs the greedy scheduling formula directly in firmware. No model file is needed — the LittleFS partition only stores the dashboard HTML, CSS, and JS.

---

## Uploading to the ESP32

After any change to a file in this folder, repack and flash the LittleFS image:

```bash
pio run -t uploadfs
```

This command rebuilds the entire filesystem image from the `data/` directory and uploads it to the designated flash partition. Individual files cannot be updated in isolation; the full image is always replaced.
