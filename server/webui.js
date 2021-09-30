'use strict'

const express = require ('express')

/*const router = express.Router();
router.use(express.json())*/

function webui(service) {

	const theWebUI = {
	
		home: (req, res) => {
			res.render('home', {})
		},
		
		songs: (req, res) => {
			res.render('showSongs', {})
		},
		
		pickSong: (req, res) => {
			const songId = req.body.txtSong
			
			service.pickSong(songId)
			.then(id => {
				const answer = {
					song: id
				}
				res.render('playSong', answer)
			})
			.catch(err => {
				res.send(`<html><body><strong>ERROR </strong>${err.Message}</body></html> <p><a href="/">Return To Home</a></p>`)
			})
		},
		
		getCounters:  (req, res) => {	
			service.getCounters()
			.then(counters => {
				const song1counter = counters.Song1.counter
				const song2counter = counters.Song2.counter
				const song3counter = counters.Song3.counter
				const song4counter = counters.Song4.counter
				const answer = {
					song1: song1counter,
					song2: song2counter,
					song3: song3counter,
					song4: song4counter
				}
				res.render('showCounters', answer)
			})
			.catch(err => {
				res.send(`<html><body><strong>ERROR </strong>${err.Message}</body></html> <p><a href="/">Return To Home</a></p>`)
			})
		},
		
		about: (req, res) => {
			res.render('showAbout', {})
		}
	}
	
	const router = express.Router();
	router.use(express.urlencoded({
		extended: true
	}))
	
	router.get('/', theWebUI.home)
	router.get('/songs', theWebUI.songs)
	router.post('/pickSong', theWebUI.pickSong)
	router.get('/about', theWebUI.about)
	router.get('/counters', theWebUI.getCounters)

	return router;
}

module.exports = webui
