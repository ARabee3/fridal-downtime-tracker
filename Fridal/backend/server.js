const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const path = require('path');

const config = require('./src/config');
const sensorRoutes = require('./src/routes/sensorRoutes');
const stopsRoutes = require('./src/routes/stopsRoutes');
const causesRoutes = require('./src/routes/causesRoutes');
const exportsRoutes = require('./src/routes/exportsRoutes');
const simulateRoutes = require('./src/routes/simulateRoutes');
const { loadData, saveData, getTodayKey } = require('./src/store/dataStore');
const { parseDuration, formatDuration, getCurrentTimeStr } = require('./src/utils/dateUtils');

const app = express();

app.use(cors());
app.use(bodyParser.json());
app.use(express.static(path.join(__dirname, config.frontendPath)));

app.use('/api/sensor', sensorRoutes);
app.use('/api/stops', stopsRoutes);
app.use('/api/causes', causesRoutes);
app.use('/api/exports', exportsRoutes);
app.use('/api/simulate', simulateRoutes);

app.post('/api/submit', (req, res) => {
  res.redirect(307, '/api/stops/submit');
});

app.get('/api/download/:filename', (req, res) => {
  res.redirect(`/api/stops/download/${req.params.filename}`);
});

app.get('/worker', (req, res) => {
  res.sendFile(path.join(__dirname, config.frontendPath, 'worker.html'));
});

app.get('/{*path}', (req, res) => {
  res.sendFile(path.join(__dirname, config.frontendPath, 'index.html'));
});

// ─── Startup cleanup: auto-end stale active stops from previous session ─────
(function startupCleanup() {
  const data = loadData();
  const today = getTodayKey();

  if (data.today !== today) {
    data.today = today;
    data.stops = [];
    saveData(data);
    console.log(`[STARTUP] New day detected (${today}). Reset stops.`);
    return;
  }

  const nowStr = getCurrentTimeStr();
  let cleaned = 0;

  data.stops.forEach(s => {
    if (s.status === 'active') {
      const durationSeconds = parseDuration(s.start, nowStr);
      s.end = nowStr;
      s.duration = formatDuration(durationSeconds);
      s.durationMinutes = durationSeconds / 60;
      s.durationSeconds = durationSeconds;
      s.isMicrostop = durationSeconds <= (config.microstopThreshold * 60);
      s.status = s.isMicrostop ? 'done' : 'pending';
      if (s.isMicrostop) {
        s.cause = 'Microstop';
        s.location = '-';
      }
      delete s.lockedBy;
      delete s.lockedAt;
      cleaned++;
      console.log(`[STARTUP] Auto-ended stale active stop ${s.id} (${s.machine_id}) — duration ${s.duration}`);
    }
  });

  if (cleaned > 0) {
    saveData(data);
    console.log(`[STARTUP] Cleaned ${cleaned} stale active stop(s).`);
  }
})();

app.listen(config.port, () => {
  console.log(`\n🏭 FRIDAL Downtime Tracker`);
  console.log(`   Running at http://localhost:${config.port}`);
  console.log(`   API: http://localhost:${config.port}/api/stops\n`);
});
