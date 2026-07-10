const express = require('express');
const cors = require('cors');
const app = express();
const port = process.env.PORT || 5000;

app.use(cors());
app.use(express.json());

// Health check
app.get('/api/health', (req, res) => {
  res.json({ status: 'ok', timestamp: new Date().toISOString() });
});

// Example endpoint – later you can add APK download, SDR control, etc.
app.get('/api/info', (req, res) => {
  res.json({
    project: 'Quad-RF Android',
    version: '0.2.0',
    description: 'Android SDR host for Quad-RF board'
  });
});

app.listen(port, () => {
  console.log(`API server running on port ${port}`);
});
