/*
 * main.js
 *
 */

'use strict';

function getR() {
    var url = location.search;
    var theRequest = {};
    if (url.indexOf('?') !== -1) {
        var str = url.substr(1);
        var strs = str.split('&');
        for (var i = 0; i < strs.length; i++) {
            theRequest[strs[i].split('=')[0]] = decodeURIComponent(strs[i].split('=')[1]);
        }
    }
    return theRequest;
}

var gR = getR();

const connectButton    = document.querySelector('button#connectButton');
const disconnectButton = document.querySelector('button#disconnectButton');
const audio            = document.querySelector('audio#audio');
const remoteVideo      = document.getElementById('remoteVideo');

connectButton.onclick     = connect_call;
disconnectButton.onclick  = disconnect_call;
disconnectButton.disabled = true;


let pc;           /* PeerConnection */
let localStream;  /* MediaStream */
let session_id;   /* String */


const configuration = {
	bundlePolicy: 'balanced',

	/* certificates */

	iceCandidatePoolSize: 0,

	iceServers: [
	{
		urls: gR['stun']? gR['stun']:'stun:192.168.1.221:3478'
	}
	],

	iceTransportPolicy: 'all',
	tcpCandidatePolicy: 'disable',
	/* peerIdentity */
};

const constraints = {
	audio: true,
	/*video: { width:640, height:480, framerate:30 }*/
};

/*
 * https://developer.mozilla.org/en-US/docs/Web/API/RTCPeerConnection/createOffer#rtcofferoptions_dictionary
 */
const offerOptions = {
	iceRestart:             false,
	voiceActivityDetection: true
};

var sdp_send = false;
function connect_call()
{
	connectButton.disabled = true;

	console.log("Connecting call");

	pc = new RTCPeerConnection(configuration);

	pc.onicecandidate = (event) => {

		if (event.candidate && event.candidate.protocol === 'tcp' && !sdp_send) {

			// All ICE candidates have been sent
            sdp_send = true;
			const sd = pc.localDescription;
			const json = JSON.stringify(sd);

			send_post_sdp(json);
		}
	};

	pc.onicecandidateerror = function(event) {

		//disconnect_call();

		//alert("ICE Candidate Error: " +
		//      event.errorCode + " " + event.errorText);
	}

	pc.ontrack = function(event) {

		const track = event.track;

		console.log("got remote track: kind=%s", track.kind);

		if (audio.srcObject !== event.streams[0]) {
			audio.srcObject = event.streams[0];
			console.log("received remote audio stream");
		}

		if (remoteVideo.srcObject !== event.streams[0]) {
			remoteVideo.srcObject = event.streams[0];
			console.log("received remote video stream");
		}
	};

	console.log("Requesting local stream");

	navigator.mediaDevices.getUserMedia(constraints)
		.then(function(stream) {

			disconnectButton.disabled = false;

			// save the stream
			localStream = stream;

			// type: MediaStreamTrack
			const audioTracks = localStream.getAudioTracks();
			const videoTracks = localStream.getVideoTracks();

			if (audioTracks.length > 0) {
				console.log("Using Audio device: '%s'",
					    audioTracks[0].label);
			}
			if (videoTracks.length > 0) {
				console.log("Using Video device: '%s'",
					    videoTracks[0].label);
			}

			localStream.getTracks()
				.forEach(track => pc.addTrack(track, localStream));

			send_post_connect();
		})
		.catch(function(error) {

			alert("Get User Media: " + error);
		});
}


/*
 * Create a new call
 */
function send_post_connect()
{
	var xhr = new XMLHttpRequest();
	const loc = self.location.origin + '/';

	console.log("send post connect: " + loc);

	xhr.open("POST", '' + loc + 'connect', true);

	xhr.onreadystatechange = function() {

		if (this.readyState === XMLHttpRequest.DONE &&
		    this.status === 200) {

			const sessid = xhr.getResponseHeader("Session-ID");

			console.log(".... session: %s", sessid);

			/* Save the session ID */
			session_id = sessid;

			pc.createOffer(offerOptions)
			.then(function (desc) {
				console.log("got local description: %s", desc.type);

				pc.setLocalDescription(desc).then(() => {
				},
				function (error) {
					console.log("setLocalDescription: %s",
						    error.toString());
				});
			})
			.catch(function(error) {
			       console.log("Failed to create session description: %s",
					   error.toString());
			});
		}
	}

	xhr.send();
}


function send_post_sdp(descr)
{
	var xhr = new XMLHttpRequest();
	const loc = self.location.origin + '/';

	console.log("send post sdp: " + loc);

	xhr.open("POST", '' + loc + 'sdp', true);
	xhr.setRequestHeader("Content-Type", "application/json");
	xhr.setRequestHeader("Session-ID", session_id);

	xhr.onreadystatechange = function() {
		if (this.readyState === XMLHttpRequest.DONE &&
		    this.status === 200) {

			console.log("post sdp: 200 ok");

			const descr = JSON.parse(xhr.response);

			console.log("remote description: type=%s", descr.type);

			pc.setRemoteDescription(descr).then(() => {
				console.log('set remote description -- success');
			}, function (error) {
				console.log("setRemoteDescription: %s",
					    error.toString());
			});
		}
	}

	xhr.send(descr);
}


function disconnect_call()
{
	console.log("Disconnecting call");
	sdp_send = false;

	localStream.getTracks().forEach(track => track.stop());

	pc.close();
	pc = null;

	disconnectButton.disabled = true;
	connectButton.disabled = false;

	// send a message to the server
	var xhr = new XMLHttpRequest();
	xhr.open("POST", '' + self.location.origin + '/disconnect', true);
	xhr.setRequestHeader("Session-ID", session_id);
	xhr.send();

	session_id = null;
}
