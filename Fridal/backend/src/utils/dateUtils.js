function getTodayKey() {
  return new Date().toISOString().split('T')[0];
}

function parseDuration(startStr, endStr) {
  const startParts = startStr.split(':').map(Number);
  const endParts = endStr.split(':').map(Number);
  const [sh, sm, ss = 0] = startParts;
  const [eh, em, es = 0] = endParts;
  const totalStart = sh * 3600 + sm * 60 + ss;
  const totalEnd = eh * 3600 + em * 60 + es;
  const diff = totalEnd - totalStart;
  if (diff <= 0) return 0;
  return diff;
}

function formatDuration(seconds) {
  const totalSeconds = Math.max(0, Math.floor(seconds));
  const h = Math.floor(totalSeconds / 3600);
  const m = Math.floor((totalSeconds % 3600) / 60);
  const s = totalSeconds % 60;
  if (h > 0) return `${h}h ${m}m ${s}s`;
  if (m > 0) return `${m}m ${s}s`;
  return `${s}s`;
}

function getCurrentTimeStr(date = new Date()) {
  const now = date;
  return `${String(now.getHours()).padStart(2, '0')}:${String(now.getMinutes()).padStart(2, '0')}:${String(now.getSeconds()).padStart(2, '0')}`;
}

module.exports = { getTodayKey, parseDuration, formatDuration, getCurrentTimeStr };
