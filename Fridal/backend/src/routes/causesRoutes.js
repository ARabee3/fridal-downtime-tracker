const express = require('express');
const { loadCauses, saveCauses } = require('../store/causesStore');

const router = express.Router();

router.get('/', (req, res) => {
  res.json(loadCauses());
});

router.put('/', (req, res) => {
  const { causes, locations } = req.body;
  const current = loadCauses();
  if (causes) current.causes = causes;
  if (locations) current.locations = locations;
  saveCauses(current);
  res.json({ success: true, ...current });
});

module.exports = router;
