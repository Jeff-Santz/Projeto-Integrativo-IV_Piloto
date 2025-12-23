import requests
import time
import random
from datetime import datetime

# URL do seu servidor Flask (local)
URL = "http://127.0.0.1:5000/data"

# Placas falsas
PLACAS = [
    "fe80::a1b2:c3d4:e5f6:1111",
    "fe80::a1b2:c3d4:e5f6:2222",
    "fe80::a1b2:c3d4:e5f6:3333",
    "fe80::cafe:babe:0000:beef",
    "2001:db8::f00d"
]

print("🤖 Iniciando simulação de enxame de placas (Modo Suave)...")

while True:
    for endereco in PLACAS:
        # Gera dados
        payload = {
            "e": endereco,
            "d": datetime.now().isoformat(),
            "t": round(random.uniform(20.0, 35.0), 2),
            "uA": round(random.uniform(40.0, 80.0), 1),
            "uS": round(random.uniform(30.0, 60.0), 1),
            "p": int(random.uniform(0, 60))
        }

        try:
            # timeout=5 diz: "Se o servidor não responder em 5s, desista e lance erro"
            requests.post(URL, json=payload, timeout=5)
            print(f"✅ Enviado: {endereco[-4:]} | Temp: {payload['t']}")
            
        except requests.exceptions.ConnectionError:
            print(f"⚠️ Servidor ocupado ou offline. Pulando {endereco[-4:]}...")
        except Exception as e:
            print(f"❌ Erro genérico em {endereco[-4:]}: {e}")

        # O PULO DO GATO: Espera 1s ENTRE cada envio de placa
        # Isso dá tempo do Flask enviar pro Google Sheets e voltar
        time.sleep(2) 

    print("💤 Ciclo completo. Aguardando 5 segundos...")
    time.sleep(5)