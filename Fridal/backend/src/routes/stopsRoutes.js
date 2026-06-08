const express = require('express');
const path = require('path');
const fs = require('fs');
const XLSX = require('xlsx');
const { loadData, saveData, getTodayKey } = require('../store/dataStore');
const { formatDuration } = require('../utils/dateUtils');
const config = require('../config');

const EXPORTS_DIR = path.join(__dirname, '../..', config.exportsDir);
const router = express.Router();

if (!fs.existsSync(EXPORTS_DIR)) {
  fs.mkdirSync(EXPORTS_DIR);
}

const LOCK_TTL_MS = 60000; // 1 minute worker lock

router.get('/', (req, res) => {
  const data = loadData();
  const today = getTodayKey();
  if (data.today !== today) {
    return res.json({ stops: [], date: today, summary: { total: 0, totalMinutes: 0, pending: 0 } });
  }

  let needsSave = false;
  data.stops.forEach(s => {
    if (s.lockedBy === 'worker' && s.lockedAt && (Date.now() - s.lockedAt) >= LOCK_TTL_MS) {
      delete s.lockedBy;
      delete s.lockedAt;
      needsSave = true;
    }
  });
  if (needsSave) saveData(data);

  const stops = data.stops.filter(s => s.date === today);
  const totalSeconds = stops.reduce((sum, s) => sum + (s.durationSeconds ?? Math.round((s.durationMinutes || 0) * 60)), 0);
  const pending = stops.filter(s => s.status === 'pending').length;

  res.json({
    stops,
    date: today,
    summary: {
      total: stops.length,
      totalMinutes: totalSeconds / 60,
      totalSeconds,
      totalFormatted: formatDuration(totalSeconds),
      pending,
      active: stops.filter(s => s.status === 'active').length
    }
  });
});

router.patch('/:id', (req, res) => {
  const data = loadData();
  const stop = data.stops.find(s => s.id === req.params.id);
  if (!stop) return res.status(404).json({ error: 'Stop not found' });

  const { cause, location } = req.body;
  if (cause !== undefined) stop.cause = cause;
  if (location !== undefined) stop.location = location;
  if (stop.cause && stop.location) stop.status = 'done';

  // Clear any stale lock fields on successful edit
  if (stop.lockedBy || stop.lockedAt) {
    delete stop.lockedBy;
    delete stop.lockedAt;
  }

  saveData(data);
  res.json({ success: true, stop });
});

router.post('/:id/lock', (req, res) => {
  const data = loadData();
  const stop = data.stops.find(s => s.id === req.params.id);
  if (!stop) return res.status(404).json({ error: 'Stop not found' });

  stop.lockedBy = req.body.lockedBy || 'worker';
  stop.lockedAt = Date.now();
  saveData(data);
  res.json({ success: true, stop });
});

router.post('/:id/unlock', (req, res) => {
  const data = loadData();
  const stop = data.stops.find(s => s.id === req.params.id);
  if (!stop) return res.status(404).json({ error: 'Stop not found' });

  delete stop.lockedBy;
  delete stop.lockedAt;
  saveData(data);
  res.json({ success: true, stop });
});

router.post('/submit', (req, res) => {
  const data = loadData();
  const today = getTodayKey();
  const stops = data.stops.filter(s => s.date === today && s.end !== null);

  if (stops.length === 0) {
    return res.status(400).json({ error: 'No completed stops to export' });
  }

  const wb = XLSX.utils.book_new();

  const rows = stops.map(s => ({
    'Date': s.date,
    'Machine': s.machine_id,
    'Start Time': s.start,
    'End Time': s.end || '-',
    'Duration': s.duration || '-',
    'Duration (sec)': s.durationSeconds ?? Math.round((s.durationMinutes || 0) * 60),
    'Location': s.location || '-',
    'Cause': s.cause || '-',
    'Type': s.isMicrostop ? 'Microstop' : 'Stop',
    'Status': s.status
  }));

  const ws = XLSX.utils.json_to_sheet(rows);
  ws['!cols'] = [
    { wch: 12 }, { wch: 10 }, { wch: 12 }, { wch: 12 },
    { wch: 12 }, { wch: 15 }, { wch: 20 }, { wch: 30 }, { wch: 12 }, { wch: 10 }
  ];

  XLSX.utils.book_append_sheet(wb, ws, 'Downtime Log');

  const totalSeconds = stops.reduce((sum, s) => sum + (s.durationSeconds ?? Math.round((s.durationMinutes || 0) * 60)), 0);
  const microstops = stops.filter(s => s.isMicrostop);
  const regularStops = stops.filter(s => !s.isMicrostop);

  const causeGroups = {};
  regularStops.forEach(s => {
    const key = s.cause || 'Unclassified';
    if (!causeGroups[key]) causeGroups[key] = { count: 0, minutes: 0 };
    causeGroups[key].count++;
    causeGroups[key].minutes += (s.durationSeconds ?? Math.round((s.durationMinutes || 0) * 60));
  });

  const summaryRows = [
    { 'Metric': 'Report Date', 'Value': today },
    { 'Metric': 'Total Stops', 'Value': stops.length },
    { 'Metric': 'Regular Stops', 'Value': regularStops.length },
    { 'Metric': 'Microstops (≤1 min)', 'Value': microstops.length },
    { 'Metric': 'Total Downtime', 'Value': formatDuration(totalSeconds) },
    { 'Metric': 'Total Downtime (sec)', 'Value': totalSeconds },
    { 'Metric': '', 'Value': '' },
    { 'Metric': '--- By Cause ---', 'Value': '' },
    ...Object.entries(causeGroups).map(([cause, d]) => ({
      'Metric': cause,
      'Value': `${d.count} stops / ${formatDuration(d.minutes)}`
    }))
  ];

  const wsSummary = XLSX.utils.json_to_sheet(summaryRows);
  wsSummary['!cols'] = [{ wch: 30 }, { wch: 25 }];
  XLSX.utils.book_append_sheet(wb, wsSummary, 'Summary');

  const filename = `downtime_${today}.xlsx`;
  const filepath = path.join(EXPORTS_DIR, filename);
  XLSX.writeFile(wb, filepath);

  data.stops.forEach(s => { if (s.date === today) s.submitted = true; });
  saveData(data);

  console.log(`[EXPORT] Saved: ${filepath}`);
  res.json({ success: true, filename, path: filepath, totalStops: stops.length });
});

router.get('/download/:filename', (req, res) => {
  const filepath = path.join(EXPORTS_DIR, req.params.filename);
  if (!fs.existsSync(filepath)) return res.status(404).json({ error: 'File not found' });
  res.download(filepath);
});

module.exports = router;
