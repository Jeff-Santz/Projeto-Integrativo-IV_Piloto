import time
import json
import requests
from threading import Lock
from flask import Flask, request, jsonify, render_template_string, redirect, url_for

# =========================
# Template do dashboard (Chart.js + gauges)
# =========================

DASH_HTML = """
<!doctype html>
<html lang="pt-BR">
<head>
    <meta charset="utf-8"/>
    <title>ESP32 Dashboard - Gauges</title>
    <meta name="viewport" content="width=device-width,initial-scale=1"/>

    <style>
        body {
            font-family: Inter, "Helvetica Neue", Arial, sans-serif;
            margin: 14px;
            background:#f5f7fb;
            color:#222;
        }
        header {
            display:flex;
            align-items:center;
            gap:16px;
        }
        h1 { margin:0; font-size:20px; }
        .toolbar {
            margin-left:auto;
            display:flex;
            gap:8px;
        }
        .row {
            display:flex;
            gap:16px;
            margin-top:12px;
            flex-wrap:wrap;
        }
        .card {
            background:white;
            border-radius:8px;
            box-shadow:0 1px 6px rgba(20,20,20,0.06);
            padding:12px;
            flex:1;
            min-width:260px;
        }
        .gauge-container {
            display:flex;
            gap:12px;
            align-items:center;
            justify-content:space-around;
            flex-wrap:wrap;
        }
        .gauge { width:220px; text-align:center; }
        .gauge canvas {
            display:block;
            margin:0 auto;
        }
        .label { font-weight:600; margin-top:6px }
        .small { color:#666; font-size:13px }
        .series { margin-top:10px; }

        table {
            width:100%;
            border-collapse:collapse;
            margin-top:10px;
        }
        th, td {
            padding:8px 6px;
            text-align:left;
            border-bottom:1px solid #eee;
            font-size:13px;
        }

        pre {
            background:#f7f9fc;
            padding:8px;
            border-radius:6px;
            overflow:auto;
            max-height:120px;
        }

        footer {
            margin-top:18px;
            font-size:13px;
            color:#666;
        }

        button {
            padding:6px 10px;
            border-radius:6px;
            border:none;
            background:#1976d2;
            color:white;
            cursor:pointer;
        }
        button.ghost {
            background:#eee;
            color:#222;
        }
    </style>

    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>

<header>
    <h1>ESP32 — Gauges & Séries</h1>
    <div class="toolbar">
        <button onclick="loadNow()">Atualizar</button>

        <form method="POST" action="/clear" style="display:inline">
            <button type="submit" class="ghost">Limpar histórico</button>
        </form>
    </div>
</header>

<div class="row">
    <div class="card" style="flex:2">
        <h3>Gauges (ponteiro) — leituras mais recentes</h3>

        <div class="gauge-container">
            <div class="gauge">
                <canvas id="gaugeTemp" width="220" height="140"></canvas>
                <div class="label">Temperatura <span class="small">(°C)</span></div>
                <div id="gValTemp" class="small">—</div>
            </div>

            <div class="gauge">
                <canvas id="gaugeUA" width="220" height="140"></canvas>
                <div class="label">Umidade do Ar <span class="small">(%)</span></div>
                <div id="gValUA" class="small">—</div>
            </div>

            <div class="gauge">
                <canvas id="gaugeUS" width="220" height="140"></canvas>
                <div class="label">Umidade do Solo <span class="small">(%)</span></div>
                <div id="gValUS" class="small">—</div>
            </div>

            <div class="gauge">
                <canvas id="gaugeP" width="220" height="140"></canvas>
                <div class="label">Partículas <span class="small">(uP)</span></div>
                <div id="gValP" class="small">—</div>
            </div>
        </div>

        <p class="small">Gauges analógicos com ponteiro + valores numéricos.</p>
    </div>

    <div class="card" style="flex:1; min-width:260px">
        <h3>Última leitura</h3>
        <pre id="last">Nenhuma leitura</pre>

        <p class="small">
            Endpoint: <code>/data</code> envia JSON com chaves <code>e,d,t,uA,uS,p</code>
        </p>
    </div>
</div>

<div class="row">
    <div class="card" style="width:100%">
        <h3>Séries temporais (últimas {{max_points}} leituras)</h3>

        <div style="display:flex;gap:12px;flex-wrap:wrap;">
            <div style="flex:1;min-width:220px" class="series">
                <canvas id="chartTemp" height="80"></canvas>
            </div>

            <div style="flex:1;min-width:220px" class="series">
                <canvas id="chartUA" height="80"></canvas>
            </div>

            <div style="flex:1;min-width:220px" class="series">
                <canvas id="chartUS" height="80"></canvas>
            </div>

            <div style="flex:1;min-width:220px" class="series">
                <canvas id="chartP" height="80"></canvas>
            </div>
        </div>

        <table id="tbl">
            <thead>
                <tr>
                    <th>#</th>
                    <th>Timestamp</th>
                    <th>Endereco (e)</th>
                    <th>Dados (t/uA/uS/p)</th>
                </tr>
            </thead>
            <tbody></tbody>
        </table>
    </div>
</div>

<footer>
    Última atualização: <span id="updated-at">—</span>
</footer>

<script>

const MAX_POINTS = {{max_points}};

/* ---------- Helpers ---------- */

function fmtTSISO(d) {
    try {
        const dt = new Date(d);
        return dt.toLocaleString();
    } catch(e){
        return d;
    }
}

function fmtTSunix(ts) {
    const d = new Date(ts*1000);
    return d.toLocaleString();
}

/* ---------- G A U G E S ---------- */

function drawGauge(canvasId, value, min, max, units, color) {
    const c = document.getElementById(canvasId);
    if (!c) return;

    const ctx = c.getContext('2d');
    const w = c.width, h = c.height;

    ctx.clearRect(0,0,w,h);

    const cx = w/2;
    const cy = h*0.85;
    const radius = Math.min(w*0.38, h*0.6);

    const startAngle = Math.PI;
    const endAngle   = 0;

    const clamped = (value === null || value === undefined)
        ? min
        : Math.max(min, Math.min(max, value));

    const frac  = (clamped - min) / (max - min);
    const angle = startAngle + (endAngle - startAngle) * frac;

    /* semicircle background */
    ctx.lineWidth = 12;
    ctx.strokeStyle = '#eee';
    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, endAngle);
    ctx.stroke();

    /* arc filled */
    ctx.strokeStyle = color || '#1976d2';
    ctx.beginPath();
    ctx.arc(cx, cy, radius, startAngle, angle);
    ctx.stroke();

    /* ticks */
    ctx.strokeStyle = '#bbb';
    const ticks = 10;
    for (let i=0;i<=ticks;i++){
        const a = startAngle + (endAngle-startAngle)*(i/ticks);
        const x1 = cx + Math.cos(a)*(radius+8);
        const y1 = cy + Math.sin(a)*(radius+8);
        const x2 = cx + Math.cos(a)*(radius-6);
        const y2 = cy + Math.sin(a)*(radius-6);

        ctx.beginPath();
        ctx.moveTo(x1,y1);
        ctx.lineTo(x2,y2);
        ctx.stroke();
    }

    /* needle */
    ctx.save();
    ctx.translate(cx, cy);
    ctx.rotate(angle - Math.PI);

    ctx.fillStyle = '#333';
    ctx.beginPath();
    ctx.moveTo(0,-6);
    ctx.lineTo(radius*0.9, 0);
    ctx.lineTo(0,6);
    ctx.closePath();
    ctx.fill();

    ctx.restore();

    /* center circle */
    ctx.fillStyle = '#fff';
    ctx.beginPath();
    ctx.arc(cx, cy, 8, 0, 2*Math.PI);
    ctx.fill();
    ctx.strokeStyle = '#ccc';
    ctx.stroke();

    /* text */
    ctx.fillStyle = '#222';
    ctx.font = '14px sans-serif';
    ctx.textAlign = 'center';
    ctx.fillText(
        (value===null?'—': value.toFixed(2)) + ' ' + (units||''),
        cx,
        cy - radius - 8
    );
}

/* ---------- C H A R T S ---------- */

let charts = {};

function buildLineChart(canvasId, label) {
    const ctx = document.getElementById(canvasId).getContext('2d');

    return new Chart(ctx, {
        type:'line',
        data:{
            labels:[],
            datasets:[{
                label: label,
                data:[],
                fill:false,
                tension:0.2,
                pointRadius:0
            }]
        },
        options:{
            animation:false,
            plugins:{ legend:{display:false} },
            scales:{
                x:{ display:false },
                y:{ beginAtZero:true }
            }
        }
    });
}

charts.temp = buildLineChart('chartTemp','Temp (°C)');
charts.ua   = buildLineChart('chartUA','Umid Ar (%)');
charts.us   = buildLineChart('chartUS','Umid Solo (%)');
charts.p    = buildLineChart('chartP','Particulas');


/* ---------- Atualização via API ---------- */

async function loadNow(){
    try {
        const r = await fetch('/api/history');
        if (!r.ok) {
            console.error('history fetch failed');
            return;
        }

        const arr = await r.json(); // newest first
        const tbody = document.querySelector('#tbl tbody');

        tbody.innerHTML = '';

        /* Table rows */
        for (let i=0;i<Math.min(arr.length,200);i++){
            const item = arr[i];

            const tr = document.createElement('tr');

            const idx = document.createElement('td');
            idx.textContent = i+1;

            const ts = document.createElement('td');
            ts.textContent =
                (item.data && item.data.d)
                    ? fmtTSISO(item.data.d)
                    : fmtTSunix(item.ts);

            const e = document.createElement('td');
            e.textContent = (item.data && item.data.e) ? item.data.e : '-';

            const dat = document.createElement('td');

            const t  = item.data?.t  ?? '-';
            const uA = item.data?.uA ?? '-';
            const uS = item.data?.uS ?? '-';
            const p  = item.data?.p  ?? '-';

            // ATENÇÃO: Corrigido para usar crases (template literals)
            dat.textContent = `t:${t} uA:${uA} uS:${uS} p:${p}`;

            tr.appendChild(idx);
            tr.appendChild(ts);
            tr.appendChild(e);
            tr.appendChild(dat);

            tbody.appendChild(tr);
        }

        /* Charts */

        const recent = arr.slice(0, MAX_POINTS).reverse();

        const labels = recent.map(it =>
            (it.data && it.data.d)
                ? new Date(it.data.d).toLocaleTimeString()
                : new Date(it.ts*1000).toLocaleTimeString()
        );

        function arrFor(key){
            return recent.map(it =>
                it.data && (key in it.data)
                    ? Number(it.data[key])
                    : null
            );
        }

        function updateChart(chartObj, labels, values){
            chartObj.data.labels = labels;
            chartObj.data.datasets[0].data = values;
            chartObj.update('none');
        }

        updateChart(charts.temp, labels, arrFor('t'));
        updateChart(charts.ua,   labels, arrFor('uA'));
        updateChart(charts.us,   labels, arrFor('uS'));
        updateChart(charts.p,    labels, arrFor('p'));

        /* Gauges + Última leitura */

        if (arr.length > 0){
            const last = arr[0].data;

            const t  = last.t  !== undefined ? Number(last.t)  : null;
            const ua = last.uA !== undefined ? Number(last.uA) : null;
            const us = last.uS !== undefined ? Number(last.uS) : null;
            const p  = last.p  !== undefined ? Number(last.p)  : null;

            drawGauge('gaugeTemp', t,  -10, 60,  '°C', '#d9480f');
            document.getElementById('gValTemp').textContent = t===null?'—':(t + ' °C');

            drawGauge('gaugeUA',   ua, 0, 100, '%', '#0b74de');
            document.getElementById('gValUA').textContent = ua===null?'—':(ua + ' %');

            drawGauge('gaugeUS',   us, 0, 100, '%', '#16a34a');
            document.getElementById('gValUS').textContent = us===null?'—':(us + ' %');

            drawGauge('gaugeP',    p, 0, 1000, 'uP', '#b45309');
            document.getElementById('gValP').textContent = p===null?'—':(p + ' uP');

            document.getElementById('last').textContent =
                JSON.stringify(last, null, 2)
                + "\\n\\n"
                + (last.d ? fmtTSISO(last.d) : fmtTSunix(arr[0].ts));
        }
        else {
            document.getElementById('last').textContent = 'Nenhuma leitura';
        }

        document.getElementById('updated-at').textContent =
            new Date().toLocaleString();

    } catch (e){
        console.error('loadNow error', e);
    }
}

loadNow();
setInterval(loadNow, {{poll_interval}});

</script>
</body>
</html>
"""

app = Flask(__name__)

# ============ CONFIGURAÇÃO ============

SHEETS_URL = "https://script.google.com/macros/s/AKfycbwyW_O432vCCMVnx9Fa4_mCStBr1pz-GQaG9QnQkRFjt_ndbHmizPaw6IyGgVFz6MHO/exec"

MAX_HISTORY = 2000
MAX_POINTS = 80
POLL_INTERVAL_MS = 2500

history = []
_lock = Lock()

# ======================================
# FUNÇÃO PARA ENVIAR AO SHEETS
# ======================================
def send_to_sheets(data):
    try:
        r = requests.post(SHEETS_URL, json=data, timeout=5)
        print("📤 Enviado ao Sheets:", r.text)
    except Exception as e:
        print("❌ Erro ao enviar ao Sheets:", e)

# ======================================
# ROTA PRINCIPAL /data (única!)
# ======================================
@app.route("/data", methods=["POST"])
def receive_data():
    try:
        data = request.get_json(force=True)
    except Exception as e:
        return jsonify({"error": "JSON inválido", "msg": str(e)}), 400

    print("📦 ESP32 enviou:", data)

    # normaliza valores numéricos
    for k in ("t", "uA", "uS", "p"):
        if k in data:
            try:
                data[k] = float(data[k])
            except:
                pass

    # salva no histórico do dashboard
    entry = {"ts": int(time.time()), "data": data}
    with _lock:
        history.append(entry)
        if len(history) > MAX_HISTORY:
            history.pop(0)

    # repassa ao Sheets
    send_to_sheets(data)

    return jsonify({"status": "ok"}), 200

# ======================================
# ROTAS DO DASHBOARD
# ======================================

@app.route("/")
def dashboard():
    return render_template_string(
        DASH_HTML,
        max_points=MAX_POINTS,
        poll_interval=POLL_INTERVAL_MS
    )

@app.route("/latest")
def latest():
    with _lock:
        if not history:
            return jsonify({}), 204
        return jsonify(history[-1])

@app.route("/api/history")
def api_history():
    with _lock:
        return jsonify(list(reversed(history)))

@app.route("/clear", methods=["POST"])
def clear_history():
    with _lock:
        history.clear()
    return redirect(url_for("dashboard"))

# ======================================
# INÍCIO DO SERVIDOR
# ======================================
if __name__ == "__main__":
    print("Servidor iniciado em http://localhost:5000")
    print("Endpoints: / (dashboard), /data (POST), /latest, /api/history")
    app.run(host="0.0.0.0", port=5000, debug=True)