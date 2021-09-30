'use strict'

const firebase = require("firebase/app")

require("firebase/auth")
require("firebase/firestore")
require("firebase/database")

const firebaseConfig = {
    apiKey: "AIzaSyDBXf9PDQg7gYaSDMYcrUPA6l5HU7nYK2k",
    authDomain: "terceiroteste-dc309.firebaseapp.com",
    databaseURL: "https://terceiroteste-dc309-default-rtdb.firebaseio.com",
    projectId: "terceiroteste-dc309",
    storageBucket: "terceiroteste-dc309.appspot.com",
    messagingSenderId: "839005443357",
    appId: "1:839005443357:web:e7b4f42204b652c31c7df4",
    measurementId: "G-XNBE4V0T30"
};

firebase.initializeApp(firebaseConfig);

function firestore_db() {
	
	const db = {
		
		getInfo: function () {
			
			return new Promise( (resolve, reject) => {
				
				firebase.database().ref("/").on("value", (snapshot) => {
					
					const data = snapshot.val()
					let area1counter = 0;
					let area2counter = 0;
					let maxCounter = 0;
					
					console.log(data)
					
					for(let obj in data){
						if(data[obj].Area == 'Area 1') area1counter++
						if(data[obj].Area == 'Area 2') area2counter++
					}
					
					if(area1counter >= area2counter) maxCounter = area1counter
					else maxCounter = area2counter
					
					const counters = {
						"areaCounters": [area1counter, area2counter],
						"area1counter": area1counter,
						"area2counter": area2counter,
						"maxCounter": maxCounter
					}
					
					console.log(counters)
					
					resolve(counters)
	
				})
				
			})
	
		
	}
	
	return db
	
}

module.exports = firestore_db

