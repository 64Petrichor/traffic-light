// Current system state — read this anywhere in your UI code
let state = null;

// Called every 100ms with fresh state
function onStateUpdate(s) {
    if (!s || !s.leds) return;

    function updateRoad(id, color) {
        const road = document.getElementById(id + "-road");
        if (!road) return;
        road.querySelectorAll(".light").forEach(l => l.classList.remove("active"));
        const target = road.querySelector("." + (color || 'red'));
        if (target) target.classList.add("active");
    }

    updateRoad("top", s.leds.top);
    updateRoad("bottom", s.leds.bottom);
    updateRoad("left", s.leds.left);
    updateRoad("right", s.leds.right);

    document.getElementById('ui-phase').textContent = (s.phase || '--').replace('_', ' ').toUpperCase();
    document.getElementById('ui-timer').textContent = Math.ceil(s.remaining / 1000) + 's';

    // LDR brightness card
    const brt = s.brightness ?? 0;
    const barColor = brt < 30 ? '#ff7b00' : brt < 60 ? '#ffd000' : '#00ff66';
    document.getElementById('ldr-value').textContent = Math.round(brt) + '%';
    document.getElementById('ldr-value').style.color = barColor;
    document.getElementById('ldr-bar').style.width = brt + '%';
    document.getElementById('ldr-bar').style.background = barColor;
    document.getElementById('ldr-threshold').textContent = (s.threshold ?? '--') + '%';
    document.getElementById('night-badge').style.display = s.nightMode ? 'inline-block' : 'none';
    document.getElementById('autodim-state').textContent = s.autoDim ? 'ON' : 'OFF';
    document.getElementById('dim-btn').textContent = s.autoDim ? '🌙 Disable Auto-Dim' : '☀️ Enable Auto-Dim';
    document.getElementById('debug').textContent = JSON.stringify(s, null, 2);
}

// Force one direction green. dir = "top" | "bottom" | "left" | "right"
async function override(dir) {
    await fetch('/api/override', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ direction: dir }) });
}

// Clear override and return to normal cycle
async function clearOverride() {
    await fetch('/api/override', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ direction: 'none' }) });
}

// Toggle auto-dim on/off
async function toggleDim() {
    await fetch('/api/dim', { method: 'POST' });
}

// Update phase durations. All values are in milliseconds.
// Omit a key to leave that timing unchanged.
async function setTimings({ top, bottom, left, right, yellow }) {
    const body = {};
    if (top != null) body.top = top;
    if (bottom != null) body.bottom = bottom;
    if (left != null) body.left = left;
    if (right != null) body.right = right;
    if (yellow != null) body.yellow = yellow;
    await fetch('/api/timing', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
}

function applySettings() {
    const g = parseInt(document.getElementById('in-green').value);
    const y = parseInt(document.getElementById('in-yellow').value);
    setTimings({top: g, bottom: g, left: g, right: g, yellow: y});
}

// ── Polling loop (do not modify) ─────────────────────────────────────
async function poll() {
    try {
        const res = await fetch('/api/status');
        state = await res.json();
        onStateUpdate(state);
    } catch (e) {
        console.log("Waiting for ESP32...");
    }
}
setInterval(poll, 100);
poll();
