'use strict'

const express = require ('express')

const router = express.Router();
router.use(express.json())

function webapi(service) {

	const theWebApi = {
	
		home: (req, res) => {
			console.log("home")
		},
		
		charts: (req, res) => {
			console.log("charts")
		},

		firebase: (req, res) => {
			service.getInfo()
		}
	}
	
	router.get('/', theWebApi.home)
	router.get('/charts', theWebApi.charts)
	router.get('/firebase', theWebApi.firebase)

	return router;
}

module.exports = webapi
