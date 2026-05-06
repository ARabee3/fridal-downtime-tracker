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
    end: null, duration: null, durationMinutes: null,
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
  let durationMinutes;

  if (Number.isFinite(requestedMinutes) && requestedMinutes > 0) {
    durationMinutes = Math.round(requestedMinutes);
    endTimestamp = startTimestamp + durationMinutes * 60 * 1000;
  } else {
    const elapsedMs = Math.max(0, endTimestamp - startTimestamp);
    durationMinutes = Math.round(elapsedMs / 60000);
  }

  stop.end = getCurrentTimeStr(new Date(endTimestamp));
  stop.endTimestamp = endTimestamp;
  stop.duration = formatDuration(durationMinutes);
  stop.durationMinutes = durationMinutes;
  stop.isMicrostop = durationMinutes <= config.microstopThreshold;
  stop.status = stop.isMicrostop ? 'done' : 'pending';
  if (stop.isMicrostop) { stop.cause = 'Microstop'; stop.location = '-'; }
  saveData(data);
  res.json({ success: true, stop, message: 'Simulated failure end' });
});

module.exports = router;
