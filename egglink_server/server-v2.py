import time
import json
import requests
from threading import Lock
from flask import Flask, request, jsonify, render_template_string, redirect, url_for

# =========================
# CONFIGURAÇÃO GERAL
# =========================

SHEETS_URL = "https://script.google.com/macros/s/AKfycbwlaZfPu01uY7I_ZSfksZ2lxdQm8T74jQ0P-e-f5gsTHLdkXsDIZHG5y_hhlKaGOzZI/exec"
MAX_HISTORY = 2000     # Histórico por placa
MAX_POINTS = 80        # Pontos no gráfico
POLL_INTERVAL_MS = 3000

app = Flask(__name__)

# Estrutura:
# devices = {
#    "fe80::1234...": [ {ts:..., data:...}, ... ],
#    "fe80::abcd...": [ ... ]
# }
devices = {}
_lock = Lock()

# =========================
# TEMPLATE: HOME (Lista de Placas)
# =========================
HOME_HTML = """
<!doctype html>
<html lang="pt-BR">
<head>
    <meta charset="utf-8"/>
    <title>Central ESP32</title>
    <meta name="viewport" content="width=device-width,initial-scale=1"/>
    <style>
        body { font-family: Inter, sans-serif; background:#f0f2f5; margin:0; padding:20px; color:#333; }
        h1 { text-align:center; margin-bottom:30px; }
        .grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
            gap: 20px;
            max-width: 1200px;
            margin: 0 auto;
        }
        .card {
            background: white;
            padding: 20px;
            border-radius: 12px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.08);
            transition: transform 0.2s;
            cursor: pointer;
            text-decoration: none;
            color: inherit;
            border-left: 5px solid #ccc;
        }
        .card:hover { transform: translateY(-3px); box-shadow: 0 4px 12px rgba(0,0,0,0.15); }
        .card.online { border-left-color: #16a34a; } /* Verde se recebeu dados recentemente */
        
        .header { display:flex; justify-content:space-between; align-items:center; margin-bottom:10px; }
        .title { font-size: 1.1em; font-weight: bold; }
        .status { font-size: 0.8em; padding: 2px 8px; border-radius: 10px; background:#eee; }
        .online .status { background: #dcfce7; color: #166534; }
        
        .info { font-size: 0.9em; color: #666; line-height: 1.6; }
        .val { font-weight: bold; color: #000; }
        .addr { font-family: monospace; font-size: 0.75em; color: #999; word-break: break-all; margin-top:10px;}
    </style>
</head>
<body>
    <h1>📡 Monitoramento de Placas</h1>
    
    {% if not devices_list %}
        <div style="text-align:center; color:#666; margin-top:50px;">
            <h3>Nenhuma placa detectada ainda.</h3>
            <p>Aguardando envio de dados...</p>
        </div>
    {% else %}
        <div class="grid">
        {% for dev in devices_list %}
            <a href="/board/{{ dev.uid }}" class="card {{ 'online' if dev.is_online else '' }}">
                <div class="header">
                    <span class="title">Placa #{{ dev.short_id }}</span>
                    <span class="status">{{ 'ONLINE' if dev.is_online else 'OFFLINE' }}</span>
                </div>
                
                <div class="info">
                    <div>Temp: <span class="val">{{ dev.last_data.t }} °C</span></div>
                    <div>Umid: <span class="val">{{ dev.last_data.uA }} %</span></div>
                    <div>Solo: <span class="val">{{ dev.last_data.uS }} %</span></div>
                </div>

                <div class="addr">{{ dev.uid }}</div>
            </a>
        {% endfor %}
        </div>
    {% endif %}
    
    <div style="text-align:center; margin-top:30px;">
        <button onclick="location.reload()" style="padding:10px 20px; cursor:pointer;">Atualizar Lista</button>
    </div>
</body>
</html>
"""

# =========================
# TEMPLATE: DASHBOARD INDIVIDUAL
# =========================
# (Basicamente o mesmo de antes, mas adaptado para saber qual UID buscar)
DASH_HTML = """
<!doctype html>
<html lang="pt-BR">
<head>
    <meta charset="utf-8"/>
    <title>Dashboard {{ short_id }}</title>
    <meta name="viewport" content="width=device-width,initial-scale=1"/>
    <style>
        body { font-family: Inter, sans-serif; margin: 14px; background:#f5f7fb; color:#222; }
        header { display:flex; align-items:center; gap:16px; margin-bottom:20px; }
        .back-btn { text-decoration:none; font-weight:bold; color:#1976d2; border:1px solid #1976d2; padding:6px 12px; border-radius:6px;}
        .toolbar { margin-left:auto; display:flex; gap:8px; }
        .row { display:flex; gap:16px; margin-top:12px; flex-wrap:wrap; }
        .card { background:white; border-radius:8px; box-shadow:0 1px 6px rgba(0,0,0,0.06); padding:12px; flex:1; min-width:260px; }
        .gauge-container { display:flex; gap:12px; align-items:center; justify-content:space-around; flex-wrap:wrap; }
        .gauge { width:220px; text-align:center; }
        .series { margin-top:10px; }
        table { width:100%; border-collapse:collapse; margin-top:10px; }
        th, td { padding:8px; border-bottom:1px solid #eee; text-align:left; font-size:13px; }
        button { padding:6px 10px; border-radius:6px; border:none; background:#1976d2; color:white; cursor:pointer; }
    </style>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>

<header>
    <a href="/" class="back-btn">← Voltar</a>
    <h1>Placa #{{ short_id }}</h1>
    <div class="toolbar">
        <button onclick="loadNow()">Atualizar</button>
    </div>
</header>

<div class="row">
    <div class="card" style="flex:2">
        <h3>Tempo Real</h3>
        <div class="gauge-container">
            <div class="gauge"><canvas id="gaugeTemp"></canvas><br><b>Temp (°C)</b> <span id="vT">-</span></div>
            <div class="gauge"><canvas id="gaugeUA"></canvas><br><b>Ar (%)</b> <span id="vUA">-</span></div>
            <div class="gauge"><canvas id="gaugeUS"></canvas><br><b>Solo (%)</b> <span id="vUS">-</span></div>
            <div class="gauge"><canvas id="gaugeP"></canvas><br><b>Partic.</b> <span id="vP">-</span></div>
        </div>
    </div>
</div>

<div class="row">
    <div class="card">
        <h3>Histórico</h3>
        <div style="display:flex;gap:10px;flex-wrap:wrap;">
            <div style="flex:1;min-width:200px"><canvas id="chartTemp" height="100"></canvas></div>
            <div style="flex:1;min-width:200px"><canvas id="chartUA" height="100"></canvas></div>
        </div>
        <table id="tbl"><thead><tr><th>Hora</th><th>Dados</th></tr></thead><tbody></tbody></table>
    </div>
</div>

<script>
const UID = "{{ uid }}";
const MAX_POINTS = {{ max_points }};
let charts = {};

/* Função de Gauge Simplificada para brevidade */
function drawGauge(id, val, min, max, color){
    const ctx = document.getElementById(id).getContext('2d');
    const w = ctx.canvas.width = 200; 
    const h = ctx.canvas.height = 120;
    
    val = Math.max(min, Math.min(max, (val||min)));
    const perc = (val-min)/(max-min);
    
    ctx.clearRect(0,0,w,h);
    ctx.beginPath(); ctx.lineWidth=15; ctx.strokeStyle='#eee';
    ctx.arc(w/2, h, 80, Math.PI, 0); ctx.stroke();
    
    ctx.beginPath(); ctx.strokeStyle=color;
    ctx.arc(w/2, h, 80, Math.PI, Math.PI + (perc*Math.PI)); ctx.stroke();
}

function initChart(id, label, color){
    return new Chart(document.getElementById(id), {
        type:'line',
        data: { labels:[], datasets:[{ label:label, borderColor:color, data:[], tension:0.3 }] },
        options: { plugins:{legend:{display:false}}, scales:{x:{display:false}} }
    });
}

charts.temp = initChart('chartTemp', 'Temp', '#d9480f');
charts.ua   = initChart('chartUA', 'Umid Ar', '#0b74de');

async function loadNow(){
    // AQUI ESTÁ O TRUQUE: Passamos o UID na URL
    const r = await fetch('/api/history?uid=' + encodeURIComponent(UID));
    if(!r.ok) return;
    
    const arr = await r.json(); // Array reverso (mais novo primeiro)
    if(arr.length === 0) return;

    // Atualiza Gauges (pega o item 0 que é o mais novo)
    const last = arr[0].data;
    drawGauge('gaugeTemp', last.t, -10, 60, '#d9480f');
    drawGauge('gaugeUA', last.uA, 0, 100, '#0b74de');
    drawGauge('gaugeUS', last.uS, 0, 100, '#16a34a');
    drawGauge('gaugeP', last.p, 0, 1000, '#b45309');
    
    document.getElementById('vT').innerText = last.t;
    document.getElementById('vUA').innerText = last.uA;
    document.getElementById('vUS').innerText = last.uS;
    document.getElementById('vP').innerText = last.p;

    // Atualiza Tabela
    const tbody = document.querySelector('#tbl tbody');
    tbody.innerHTML = '';
    arr.slice(0, 10).forEach(item => {
        const tr = document.createElement('tr');
        const d = new Date(item.data.d || item.ts*1000);
        tr.innerHTML = `<td>${d.toLocaleTimeString()}</td><td>T:${item.data.t} / A:${item.data.uA}</td>`;
        tbody.appendChild(tr);
    });

    // Atualiza Gráficos (inverte para ordem cronológica)
    const hist = arr.slice(0, MAX_POINTS).reverse();
    const lbls = hist.map(x => new Date(x.data.d || x.ts*1000).toLocaleTimeString());
    
    charts.temp.data.labels = lbls;
    charts.temp.data.datasets[0].data = hist.map(x => x.data.t);
    charts.temp.update();
    
    charts.ua.data.labels = lbls;
    charts.ua.data.datasets[0].data = hist.map(x => x.data.uA);
    charts.ua.update();
}

loadNow();
setInterval(loadNow, {{ poll_interval }});
</script>
</body>
</html>
"""

# =========================
# FUNÇÕES AUXILIARES
# =========================

def send_to_sheets(data):
    # Opcional: Você pode querer enviar o ID da placa junto para o Sheets
    try:
        requests.post(SHEETS_URL, json=data, timeout=3)
    except:
        pass

# =========================
# ROTAS DO FLASK
# =========================

@app.route("/data", methods=["POST"])
def receive_data():
    try:
        data = request.get_json(force=True)
        # Identificador único (IPv6)
        raw_e = data.get("e")

        # Se for None, vazio ou string em branco, define um nome padrão
        if not raw_e or str(raw_e).strip() == "":
            uid = "desconhecido"
        else:
            uid = str(raw_e)
        
        # Limpeza básica dos dados numéricos
        for k in ("t", "uA", "uS", "p"):
            if k in data:
                try: data[k] = float(data[k])
                except: data[k] = 0.0

        entry = {
            "ts": int(time.time()),
            "data": data
        }

        with _lock:
            # Se é a primeira vez que vemos essa placa, cria a lista dela
            if uid not in devices:
                devices[uid] = []
            
            # Adiciona ao histórico daquela placa específica
            devices[uid].append(entry)
            
            # Limita tamanho
            if len(devices[uid]) > MAX_HISTORY:
                devices[uid].pop(0)

        # Envia ao Sheets (assíncrono idealmente, mas aqui síncrono é ok)
        print(f"📡 Dados recebidos de {uid[-4:]}")
        send_to_sheets(data)

        return jsonify({"status": "ok"}), 200

    except Exception as e:
        print("Erro no receive:", e)
        return jsonify({"error": str(e)}), 400


@app.route("/")
def home():
    """Tela Inicial: Lista todas as placas encontradas"""
    now = time.time()
    devices_view = []

    with _lock:
        # Itera sobre o dicionário para criar os dados de exibição
        for uid, history in devices.items():
            if not history:
                continue
            
            last_entry = history[-1]
            last_ts = last_entry["ts"]
            
            # Considera online se mandou dados nos últimos 60 segundos
            is_online = (now - last_ts) < 60
            
            devices_view.append({
                "uid": uid,
                "short_id": uid[-4:].upper(), # Pega os últimos 4 caracteres
                "is_online": is_online,
                "last_data": last_entry["data"]
            })

    # Ordena: Online primeiro, depois por nome curto
    devices_view.sort(key=lambda x: (not x["is_online"], x["short_id"]))

    return render_template_string(HOME_HTML, devices_list=devices_view)


@app.route("/board/<path:uid>")
def board(uid):
    """Dashboard individual de uma placa"""
    # short_id para exibir no título
    short_id = uid[-4:].upper()
    return render_template_string(
        DASH_HTML, 
        uid=uid, 
        short_id=short_id,
        max_points=MAX_POINTS,
        poll_interval=POLL_INTERVAL_MS
    )


@app.route("/api/history")
def api_history():
    """Retorna JSON para o gráfico. Requer parâmetro ?uid=..."""
    uid = request.args.get("uid")
    
    with _lock:
        if not uid or uid not in devices:
            return jsonify([])
        
        # Retorna lista reversa (mais recente primeiro)
        # Cópia da lista para não dar erro de concorrência
        hist_copy = list(reversed(devices[uid]))
        
    return jsonify(hist_copy)


@app.route("/clear", methods=["POST"])
def clear_all():
    with _lock:
        devices.clear()
    return redirect("/")


if __name__ == "__main__":
    print("🚀 Servidor Multi-Placas iniciado!")
    app.run(host="0.0.0.0", port=5000, debug=True)