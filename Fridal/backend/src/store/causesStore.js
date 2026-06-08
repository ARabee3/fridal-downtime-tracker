const fs = require('fs');
const path = require('path');
const config = require('../config');

const CAUSES_FILE = path.join(__dirname, '../..', config.causesFile);

const EMPTY_DATA = { causes: [], locations: [] };

function loadCauses() {
  if (!fs.existsSync(CAUSES_FILE)) {
    fs.writeFileSync(CAUSES_FILE, JSON.stringify(EMPTY_DATA, null, 2));
    return { ...EMPTY_DATA };
  }
  return JSON.parse(fs.readFileSync(CAUSES_FILE, 'utf8'));
}

function saveCauses(causes) {
  fs.writeFileSync(CAUSES_FILE, JSON.stringify(causes, null, 2));
}

module.exports = { loadCauses, saveCauses, CAUSES_FILE };
