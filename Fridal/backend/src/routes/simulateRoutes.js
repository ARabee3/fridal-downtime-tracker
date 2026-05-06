const express = require('express');
const { v4: uuidv4 } = require('uuid');
const { loadData, saveData, getTodayKey } = require('../store/dataStore');
const { formatDuration, getCurrentTimeStr } = require('../utils/dateUtils');
const config = require('../config');

const router = express.Router();

router.post('/start', (req, res) => {
  const { machine_id = 'M1' } = req.body;
  const data = loadData();
  const today = getTodayKey();
  if (data.today !== today) { data.today = today; data.stops = []; }

  const activeStop = [...data.stops].reverse().find(
    s => s.machine_id === machine_id && s.status === 'active'
  );

  if (activeStop) {
    return res.status(409).json({
      error: 'Active stop already exists for machine',
      stop_id: activeStop.id,
      stop: activeStop
    });
  }

  const startTimestamp = Date.now();
  const stop = {
    id: uuidv4(), machine_id, start: getCurrentTimeStr(new Date(startTimestamp)),
    startTimestamp,
    end: null, duration: null, durationMinutes: null, durationSeconds: null,
    cause: '', location: '', isMicrostop: false, status: 'active', date: today
  };
  data.stops.push(stop);
  saveData(data);
  res.json({ success: true, stop_id: stop.id, message: 'Simulated failure start' });
});

router.post('/end', (req, res) => {
  const data = loadData();
  const { machine_id = 'M1' } = req.body;
  const stop = [...data.stops].reverse().find(s => s.status === 'active' && s.machine_id === machine_id);
  if (!stop) return res.status(404).json({ error: 'No active stop' });
  const startTimestamp = Number.isFinite(stop.startTimestamp) ? stop.startTimestamp : Date.now();
  const requestedMinutes = Number(req.body.minutes);

  let endTimestamp = Date.now();
  let durationSeconds;

  if (Number.isFinite(requestedMinutes) && requestedMinutes > 0) {
    durationSeconds = Math.round(requestedMinutes * 60);
    endTimestamp = startTimestamp + durationSeconds * 1000;
  } else {
    const elapsedMs = Math.max(0, endTimestamp - startTimestamp);
    durationSeconds = Math.round(elapsedMs / 1000);
  }

  stop.end = getCurrentTimeStr(new Date(endTimestamp));
  stop.endTimestamp = endTimestamp;
  stop.duration = formatDuration(durationSeconds);
  stop.durationMinutes = durationSeconds / 60;
  stop.durationSeconds = durationSeconds;
  stop.isMicrostop = durationSeconds <= (config.microstopThreshold * 60);
  stop.status = stop.isMicrostop ? 'done' : 'pending';
  if (stop.isMicrostop) { stop.cause = 'Microstop'; stop.location = '-'; }
  saveData(data);
  res.json({ success: true, stop, message: 'Simulated failure end' });
});

module.exports = router;
