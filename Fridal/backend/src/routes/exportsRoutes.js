const express = require('express');
const path = require('path');
const fs = require('fs');
const config = require('../config');

const EXPORTS_DIR = path.join(__dirname, '../..', config.exportsDir);
const router = express.Router();

router.get('/', (req, res) => {
  const files = fs.readdirSync(EXPORTS_DIR)
    .filter(f => f.endsWith('.xlsx'))
    .map(f => ({
      filename: f,
      date: f.replace('downtime_', '').replace('.xlsx', ''),
      size: fs.statSync(path.join(EXPORTS_DIR, f)).size
    }))
    .sort((a, b) => b.date.localeCompare(a.date));
  res.json({ exports: files });
});

module.exports = router;
