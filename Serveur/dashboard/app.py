import streamlit as st
import pandas as pd
import plotly.graph_objects as go
import requests

API = "http://api:8000"

st.set_page_config(page_title="metAI Dashboard", page_icon="🌦️", layout="wide")
st.title("🌦️ metAI — Weather Prediction Dashboard")
st.caption("STM32 embedded ML model vs Open-Meteo real weather · Le Bourget du Lac, Savoie, France")

# ── Fetch data ─────────────────────────────────────────────────────────────────
@st.cache_data(ttl=30)
def load_data():
    try:
        rows = requests.get(f"{API}/uplinks", params={"limit": 500}, timeout=5).json()
        stats = requests.get(f"{API}/stats", timeout=5).json()
        return pd.DataFrame(rows), stats
    except Exception as e:
        st.error(f"Cannot reach API: {e}")
        return pd.DataFrame(), {}

df, stats = load_data()

if df.empty:
    st.warning("No data yet — waiting for uplinks from TTN.")
    st.stop()

df["received_at"] = pd.to_datetime(df["received_at"], format='ISO8601')
df = df.sort_values("received_at")

# ── Top KPI row ────────────────────────────────────────────────────────────────
latest = df.iloc[-1]

c1, c2, c3, c4, c5 = st.columns(5)
c1.metric("🌡️ Temperature", f"{latest['temperature']:.1f} °C",
          delta=f"{latest['temperature'] - latest['real_temp']:.1f} vs real" if latest['real_temp'] else None)
c2.metric("💧 Humidity", f"{latest['humidity']:.1f} %",
          delta=f"{latest['humidity'] - latest['real_humidity']:.1f} vs real" if latest['real_humidity'] else None)
c3.metric("📊 Pressure", f"{latest['pressure']:.1f} hPa",
          delta=f"{latest['pressure'] - latest['real_pressure']:.1f} vs real" if latest['real_pressure'] else None)
c4.metric("🤖 Model prediction", latest["prediction_fr"] or "—")
c5.metric("🌍 Real condition", latest["real_condition"] or "—",
          delta="✅ Match" if latest["match"] else "❌ Mismatch",
          delta_color="normal" if latest["match"] else "inverse")

st.divider()

# ── Accuracy gauge ─────────────────────────────────────────────────────────────
col_gauge, col_table = st.columns([1, 2])

with col_gauge:
    st.subheader("Model accuracy")
    accuracy = stats.get("accuracy_pct", 0) or 0
    total    = stats.get("total", 0)
    correct  = stats.get("correct", 0)

    fig_gauge = go.Figure(go.Indicator(
        mode="gauge+number+delta",
        value=accuracy,
        number={"suffix": "%"},
        delta={"reference": 50, "increasing": {"color": "green"}},
        gauge={
            "axis": {"range": [0, 100]},
            "bar":  {"color": "#1f77b4"},
            "steps": [
                {"range": [0, 40],   "color": "#ff4b4b"},
                {"range": [40, 70],  "color": "#ffa500"},
                {"range": [70, 100], "color": "#00cc96"},
            ],
            "threshold": {"line": {"color": "black", "width": 4}, "value": 70}
        },
        title={"text": f"Correct: {int(correct or 0)}/{total}"}
    ))
    fig_gauge.update_layout(height=280, margin=dict(t=40, b=0))
    st.plotly_chart(fig_gauge, use_container_width=True)

with col_table:
    st.subheader("Recent uplinks")
    display_cols = ["received_at", "temperature", "humidity", "pressure",
                    "prediction_fr", "real_condition", "match"]
    recent = df[display_cols].tail(10).copy()
    recent["received_at"] = recent["received_at"].dt.strftime("%H:%M:%S")
    recent["match"] = recent["match"].map({1: "✅", 0: "❌"})
    recent.columns = ["Time", "Temp °C", "Hum %", "Press hPa",
                       "Model", "Real", "Match"]
    st.dataframe(recent[::-1], use_container_width=True, hide_index=True)

st.divider()

# ── Time series: sensor vs real ────────────────────────────────────────────────
st.subheader("📈 Sensor readings vs Open-Meteo over time")

tab_temp, tab_hum, tab_press = st.tabs(["Temperature", "Humidity", "Pressure"])

def dual_chart(y_model, y_real, label, unit):
    fig = go.Figure()
    fig.add_trace(go.Scatter(
        x=df["received_at"], y=df[y_model],
        name=f"Sensor ({label})", line=dict(color="#1f77b4", width=2)
    ))
    fig.add_trace(go.Scatter(
        x=df["received_at"], y=df[y_real],
        name=f"Open-Meteo ({label})", line=dict(color="#ff7f0e", width=2, dash="dash")
    ))
    fig.update_layout(
        yaxis_title=unit, height=320,
        legend=dict(orientation="h", y=1.1),
        margin=dict(t=10, b=30)
    )
    return fig

with tab_temp:
    st.plotly_chart(dual_chart("temperature", "real_temp", "Temp", "°C"),
                    use_container_width=True)
with tab_hum:
    st.plotly_chart(dual_chart("humidity", "real_humidity", "Humidity", "%"),
                    use_container_width=True)
with tab_press:
    st.plotly_chart(dual_chart("pressure", "real_pressure", "Pressure", "hPa"),
                    use_container_width=True)

st.divider()

# ── Prediction distribution ────────────────────────────────────────────────────
st.subheader("🤖 Prediction class distribution")
pred_counts = df["prediction_fr"].value_counts().reset_index()
pred_counts.columns = ["Condition", "Count"]
fig_bar = go.Figure(go.Bar(
    x=pred_counts["Condition"], y=pred_counts["Count"],
    marker_color="#1f77b4"
))
fig_bar.update_layout(height=280, margin=dict(t=10, b=30))
st.plotly_chart(fig_bar, use_container_width=True)

st.caption("Auto-refreshes every 30 seconds · Data source: TTN LoRaWAN + Open-Meteo API")
