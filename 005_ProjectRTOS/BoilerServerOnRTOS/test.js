const express = require('express');
const app = express();
app.get('/', (req, res) => res.send('SERVER IS WORKING'));
app.listen(3000, () => console.log('TEST SUCCESSFUL on 3000'));