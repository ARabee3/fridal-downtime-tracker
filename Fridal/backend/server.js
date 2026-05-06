const express = require('express');
const cors = require('cors');
const bodyParser = require('body-parser');
const path = require('path');

const config = require('./src/config');
const sensorRoutes = require('./src/routes/sensorRoutes');
const stopsRoutes = require('./src/routes/stopsRoutes');
const causesRoutes = require('./src/routes/causesRoutes');
const exportsRoutes = require('./src/routes/exportsRoutes');
const simulateRoutes = require('./src/routes/simulateRoutes');

const app = express();

app.use(cors());
app.use(bodyParser.json());
app.use(express.static(path.join(__dirname, config.frontendPath)));

app.use('/api/sensor', sensorRoutes);
app.use('/api/stops', stopsRoutes);
app.use('/api/causes', causesRoutes);
app.use('/api/exports', exportsRoutes);
app.use('/api/simulate', simulateRoutes);

app.post('/api/submit', (req, res) => {
  res.redirect(307, '/api/stops/submit');
});

app.get('/api/download/:filename', (req, res) => {
  res.redirect(`/api/stops/download/${req.params.filename}`);
});

app.get('/{*path}', (req, res) => {
  res.sendFile(path.join(__dirname, config.frontendPath, 'index.html'));
});

app.listen(config.port, () => {
  console.log(`\n🏭 Fridail Downtime Tracker`);
  console.log(`   Running at http://localhost:${config.port}`);
  console.log(`   API: http://localhost:${config.port}/api/stops\n`);
});
