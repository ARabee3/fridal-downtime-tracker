const fs = require('fs');
const path = require('path');
const config = require('../config');

const DATA_FILE = path.join(__dirname, '../..', config.dataFile);

function loadData() {
  if (!fs.existsSync(DATA_FILE)) {
    return { sessions: {}, today: getTodayKey(), stops: [] };
  }
  return JSON.parse(fs.readFileSync(DATA_FILE, 'utf8'));
}

function saveData(data) {
  fs.writeFileSync(DATA_FILE, JSON.stringify(data, null, 2));
}

function getTodayKey() {
  return new Date().toISOString().split('T')[0];
}

module.exports = { loadData, saveData, getTodayKey };
module.exports.DATA_FILE = DATA_FILE;
