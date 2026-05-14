const ROADS     = ['top', 'bottom', 'left', 'right'];
const INT_LEVEL = {'low': 0, 'med': 1, 'high': 2};

let state = null;

// ── Rendering helpers ─────────────────────────────────────────────────────────

function queueColor(pct) {
    return pct > 70 ? '#ff4444' : pct > 30 ? '#ffd000' : '#00ff66';
}

function renderQueues(queues, intensity) {
    ROADS.forEach((r, i) => {
        const pct   = Math.round(queues[i] * 100);
        const color = queueColor(pct);

        const bar   = document.getElementById('q-' + r);
        const pctEl = document.getElementById('q-' + r + '-pct');
        if (bar)   { bar.style.width = pct + '%'; bar.style.background = color; }
        if (pctEl) pctEl.textContent = pct + '%';

        const ql = document.getElementById('ql-' + r);
        if (ql) { ql.textContent = pct + '%'; ql.style.color = color; }

        ['low', 'med', 'high'].forEach((lvl, li) => {
            const btn = document.getElementById('int-' + r + '-' + lvl);
            if (btn) btn.classList.toggle('active', intensity[i] === li);
        });
    });
}

// ── Live state update (called every 100 ms from poll()) ───────────────────────

function onStateUpdate(s) {
    if (!s || !s.leds) return;

    // Intersection lights
    ROADS.forEach(r => {
        const road = document.getElementById(r + '-road');
        if (!road) return;
        road.querySelectorAll('.light').forEach(l => l.classList.remove('active'));
        const target = road.querySelector('.' + (s.leds[r] || 'red'));
        if (target) target.classList.add('active');
    });

    const queues    = s.queues    ? ROADS.map(r => s.queues[r] ?? 0)               : [0,0,0,0];
    const intensity = s.intensity ? ROADS.map(r => INT_LEVEL[s.intensity[r]] ?? 1) : [1,1,1,1];
    renderQueues(queues, intensity);

    document.getElementById('ui-phase').textContent =
        (s.phase || '--').replace('_', ' ').toUpperCase();
    document.getElementById('ui-timer').textContent = Math.ceil(s.remaining / 1000) + 's';

    const mode  = s.mode || 'normal';
    const badge = document.getElementById('ui-mode-badge');
    if (badge) { badge.textContent = mode.toUpperCase(); badge.className = 'mode-badge ' + mode; }

    const btnMl = document.getElementById('btn-ml');
    if (btnMl) {
        btnMl.textContent      = s.mlMode ? 'Disable ML Mode' : 'Enable ML Mode';
        btnMl.style.background = s.mlMode ? 'linear-gradient(135deg,#003a4a,#00e1ff)' : '';
        btnMl.style.color      = s.mlMode ? '#fff' : '';
        btnMl.style.border     = s.mlMode ? 'none' : '';
    }

    const brt      = s.brightness ?? 0;
    const barColor = brt < 30 ? '#ff7b00' : brt < 60 ? '#ffd000' : '#00ff66';
    document.getElementById('ldr-value').textContent      = Math.round(brt) + '%';
    document.getElementById('ldr-value').style.color      = barColor;
    document.getElementById('ldr-bar').style.width        = brt + '%';
    document.getElementById('ldr-bar').style.background   = barColor;
    document.getElementById('ldr-threshold').textContent  = (s.threshold ?? '--') + '%';
    document.getElementById('night-badge').style.display  = s.nightMode ? 'inline-block' : 'none';
    document.getElementById('autodim-state').textContent  = s.autoDim ? 'ON' : 'OFF';
    document.getElementById('dim-btn').textContent        =
        s.autoDim ? '🌙 Disable Auto-Dim' : '☀️ Enable Auto-Dim';

    document.getElementById('debug').textContent = JSON.stringify(s, null, 2);
}

// ── Control actions ───────────────────────────────────────────────────────────

async function toggleML() {
    const enabled = !(state && state.mlMode);
    await fetch('/api/ml', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({enabled}),
    });
}

async function setIntensity(road, level) {
    await fetch('/api/intensity', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({road, level}),
    });
}

async function override(dir) {
    await fetch('/api/override', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({direction: dir}),
    });
}

async function clearOverride() {
    await fetch('/api/override', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({direction: 'none'}),
    });
}

async function toggleDim() {
    await fetch('/api/dim', {method: 'POST'});
}

// ── Polling loop ──────────────────────────────────────────────────────────────

async function poll() {
    try {
        const res = await fetch('/api/status');
        state = await res.json();
        onStateUpdate(state);
    } catch (e) {
        const badge = document.getElementById('ui-mode-badge');
        if (badge) { badge.textContent = 'OFFLINE'; badge.className = 'mode-badge'; }
    }
}
setInterval(poll, 100);
poll();
