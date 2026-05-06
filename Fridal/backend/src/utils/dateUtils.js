function getTodayKey() {
  return new Date().toISOString().split('T')[0];
}

function parseDuration(startStr, endStr) {
  const [sh, sm] = startStr.split(':').map(Number);
  const [eh, em] = endStr.split(':').map(Number);
  const totalStart = sh * 60 + sm;
  const totalEnd = eh * 60 + em;
  const diff = totalEnd - totalStart;
  if (diff <= 0) return 0;
  return diff;
}

function formatDuration(minutes) {
  const h = Math.floor(minutes / 60);
  const m = minutes % 60;
  return h > 0 ? `${h}h ${m}m` : `${m}m`;
}

function getCurrentTimeStr(date = new Date()) {
  const now = date;
  return `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}`;
}

module.exports = { getTodayKey, parseDuration, formatDuration, getCurrentTimeStr };
