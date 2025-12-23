import time
import json
import requests
import threading
from threading import Lock
from flask import Flask, request, jsonify, render_template_string, redirect, url_for

# =========================
# CONFIGURAÇÃO GERAL
# =========================

SHEETS_URL = "https://script.google.com/macros/s/AKfycbwlaZfPu01uY7I_ZSfksZ2lxdQm8T74jQ0P-e-f5gsTHLdkXsDIZHG5y_hhlKaGOzZI/exec"
MAX_HISTORY = 2000     
MAX_POINTS = 80        
FIRE_THRESHOLD = 31.0

app = Flask(__name__)

devices = {}
_lock = Lock()

# =========================
# TEMPLATE: HOME (Lista de Módulos)
# =========================
HOME_HTML = """
<!doctype html>
<html lang="pt-BR">
<head>
    <meta charset="utf-8"/>
    <title>EggLink | Monitoramento Climático</title>
    <meta name="viewport" content="width=device-width,initial-scale=1"/>
    <style>
        body { font-family: 'Segoe UI', Inter, sans-serif; background:#f4f7f6; margin:0; padding:20px; color:#333; transition: background 0.5s; }
        
        /* Estilos de Alarme */
        body.alarm-active { background: #5a1a1a; }
        .alarm-banner { 
            display:none; background:#c0392b; color:white; text-align:center; 
            padding:15px; font-weight:bold; font-size:1.2em; border-radius:8px; margin-bottom:20px;
            animation: blink 1s infinite;
        }
        
        header.main-header { text-align:center; margin-bottom:40px; padding-bottom: 20px; border-bottom: 2px solid #e0e0e0; }
        h1 { margin:0; font-size: 28px; color: #2c3e50; }
        .subtitle { color: #7f8c8d; font-size: 16px; margin-top: 5px; }

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
            border-radius: 16px;
            box-shadow: 0 4px 15px rgba(0,0,0,0.05);
            transition: transform 0.2s;
            cursor: pointer;
            text-decoration: none;
            color: inherit;
            border-left: 6px solid #bdc3c7;
        }
        .card:hover { transform: translateY(-5px); box-shadow: 0 8px 20px rgba(0,0,0,0.1); }
        .card.online { border-left-color: #27ae60; }
        
        .card.fire {
            border-left-color: #c0392b;
            background-color: #fadbd8;
            border: 2px solid #c0392b;
            animation: shake 0.5s infinite;
        }

        .header-card { display:flex; justify-content:space-between; align-items:center; margin-bottom:15px; }
        .title-card { font-size: 1.2em; font-weight: 700; color: #2c3e50; }
        
        .status { font-size: 0.75em; padding: 4px 10px; border-radius: 20px; background:#eee; font-weight: bold; text-transform: uppercase;}
        .online .status { background: #eafaf1; color: #27ae60; border: 1px solid #27ae60; }
        .fire .status { background: #c0392b; color: white; border: 1px solid #922b21; }
        
        .info { font-size: 0.95em; color: #555; line-height: 1.8; }
        .val { font-weight: bold; color: #2c3e50; }
        .addr { font-family: monospace; font-size: 0.7em; color: #aaa; margin-top:15px; text-align:right;}

        footer { margin-top: 50px; text-align: center; font-size: 12px; color: #95a5a6; border-top: 1px solid #e0e0e0; padding-top: 20px; }
        .btn-refresh { background-color: #34495e; color: white; border: none; padding: 10px 20px; border-radius: 8px; cursor: pointer; font-size: 14px; }
        .btn-refresh:hover { background-color: #2c3e50; }

        @keyframes blink { 50% { opacity: 0.5; } }
        @keyframes shake { 0% { transform: translate(1px, 1px) rotate(0deg); } 10% { transform: translate(-1px, -2px) rotate(-1deg); } 20% { transform: translate(-3px, 0px) rotate(1deg); } 30% { transform: translate(3px, 2px) rotate(0deg); } 40% { transform: translate(1px, -1px) rotate(1deg); } 50% { transform: translate(-1px, 2px) rotate(-1deg); } 60% { transform: translate(-3px, 1px) rotate(0deg); } 70% { transform: translate(3px, 1px) rotate(-1deg); } 80% { transform: translate(-1px, -1px) rotate(1deg); } 90% { transform: translate(1px, 2px) rotate(0deg); } 100% { transform: translate(1px, -2px) rotate(-1deg); } }
    </style>
</head>
<body>
    <audio id="siren" loop>
        <source src="https://www.soundjay.com/mechanical/sounds/smoke-detector-1.mp3" type="audio/mpeg">
    </audio>

    <div id="fire-alert" class="alarm-banner">
        🔥 ALERTA DE INCÊNDIO: Níveis críticos detectados! 🔥<br>
        <span id="fire-locations" style="font-size:0.8em"></span>
    </div>

    <header class="main-header">
        <h1 id="page-title">🥚 EggLink Dashboard</h1>
        <div class="subtitle">Sistema de Monitoramento Ambiental - Grupo CM-Piloto</div>
    </header>
    
    {% if not devices_list %}
        <div style="text-align:center; color:#7f8c8d; margin-top:50px; padding: 40px; background: white; border-radius: 12px; max-width: 600px; margin-left: auto; margin-right: auto;">
            <h3>Aguardando conexão dos módulos...</h3>
            <p>Nenhum dispositivo detectado .</p>
            <p><small>Atualize a página após ligar as placas.</small></p>
        </div>
    {% else %}
        <div class="grid">
        {% for dev in devices_list %}
            <a href="/board/{{ dev.uid }}" class="card {{ 'fire' if dev.is_fire else ('online' if dev.is_online else '') }}">
                <div class="header-card">
                    <span class="title-card">Módulo #{{ dev.short_id }}</span>
                    <span class="status">
                        {% if dev.is_fire %}🔥 FOGO{% elif dev.is_online %}ATIVO{% else %}OFFLINE{% endif %}
                    </span>
                </div>
                
                <div class="info">
                    <div>🌡️ Temp: <span class="val">{{ dev.last_data.t }} °C</span></div>
                    <div>💧 Umid. Ar: <span class="val">{{ dev.last_data.uA }} %</span></div>
                    <div>🌱 Umid. Solo: <span class="val">{{ dev.last_data.uS }} %</span></div>
                    <div style="{{ 'color:red;font-weight:900;' if dev.is_fire else '' }}">
                        🌫️ Partículas: <span class="val">{{ dev.last_data.p }} ppm</span>
                    </div>
                </div>

                <div class="addr">IPv6: ...{{ dev.short_id }}</div>
            </a>
        {% endfor %}
        </div>
    {% endif %}
    
    <div style="text-align:center; margin-top:40px;">
        <button onclick="location.reload()" class="btn-refresh">Atualizar Lista</button>
    </div>

    <footer>
        <b>EggLink_v2</b> | Projeto Integrativo IV<br>
        Percurso Competências da Elétrica 2024 - Grupo CM
    </footer>

    <script>
        document.addEventListener("DOMContentLoaded", function() {
            const fireCards = document.querySelectorAll('.card.fire');
            const siren = document.getElementById('siren');
            const alertBanner = document.getElementById('fire-alert');
            const locSpan = document.getElementById('fire-locations');
            const title = document.getElementById('page-title');

            if (fireCards.length > 0) {
                document.body.classList.add('alarm-active');
                alertBanner.style.display = 'block';
                title.style.color = 'white';
                
                let names = [];
                fireCards.forEach(card => {
                    const name = card.querySelector('.title-card').innerText;
                    names.push(name);
                });
                locSpan.innerText = "Local: " + names.join(", ");

                try {
                    siren.volume = 0.5;
                    siren.play().catch(e => console.log("Clique na página para ativar o som"));
                } catch(e){}
            }
            
            // REMOVIDO: O setTimeout de refresh automático foi retirado.
            // Agora só atualiza se clicar no botão.
        });
    </script>
</body>
</html>
"""

# =========================
# TEMPLATE: DASHBOARD INDIVIDUAL
# =========================
DASH_HTML = """
<!doctype html>
<html lang="pt-BR">
<head>
    <meta charset="utf-8"/>
    <title>EggLink | Módulo {{ short_id }}</title>
    <meta name="viewport" content="width=device-width,initial-scale=1"/>
    <style>
        body { font-family: 'Segoe UI', Inter, sans-serif; margin: 20px; background:#f4f7f6; color:#333; transition: background 0.5s; }
        
        body.fire-mode { background-color: #e74c3c !important; }
        body.fire-mode h1, body.fire-mode h3, body.fire-mode small, body.fire-mode th { color: white !important; }
        body.fire-mode .card { background: #fadbd8; border: 2px solid darkred; }
        
        .fire-overlay {
            display:none; position:fixed; top:0; left:0; width:100%; height:100%;
            background: rgba(255,0,0,0.3); z-index:999; pointer-events:none;
            animation: pulse-bg 1s infinite;
        }

        header { display:flex; align-items:center; gap:16px; margin-bottom:20px; padding-bottom:15px; border-bottom:1px solid #ddd; }
        
        .back-btn { 
            text-decoration:none; font-weight:600; color:#34495e; 
            background: #fff; border:1px solid #ccc; padding:8px 16px; 
            border-radius:8px; transition:0.2s;
        }
        .back-btn:hover { background:#eee; }
        
        h1 { font-size: 22px; color: #2c3e50; margin: 0; }
        .toolbar { margin-left:auto; display:flex; gap:8px; }
        
        .row { display:flex; gap:20px; margin-top:20px; flex-wrap:wrap; }
        .card { background:white; border-radius:12px; box-shadow:0 2px 10px rgba(0,0,0,0.05); padding:20px; flex:1; min-width:300px; }
        h3 { margin-top:0; color: #7f8c8d; font-size: 16px; text-transform: uppercase; letter-spacing: 1px; border-bottom: 2px solid #f0f0f0; padding-bottom: 10px; display: inline-block;}
        
        .gauge-container { display:flex; gap:15px; align-items:center; justify-content:space-around; flex-wrap:wrap; margin-top: 15px; }
        .gauge { width:200px; text-align:center; position: relative; }
        
        table { width:100%; border-collapse:collapse; margin-top:15px; }
        th { text-align:left; color:#7f8c8d; font-size:12px; padding: 8px; border-bottom: 2px solid #eee; }
        td { padding:10px 8px; border-bottom:1px solid #f9f9f9; font-size:14px; color: #2c3e50; }
        
        .btn-update { padding:8px 16px; border-radius:8px; border:none; background:#27ae60; color:white; cursor:pointer; font-weight: bold; }
        .value-display { font-size: 24px; font-weight: bold; color: #2c3e50; margin-top: -10px; display: block;}
        .unit { font-size: 14px; color: #95a5a6; font-weight: normal; }
        footer { margin-top: 40px; text-align: center; color: #bdc3c7; font-size: 11px; }

        @keyframes pulse-bg { 0% { box-shadow: inset 0 0 0 0px red; } 50% { box-shadow: inset 0 0 50px 20px red; } 100% { box-shadow: inset 0 0 0 0px red; } }
    </style>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>

<div id="fireOverlay" class="fire-overlay"></div>
<audio id="siren" loop>
    <source src="https://www.soundjay.com/mechanical/sounds/smoke-detector-1.mp3" type="audio/mpeg">
</audio>

<header>
    <a href="/" class="back-btn">← Voltar</a>
    <div>
        <h1 id="headerTitle">Módulo EggLink #{{ short_id }}</h1>
        <small id="statusText" style="color:#7f8c8d">Status: Aguardando Sincronização</small>
    </div>
    <div class="toolbar">
        <button onclick="loadNow()" class="btn-update">Atualizar Dados</button>
    </div>
</header>

<div class="row">
    <div class="card" style="flex:2">
        <h3>Condições Atuais</h3>
        <div class="gauge-container">
            <div class="gauge">
                <canvas id="gaugeTemp"></canvas>
                <div class="value-display"><span id="vT">-</span> <span class="unit">°C</span></div>
                <small>Temperatura</small>
            </div>
            <div class="gauge">
                <canvas id="gaugeUA"></canvas>
                <div class="value-display"><span id="vUA">-</span> <span class="unit">%</span></div>
                <small>Umid. Ar</small>
            </div>
            <div class="gauge">
                <canvas id="gaugeUS"></canvas>
                <div class="value-display"><span id="vUS">-</span> <span class="unit">%</span></div>
                <small>Umid. Solo</small>
            </div>
            <div class="gauge">
                <canvas id="gaugeP"></canvas>
                <div class="value-display"><span id="vP">-</span> <span class="unit">ppm</span></div>
                <small>Partículas</small>
            </div>
        </div>
    </div>
</div>

<div class="row">
    <div class="card">
        <h3>Variação Temporal</h3>
        <div style="display:flex;gap:20px;flex-wrap:wrap;">
            <div style="flex:1;min-width:250px">
                <h4 style="margin:0 0 10px 0; font-size:14px; color:#666;">Temperatura</h4>
                <canvas id="chartTemp" height="120"></canvas>
            </div>
            <div style="flex:1;min-width:250px">
                <h4 style="margin:0 0 10px 0; font-size:14px; color:#666;">Umidade do Ar</h4>
                <canvas id="chartUA" height="120"></canvas>
            </div>
        </div>
        
        <h3 style="margin-top:30px">Log de Eventos</h3>
        <table id="tbl">
            <thead>
                <tr>
                    <th>HORÁRIO</th>
                    <th>DETALHES</th>
                </tr>
            </thead>
            <tbody></tbody>
        </table>
    </div>
</div>

<footer>EggLink_v1 System • Grupo CM-Piloto</footer>

<script>
const UID = "{{ uid }}";
const MAX_POINTS = {{ max_points }};
let charts = {};
const FIRE_LIMIT = 30.0;

function drawGauge(id, val, min, max, color){
    const ctx = document.getElementById(id).getContext('2d');
    const w = ctx.canvas.width = 200; 
    const h = ctx.canvas.height = 100;
    val = Math.max(min, Math.min(max, (val||min)));
    const perc = (val-min)/(max-min);
    ctx.clearRect(0,0,w,h+40);
    ctx.beginPath(); ctx.lineWidth=12; ctx.strokeStyle='#ecf0f1'; ctx.arc(w/2, h, 80, Math.PI, 0); ctx.stroke();
    ctx.beginPath(); ctx.strokeStyle=color; ctx.arc(w/2, h, 80, Math.PI, Math.PI + (perc*Math.PI)); ctx.stroke();
}

function initChart(id, label, color){
    return new Chart(document.getElementById(id), {
        type:'line',
        data: { labels:[], datasets:[{ label:label, borderColor:color, backgroundColor: color + '10', data:[], tension:0.4, fill: true }] },
        options: { responsive: true, plugins:{legend:{display:false}}, scales:{x:{display:false}, y:{grid:{color:'#f0f0f0'}}} }
    });
}

charts.temp = initChart('chartTemp', 'Temperatura', '#e67e22');
charts.ua   = initChart('chartUA', 'Umidade Ar', '#3498db');

function checkAlarm(p_val) {
    const siren = document.getElementById('siren');
    const overlay = document.getElementById('fireOverlay');
    const title = document.getElementById('headerTitle');
    const status = document.getElementById('statusText');

    if (p_val > FIRE_LIMIT) {
        document.body.classList.add('fire-mode');
        overlay.style.display = 'block';
        title.innerText = "🔥 INCÊNDIO DETECTADO 🔥";
        status.innerText = "Nível de partículas crítico: " + p_val + " ppm";
        status.style.color = "white";
        try { siren.play().catch(e=>{}); } catch(e){}
    } else {
        document.body.classList.remove('fire-mode');
        overlay.style.display = 'none';
        title.innerText = "Módulo EggLink #{{ short_id }}";
        status.innerText = "Status: Monitoramento em Tempo Real";
        status.style.color = "#7f8c8d";
        siren.pause();
        siren.currentTime = 0;
    }
}

async function loadNow(){
    const r = await fetch('/api/history?uid=' + encodeURIComponent(UID));
    if(!r.ok) return;
    const arr = await r.json(); 
    if(arr.length === 0) return;

    const last = arr[0].data;
    
    checkAlarm(last.p);

    drawGauge('gaugeTemp', last.t, -10, 50, '#e67e22');
    drawGauge('gaugeUA', last.uA, 0, 100, '#3498db');
    drawGauge('gaugeUS', last.uS, 0, 100, '#27ae60');
    
    const pColor = last.p > FIRE_LIMIT ? '#c0392b' : '#7f8c8d';
    drawGauge('gaugeP', last.p, 0, 100, pColor);

    document.getElementById('vT').innerText = last.t;
    document.getElementById('vUA').innerText = last.uA;
    document.getElementById('vUS').innerText = last.uS;
    document.getElementById('vP').innerText = last.p;

    const tbody = document.querySelector('#tbl tbody');
    tbody.innerHTML = '';
    arr.slice(0, 8).forEach(item => {
        const tr = document.createElement('tr');
        const d = new Date(item.data.d || item.ts*1000);
        const isFire = (item.data.p > FIRE_LIMIT) ? "style='background:#fadbd8; font-weight:bold; color:#c0392b'" : "";
        const fireIcon = (item.data.p > FIRE_LIMIT) ? "🔥 ALERTA: " : "";
        
        tr.innerHTML = `<td ${isFire}>${d.toLocaleTimeString()}</td>` +
                       `<td ${isFire}>${fireIcon}P:${item.data.p}ppm | T:${item.data.t}°C</td>`;
        tbody.appendChild(tr);
    });

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

// REMOVIDO: setInterval do auto-refresh.
</script>
</body>
</html>
"""

# =========================
# FUNÇÕES AUXILIARES (AGORA COM THREADING)
# =========================

def send_to_sheets_thread(data):
    """Envia para o Google Sheets em segundo plano para não travar o servidor"""
    try:
        requests.post(SHEETS_URL, json=data, timeout=5)
    except Exception as e:
        print(f"Erro ao enviar para Sheets (Background): {e}")

# =========================
# ROTAS DO FLASK
# =========================

@app.route("/data", methods=["POST"])
def receive_data():
    try:
        data = request.get_json(force=True)
        raw_e = data.get("e")

        if not raw_e or str(raw_e).strip() == "":
            uid = "desconhecido"
        else:
            uid = str(raw_e)
        
        for k in ("t", "uA", "uS", "p"):
            if k in data:
                try: data[k] = float(data[k])
                except: data[k] = 0.0

        entry = {
            "ts": int(time.time()),
            "data": data
        }

        with _lock:
            if uid not in devices:
                devices[uid] = []
            devices[uid].append(entry)
            if len(devices[uid]) > MAX_HISTORY:
                devices[uid].pop(0)

        # Envia ao Sheets DE FORMA ASSÍNCRONA (THREAD)
        # Isso libera o ESP32 imediatamente e evita o erro de timeout
        threading.Thread(target=send_to_sheets_thread, args=(data,)).start()
        
        print(f"📡 [EggLink] Dados recebidos: {uid[-4:]}")

        return jsonify({"status": "ok"}), 200

    except Exception as e:
        print("Erro no receive:", e)
        return jsonify({"error": str(e)}), 400


@app.route("/")
def home():
    now = time.time()
    devices_view = []

    with _lock:
        for uid, history in devices.items():
            if not history:
                continue
            
            last_entry = history[-1]
            last_ts = last_entry["ts"]
            last_data = last_entry["data"]
            
            is_online = (now - last_ts) < 60
            
            p_val = last_data.get("p", 0)
            is_fire = p_val > FIRE_THRESHOLD
            
            devices_view.append({
                "uid": uid,
                "short_id": uid[-4:].upper(),
                "is_online": is_online,
                "is_fire": is_fire,
                "last_data": last_data
            })

    devices_view.sort(key=lambda x: (not x["is_fire"], not x["is_online"], x["short_id"]))

    return render_template_string(HOME_HTML, devices_list=devices_view)


@app.route("/board/<path:uid>")
def board(uid):
    short_id = uid[-4:].upper()
    return render_template_string(
        DASH_HTML, 
        uid=uid, 
        short_id=short_id,
        max_points=MAX_POINTS
    )


@app.route("/api/history")
def api_history():
    uid = request.args.get("uid")
    with _lock:
        if not uid or uid not in devices:
            return jsonify([])
        hist_copy = list(reversed(devices[uid]))
    return jsonify(hist_copy)


@app.route("/clear", methods=["POST"])
def clear_all():
    with _lock:
        devices.clear()
    return redirect("/")


if __name__ == "__main__":
    print("🚀 EggLink Server (Estável e Manual) iniciado!")
    app.run(host="0.0.0.0", port=5000, debug=True)