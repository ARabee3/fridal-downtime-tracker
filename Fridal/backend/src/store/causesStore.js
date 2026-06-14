const fs = require('fs');
const path = require('path');
const config = require('../config');

const CAUSES_FILE = path.join(__dirname, '../..', config.causesFile);

const EMPTY_DATA = { causes: [], locations: [], locationCauses: {} };
const BREAK_CAUSE = 'Break';

function ensureBreakCause(data) {
  let changed = false;

  // Ensure Break exists in the global causes list
  if (!data.causes) data.causes = [];
  if (!data.causes.includes(BREAK_CAUSE)) {
    data.causes.unshift(BREAK_CAUSE);
    changed = true;
  }

  // Ensure Break is first in every location's cause list
  if (!data.locationCauses) data.locationCauses = {};
  for (const loc of data.locations || []) {
    const list = data.locationCauses[loc] || (data.locationCauses[loc] = []);
    const idx = list.indexOf(BREAK_CAUSE);
    if (idx === -1) {
      list.unshift(BREAK_CAUSE);
      changed = true;
    } else if (idx !== 0) {
      list.splice(idx, 1);
      list.unshift(BREAK_CAUSE);
      changed = true;
    }
  }

  return changed;
}

function loadCauses() {
  if (!fs.existsSync(CAUSES_FILE)) {
    fs.writeFileSync(CAUSES_FILE, JSON.stringify(EMPTY_DATA, null, 2));
    return { ...EMPTY_DATA };
  }
  const data = JSON.parse(fs.readFileSync(CAUSES_FILE, 'utf8'));
  if (ensureBreakCause(data)) {
    saveCauses(data);
  }
  return data;
}

function saveCauses(causes) {
  fs.writeFileSync(CAUSES_FILE, JSON.stringify(causes, null, 2));
}

module.exports = { loadCauses, saveCauses, ensureBreakCause, CAUSES_FILE };
