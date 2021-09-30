'use strict'

const port = 8888;

const express = require('express')
const app = express()

const playlistFirebaseCreator = require('./playlistFirebase.js')
const playlistFirebase = playlistFirebaseCreator()

const serviceCreator = require('./services.js')
const service = serviceCreator(playlistFirebase)

const webapiCreator = require('./webapi.js')
const webapi = webapiCreator(service)

const webuiCreator = require('./webui.js')
const webui = webuiCreator(service)

app.use('/api', webapi);
app.use(webui);
app.set('view engine', 'hbs')

app.listen(port, () => console.log("Listening on port 8888"))

