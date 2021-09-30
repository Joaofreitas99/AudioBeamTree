'use strict'

function service( playlistFirebase) {
	const theService = {

		pickSong: (songId) => {
			return playlistFirebase.pickSong(songId)
		},
			
		getCounters: () => {
			return playlistFirebase.getCounters()
		}
	}
	return theService
}

module.exports = service


