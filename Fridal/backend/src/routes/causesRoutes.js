const express = require('express');
const { loadCauses, saveCauses, ensureBreakCause } = require('../store/causesStore');

const router = express.Router();

router.get('/', (req, res) => {
  res.json(loadCauses());
});

router.put('/', (req, res) => {
  const { causes, locations, locationCauses } = req.body;
  const current = loadCauses();
  if (causes) current.causes = causes;
  if (locations) current.locations = locations;
  if (locationCauses) current.locationCauses = locationCauses;
  ensureBreakCause(current);
  saveCauses(current);
  res.json({ success: true, ...current });
});

module.exports = router;
