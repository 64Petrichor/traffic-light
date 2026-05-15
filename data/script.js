const ROADS     = ['top', 'bottom', 'left', 'right'];
const INT_LEVEL = {'low': 0, 'med': 1, 'high': 2};

let state = null;
const modeStats = {
    normal: { sum: 0, count: 0 },
    greedy: { sum: 0, count: 0 },
};

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

    // Accumulate avg queue % for the active mode
    const mlMode = s.mlMode || 'normal';
    if (modeStats[mlMode]) {
        const avgPct = Math.round((queues[0] + queues[1] + queues[2] + queues[3]) / 4 * 100);
        modeStats[mlMode].sum   += avgPct;
        modeStats[mlMode].count += 1;
    }
    // Update mode-stat labels
    ['normal', 'greedy'].forEach(m => {
        const el = document.getElementById('stat-' + m);
        if (!el) return;
        const st = modeStats[m];
        el.textContent = st.count > 0 ? 'avg ' + Math.round(st.sum / st.count) + '% waiting' : '--';
    });

    document.getElementById('ui-phase').textContent =
        (s.phase || '--').replace('_', ' ').toUpperCase();
    const displayMs = (s.remaining || 0) * (s.simSpeed || 1);
    document.getElementById('ui-timer').textContent = Math.ceil(displayMs / 1000) + 's';

    const mode  = s.mode || 'normal';
    const badge = document.getElementById('ui-mode-badge');
    if (badge) { badge.textContent = mode.toUpperCase(); badge.className = 'mode-badge ' + mode; }

    ['normal', 'greedy'].forEach(m => {
        const tab = document.getElementById('tab-' + m);
        if (tab) tab.classList.toggle('active', mlMode === m);
    });

    const statusEl = document.getElementById('model-status');
    if (statusEl) {
        const show = s.supervisedLoaded === false;
        statusEl.textContent = show ? 'Supervised model not loaded — run: pio run -t uploadfs' : '';
        statusEl.style.display = show ? 'block' : 'none';
    }

    [1, 2, 5].forEach(spd => {
        const btn = document.getElementById('spd-' + spd);
        if (btn) btn.classList.toggle('active', s.simSpeed === spd);
    });

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

async function setMode(mode) {
    await fetch('/api/ml', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({mode}),
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

async function setSpeed(speed) {
    await fetch('/api/speed', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({speed}),
    });
}

// ── Screen Wake Lock ──────────────────────────────────────────────────────────

let wakeLock = null;

async function acquireWakeLock() {
    if (!('wakeLock' in navigator)) return;
    try { wakeLock = await navigator.wakeLock.request('screen'); } catch (_) {}
}

document.addEventListener('visibilitychange', () => {
    if (document.visibilityState === 'visible') acquireWakeLock();
});

acquireWakeLock();

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
