const express = require('express');
const { v4: uuidv4 } = require('uuid');
const { loadData, saveData, getTodayKey } = require('../store/dataStore');
const { parseDuration, formatDuration, getCurrentTimeStr } = require('../utils/dateUtils');
const config = require('../config');

const router = express.Router();

router.post('/failure-start', (req, res) => {
  const data = loadData();
  const today = getTodayKey();

  if (data.today !== today) {
    data.today = today;
    data.stops = [];
  }

  const { machine_id = 'M1', timestamp } = req.body;
  const startTime = timestamp || getCurrentTimeStr();

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

  const stop = {
    id: uuidv4(),
    machine_id,
    start: startTime,
    end: null,
    duration: null,
    durationMinutes: null,
    durationSeconds: null,
    cause: '',
    location: '',
    isMicrostop: false,
    status: 'active',
    date: today
  };

  data.stops.push(stop);
  saveData(data);

  console.log(`[SENSOR] Failure START at ${startTime} — ID: ${stop.id}`);
  res.json({ success: true, stop_id: stop.id, start: startTime });
});

router.post('/failure-end', (req, res) => {
  const data = loadData();
  const { stop_id, machine_id = 'M1', timestamp } = req.body;
  const endTime = timestamp || getCurrentTimeStr();

  let stop;
  if (stop_id) {
    stop = data.stops.find(s => s.id === stop_id && s.status === 'active');
  } else {
    stop = [...data.stops].reverse().find(s => s.machine_id === machine_id && s.status === 'active');
  }

  if (!stop) {
    return res.status(404).json({ error: 'No active stop found' });
  }

  const durationSeconds = parseDuration(stop.start, endTime);
  stop.end = endTime;
  stop.duration = formatDuration(durationSeconds);
  stop.durationMinutes = durationSeconds / 60;
  stop.durationSeconds = durationSeconds;
  stop.isMicrostop = durationSeconds <= (config.microstopThreshold * 60);
  stop.status = 'pending';

  if (stop.isMicrostop) {
    stop.cause = 'Microstop';
    stop.location = '-';
    stop.status = 'done';
  }

  saveData(data);
  console.log(`[SENSOR] Failure END at ${endTime} — Duration: ${stop.duration}`);
  res.json({ success: true, stop });
});

module.exports = router;
