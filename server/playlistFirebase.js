'use strict'

const admin = require('firebase-admin');
const serviceAccount = require('./serviceAccount.json');

admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  databaseURL: "https://playlist-ffed8-default-rtdb.europe-west1.firebasedatabase.app/"
});

// As an admin, the app has access to read and write all data, regardless of Security Rules
const db = admin.database();
const ref = db.ref("AudioBeamTree");
const countersRef = db.ref("Counters");

/*
const playlistRef = db.ref("Playlist");

playlistRef.set({
    '1': {
        'id': '1',
        'name': 'EverybodysChanging',
		'artist': 'Keane',
		'genre': 'Pop'
    },
    '2': {
        'id': '2',
        'name': 'ChamberOfReflection',
		'artist': 'Mac DeMarco',
		'genre': 'Pop'
    },
    '3': {
        'id': '3',
        'name': 'OdeToTheMets',
		'artist': 'The Strokes',
		'genre': 'Pop'
    }
})*/

let song1c = 0;
let song2c = 0;
let song3c = 0;
let song4c = 0;

function firestore_db() {
	
	const fdatabase = {
		
		pickSong: async function (songId) {
			
			ref.set({
			  Song: {
				id: songId
			  }
			})
			
			if(songId == 1){
				countersRef.update({
					Song1: {
						counter: ++song1c
					},
					Song2: {
						counter: song2c
					},
					Song3: {
						counter: song3c
					},
					Song4: {
						counter: song4c
					}
				})
			} else if(songId == 2){
				countersRef.update({
					Song1: {
						counter: song1c
					},
					Song2: {
						counter: ++song2c
					},
					Song3: {
						counter: song3c
					},
					Song4: {
						counter: song4c
					}
				})
			} else if(songId == 3){
				countersRef.update({
					Song1: {
						counter: song1c
					},
					Song2: {
						counter: song2c
					},
					Song3: {
						counter: ++song3c
					},
					Song4: {
						counter: song4c
					}
				})
			} else if(songId == 4){
				countersRef.update({
					Song1: {
						counter: song1c
					},
					Song2: {
						counter: song2c
					},
					Song3: {
						counter: song3c
					},
					Song4: {
						counter: ++song4c
					}
				})
			}
			
		},
		
		getCounters: function () {
			
			return new Promise( (resolve, reject) => {
				
				countersRef.on("value", (snapshot) => {
					const data = snapshot.val()
					resolve(data)
				})

			})
			
		}
	}
	
	return fdatabase
	
}

module.exports = firestore_db

