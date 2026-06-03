import { useState, useEffect, useCallback } from 'react';
import axios from 'axios';
import { Thermometer, Droplets, Gauge, Cloud, CloudRain, CheckCircle, XCircle, Database, RotateCcw } from 'lucide-react';
import {
  LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer,
  BarChart, Bar, RadialBarChart, RadialBar
} from 'recharts';

const API_URL = import.meta.env.DEV ? '/api' : `http://${window.location.hostname}:8000`;

const formatTime = (dateStr) => new Date(dateStr).toLocaleTimeString('fr-FR', { hour: '2-digit', minute: '2-digit', second: '2-digit' });

export default function App() {
  const [uplinks, setUplinks] = useState([]);
  const [stats, setStats] = useState({ total: 0, correct: 0, accuracy_pct: 0 });
  const [influxData, setInfluxData] = useState([]);
  const [loading, setLoading] = useState(true);

  const fetchData = useCallback(async () => {
    try {
      const [uplinksRes, statsRes, influxRes] = await Promise.all([
        axios.get(`${API_URL}/uplinks?limit=500`),
        axios.get(`${API_URL}/stats`),
        axios.get(`${API_URL}/influx_history?limit=200`)
      ]);
      
      const sortedUplinks = uplinksRes.data
        .map(u => ({ ...u, received_at: new Date(u.received_at) }))
        .sort((a, b) => a.received_at - b.received_at);
      
      setUplinks(sortedUplinks);
      setStats(statsRes.data);
      if (influxRes.data && Array.isArray(influxRes.data)) {
        setInfluxData(influxRes.data.sort((a, b) => new Date(a.time) - new Date(b.time)));
      }
    } catch (err) {
      console.error('Failed to fetch data:', err);
    } finally {
      setLoading(false);
    }
  }, []);

  useEffect(() => {
    fetchData();
    
    const eventSource = new EventSource(`${API_URL}/events`);
    
    eventSource.onmessage = (event) => {
      if (event.data === 'update') {
        fetchData();
      }
    };

    return () => {
      eventSource.close();
    };
  }, [fetchData]);

  if (loading || uplinks.length === 0) {
    return (
      <div className="min-h-screen flex items-center justify-center bg-slate-900 text-slate-300">
        <div className="text-center">
          <CloudRain className="w-16 h-16 mx-auto mb-4 text-blue-400 animate-pulse" />
          <p className="text-xl">En attente des données TTN...</p>
        </div>
      </div>
    );
  }

  const latest = uplinks[uplinks.length - 1];
  const accuracy = stats.accuracy_pct || 0;
  const total = stats.total || 0;
  const correct = stats.correct || 0;

  const chartData = uplinks.map(u => ({
    time: formatTime(u.received_at),
    temp: u.temperature,
    realTemp: u.real_temp,
    hum: u.humidity,
    realHum: u.real_humidity,
    press: u.pressure,
    realPress: u.real_pressure
  }));

  const predCounts = {};
  uplinks.forEach(u => {
    if (u.prediction_fr) predCounts[u.prediction_fr] = (predCounts[u.prediction_fr] || 0) + 1;
  });
  const barData = Object.entries(predCounts).map(([Condition, Count]) => ({ Condition, Count }));

  const gaugeData = [{ name: 'Précision', value: accuracy, fill: accuracy >= 70 ? '#10b981' : accuracy >= 40 ? '#f59e0b' : '#ef4444' }];

  return (
    <div className="min-h-screen bg-slate-900 text-slate-100 p-4 md:p-8">
      <header className="mb-8 text-center">
        <h1 className="text-3xl md:text-4xl font-bold text-blue-400 flex items-center justify-center gap-3">
          <CloudRain className="w-10 h-10" /> metAI Dashboard
        </h1>
        <p className="text-slate-400 mt-2">Modèle ML embarqué STM32 vs Open-Meteo · Thonon-les-Bains, France</p>
      </header>

      {/* KPIs */}
      <div className="grid grid-cols-2 md:grid-cols-5 gap-4 mb-8">
        <KPICard icon={Thermometer} label="Température" value={`${latest.temperature?.toFixed(1) || '—'} °C`} delta={latest.real_temp ? `${(latest.temperature - latest.real_temp).toFixed(1)} vs réel` : null} color="text-red-400" />
        <KPICard icon={Droplets} label="Humidité" value={`${latest.humidity?.toFixed(1) || '—'} %`} delta={latest.real_humidity ? `${(latest.humidity - latest.real_humidity).toFixed(1)} vs réel` : null} color="text-blue-400" />
        <KPICard icon={Gauge} label="Pression" value={`${latest.pressure?.toFixed(1) || '—'} hPa`} delta={latest.real_pressure ? `${(latest.pressure - latest.real_pressure).toFixed(1)} vs réel` : null} color="text-yellow-400" />
        <KPICard icon={Cloud} label="Prédiction" value={latest.prediction_fr || '—'} color="text-purple-400" />
        <KPICard 
          icon={latest.match ? CheckCircle : XCircle} 
          label="Condition Réelle" 
          value={latest.real_condition || '—'} 
          delta={latest.match ? '✅ Match' : '❌ Mismatch'} 
          deltaColor={latest.match ? 'text-green-400' : 'text-red-400'}
          color="text-emerald-400" 
        />
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-8">
        {/* Gauge */}
        <div className="bg-slate-800 rounded-xl p-6 shadow-lg border border-slate-700 flex flex-col justify-center">
          <h3 className="text-xl font-semibold mb-4 text-center">Précision du Modèle</h3>
          <ResponsiveContainer width="100%" height={450}>
            <RadialBarChart cx="50%" cy="50%" innerRadius="60%" outerRadius="90%" barSize={25} data={gaugeData} startAngle={180} endAngle={0}>
              <RadialBar background clockWise dataKey="value" />
              <Tooltip formatter={(value) => `${value}%`} />
              <text x="50%" y="50%" textAnchor="middle" dominantBaseline="middle" className="fill-slate-100 text-5xl font-bold">
                {accuracy}%
              </text>
              <text x="50%" y="65%" textAnchor="middle" className="fill-slate-400 text-xl">
                {correct}/{total} corrects
              </text>
            </RadialBarChart>
          </ResponsiveContainer>
          <button 
            onClick={async () => {
              if (window.confirm('Êtes-vous sûr de vouloir réinitialiser toutes les statistiques et l\'historique ?')) {
                try {
                  await axios.post(`${API_URL}/reset`);
                  fetchData();
                } catch (err) {
                  console.error('Failed to reset stats:', err);
                  alert('Échec de la réinitialisation');
                }
              }
            }}
            className="mt-6 w-full py-2.5 px-4 bg-red-600 hover:bg-red-700 text-white rounded-lg font-medium transition flex items-center justify-center gap-2"
          >
            <RotateCcw className="w-4 h-4" /> Réinitialiser les statistiques
          </button>
        </div>

        {/* Recent Uplinks */}
        <div className="lg:col-span-2 bg-slate-800 rounded-xl p-6 shadow-lg border border-slate-700 overflow-hidden">
          <h3 className="text-lg font-semibold mb-4">Dernières Uplinks</h3>
          <div className="overflow-x-auto">
            <table className="w-full text-sm text-left">
              <thead className="text-xs text-slate-400 uppercase bg-slate-700/50">
                <tr>
                  <th className="px-4 py-3 rounded-l-lg">Heure</th>
                  <th className="px-4 py-3">Temp °C</th>
                  <th className="px-4 py-3">Hum %</th>
                  <th className="px-4 py-3">Press hPa</th>
                  <th className="px-4 py-3">Modèle</th>
                  <th className="px-4 py-3">Réel</th>
                  <th className="px-4 py-3 rounded-r-lg">Match</th>
                </tr>
              </thead>
              <tbody>
                {uplinks.slice(-10).reverse().map((u, i) => (
                  <tr key={i} className="border-b border-slate-700 hover:bg-slate-700/30 transition">
                    <td className="px-4 py-3 font-mono">{formatTime(u.received_at)}</td>
                    <td className="px-4 py-3">{u.temperature?.toFixed(1)}</td>
                    <td className="px-4 py-3">{u.humidity?.toFixed(1)}</td>
                    <td className="px-4 py-3">{u.pressure?.toFixed(1)}</td>
                    <td className="px-4 py-3 text-purple-300">{u.prediction_fr}</td>
                    <td className="px-4 py-3 text-emerald-300">{u.real_condition}</td>
                    <td className="px-4 py-3">{u.match ? '✅' : '❌'}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        </div>
      </div>

      {/* Charts */}
      <div className="bg-slate-800 rounded-xl p-6 shadow-lg border border-slate-700 mb-8">
        <h3 className="text-lg font-semibold mb-6">📈 Capteur vs Open-Meteo dans le temps</h3>
        <div className="space-y-8">
          <TimeSeriesChart data={chartData} dataKey1="temp" dataKey2="realTemp" label1="Capteur" label2="Open-Meteo" unit="°C" color1="#3b82f6" color2="#f97316" title="Température" />
          <TimeSeriesChart data={chartData} dataKey1="hum" dataKey2="realHum" label1="Capteur" label2="Open-Meteo" unit="%" color1="#3b82f6" color2="#f97316" title="Humidité" />
          <TimeSeriesChart data={chartData} dataKey1="press" dataKey2="realPress" label1="Capteur" label2="Open-Meteo" unit="hPa" color1="#3b82f6" color2="#f97316" title="Pression" />
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-8">
        {/* Prediction Distribution */}
        <div className="bg-slate-800 rounded-xl p-6 shadow-lg border border-slate-700">
          <h3 className="text-lg font-semibold mb-4">Distribution des Prédictions</h3>
          <ResponsiveContainer width="100%" height={300}>
            <BarChart data={barData}>
              <CartesianGrid strokeDasharray="3 3" stroke="#334155" />
              <XAxis dataKey="Condition" stroke="#94a3b8" fontSize={12} angle={-15} textAnchor="end" height={60} />
              <YAxis stroke="#94a3b8" />
              <Tooltip contentStyle={{ backgroundColor: '#1e293b', border: 'none', borderRadius: '8px' }} />
              <Bar dataKey="Count" fill="#3b82f6" radius={[4, 4, 0, 0]} />
            </BarChart>
          </ResponsiveContainer>
        </div>

        {/* InfluxDB History */}
        <div className="bg-slate-800 rounded-xl p-6 shadow-lg border border-slate-700 overflow-hidden">
          <h3 className="text-lg font-semibold mb-4 flex items-center gap-2"><Database className="w-5 h-5" /> Historique InfluxDB</h3>
          <div className="overflow-x-auto max-h-[350px]">
            <table className="w-full text-sm text-left">
              <thead className="text-xs text-slate-400 uppercase bg-slate-700/50 sticky top-0">
                <tr>
                  <th className="px-4 py-3">Heure</th>
                  <th className="px-4 py-3">Temp °C</th>
                  <th className="px-4 py-3">Hum %</th>
                  <th className="px-4 py-3">Press hPa</th>
                  <th className="px-4 py-3">Modèle</th>
                  <th className="px-4 py-3">Réel</th>
                </tr>
              </thead>
              <tbody>
                {influxData.slice(-20).reverse().map((row, i) => (
                  <tr key={i} className="border-b border-slate-700 hover:bg-slate-700/30 transition">
                    <td className="px-4 py-2 font-mono text-xs">{formatTime(row.time)}</td>
                    <td className="px-4 py-2">{row.temperature_C?.toFixed(1)}</td>
                    <td className="px-4 py-2">{row.humidity_pct?.toFixed(1)}</td>
                    <td className="px-4 py-2">{row.pressure_hPa?.toFixed(1)}</td>
                    <td className="px-4 py-2 text-purple-300">{row.weather_label?.replace(/"/g, '')}</td>
                    <td className="px-4 py-2 text-emerald-300">{row.real_condition?.replace(/"/g, '')}</td>
                  </tr>
                ))}
                {influxData.length === 0 && (
                  <tr><td colSpan="6" className="px-4 py-8 text-center text-slate-500">Aucune donnée InfluxDB</td></tr>
                )}
              </tbody>
            </table>
          </div>
        </div>
      </div>

      <footer className="text-center text-slate-500 text-sm py-4 border-t border-slate-800">
        Actualisation en temps réel (Server-Sent Events) · Source : TTN LoRaWAN + Open-Meteo API
      </footer>
    </div>
  );
}

function KPICard({ icon: Icon, label, value, delta, deltaColor = 'text-slate-400', color }) {
  return (
    <div className="bg-slate-800 rounded-xl p-4 shadow-lg border border-slate-700 flex flex-col items-center text-center">
      <Icon className={`w-8 h-8 mb-2 ${color}`} />
      <span className="text-slate-400 text-xs uppercase tracking-wider">{label}</span>
      <span className="text-xl font-bold mt-1">{value}</span>
      {delta && <span className={`text-xs mt-1 font-medium ${deltaColor}`}>{delta}</span>}
    </div>
  );
}

function TimeSeriesChart({ data, dataKey1, dataKey2, label1, label2, unit, color1, color2, title }) {
  return (
    <div>
      <h4 className="text-md font-medium text-slate-300 mb-2">{title}</h4>
      <ResponsiveContainer width="100%" height={250}>
        <LineChart data={data}>
          <CartesianGrid strokeDasharray="3 3" stroke="#334155" />
          <XAxis dataKey="time" stroke="#94a3b8" fontSize={12} />
          <YAxis stroke="#94a3b8" fontSize={12} unit={unit} />
          <Tooltip contentStyle={{ backgroundColor: '#1e293b', border: 'none', borderRadius: '8px' }} />
          <Legend />
          <Line type="monotone" dataKey={dataKey1} name={`${label1} (${unit})`} stroke={color1} strokeWidth={2} dot={false} />
          <Line type="monotone" dataKey={dataKey2} name={`${label2} (${unit})`} stroke={color2} strokeWidth={2} strokeDasharray="5 5" dot={false} />
        </LineChart>
      </ResponsiveContainer>
    </div>
  );
}
