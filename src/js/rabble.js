var imageNumber = 0;
var xhrTimeout = null;
var DOWNLOAD_TIMEOUT = 10000;

function sendBitmap(bitmap, chunkSize, thisImageNumber) {
	if (imageNumber !== thisImageNumber) {
		console.log("image number mismatch: " + imageNumber + ", " + thisImageNumber);
		return;
	}

	if (xhrTimeout === null) {
		return;
	}

	clearTimeout(xhrTimeout);
	xhrTimeout = null;

	sendAppMessageEx(NET_IMAGE_QUEUE, {
		"NETIMAGE_BEGIN": bitmap.length
	});

	for (var i = 0; i < bitmap.length; i += chunkSize) {
		sendAppMessageEx(NET_IMAGE_QUEUE, {
			"NETIMAGE_DATA": bitmap.slice(i, i + chunkSize)
		});
	}

	sendAppMessageEx(NET_IMAGE_QUEUE, {
		"NETIMAGE_END": "done"
	});
}

function sendFailure(error, thisImageNumber) {
	console.log("sendFailure: " + error);

	if (imageNumber !== thisImageNumber) {
		console.log("image number mismatch: " + imageNumber + ", " + thisImageNumber);
		return;
	}

	if (xhrTimeout !== null) {
		clearTimeout(xhrTimeout);
		xhrTimeout = null;
	}

	sendAppMessageEx(NET_IMAGE_QUEUE, {
		"NETIMAGE_BEGIN": 0
	});
	sendAppMessageEx(NET_IMAGE_QUEUE, {
		"NETIMAGE_END": "done"
	});
}

function convertImage(rgbaPixels, numComponents, width, height) {

	var watch_info;
	if (Pebble.getActiveWatchInfo) {
		watch_info = Pebble.getActiveWatchInfo() || {
			'platform': 'aplite'
		};

	} else {
		watch_info = {
			'platform': 'aplite'
		};
	}

	var ratio = Math.min(144 / width, 168 / height);
	var ratio = Math.min(ratio, 1);

	var final_width = Math.floor(width * ratio);
	var final_height = Math.floor(height * ratio);
	var final_pixels = [];
	var bitmap = [];

	if (watch_info.platform === 'aplite') {
		var grey_pixels = greyScale(rgbaPixels, width, height, numComponents);

		ScaleRect(final_pixels, grey_pixels, width, height, final_width, final_height, 1);
		floydSteinberg(final_pixels, final_width, final_height, pebble_nearest_color_to_black_white);

		bitmap = toPBI(final_pixels, final_width, final_height);

	} else {
		ScaleRect(final_pixels, rgbaPixels, width, height, final_width, final_height, numComponents);
		floydSteinberg(final_pixels, final_width, final_height, pebble_nearest_color_to_pebble_palette);

		var png = generatePngForPebble(final_width, final_height, final_pixels);

		for (var i = 0; i < png.length; i++) {
			bitmap.push(png.charCodeAt(i));
		}
	}

	return bitmap;
}

function getPbiImage(url, chunkSize, imageNumber) {
	var xhr = new XMLHttpRequest();
	xhr.open("GET", url, true);
	xhr.responseType = "arraybuffer";
	xhr.onload = function() {
		if (xhr.status == 200 && xhr.response) {
			var data = new Uint8Array(xhr.response);
			var bitmap = [];
			for (var i = 0; i < data.byteLength; i++) {
				bitmap.push(data[i]);
			}

			sendBitmap(bitmap, chunkSize, imageNumber);

		} else {
			sendFailure(xhr.status, imageNumber);
		}
	};

	xhr.send(null);
}

function getGifImage(url, chunkSize, imageNumber) {
	var xhr = new XMLHttpRequest();
	xhr.open("GET", url, true);
	xhr.responseType = "arraybuffer";
	xhr.onload = function() {
		var data = new Uint8Array(xhr.response || xhr.mozResponseArrayBuffer);
		var gr = new GifReader(data);
		console.log("Gif size : " + gr.width + " " + gr.height);

		var pixels = [];
		gr.decodeAndBlitFrameRGBA(0, pixels);

		var bitmap = convertImage(pixels, 4, gr.width, gr.height);

		sendBitmap(bitmap, chunkSize, imageNumber);
	};

	xhr.send(null);
}

function getJpegImage(url, chunkSize, imageNumber) {
	/*
	var xhr = new XMLHttpRequest();
	xhr.open("GET", url, true);
	xhr.responseType = "arraybuffer";
	xhr.onload = function() {
		var data = new Uint8Array(xhr.response || xhr.mozResponseArrayBuffer);

		try {
			var parser = new JpegDecoder();
			parser.parse(data);

			console.log("Jpeg size : " + parser.width + "x" + parser.height);

			var pixels = parser.getData(parser.width, parser.height);

			var bitmap = convertImage(pixels, 3, parser.width, parser.height);

			sendBitmap(bitmap, chunkSize, imageNumber);

		} catch (ex) {
			sendFailure(ex, imageNumber);
		}
	};

	xhr.send(null);
	*/
	var j = new JpegImage();
	j.onload = function() {
		try {
			var pixels = j.getData(j.width, j.height);
			var bitmap = convertImage(pixels, 3, j.width, j.height);
			sendBitmap(bitmap, chunkSize, imageNumber);
		} catch (ex) {
			sendFailure(ex, imageNumber);
		}
	};

	try {
		j.load(url);
	} catch (ex) {
		sendFailure(ex, imageNumber);
	}

}

function getPngImage(url, chunkSize, imageNumber) {

	var request = http.get(url, function(response) {
		var data = "";

		response.setEncoding("binary");

		response.on("data", function(chunk) {
			data += chunk;
		});

		response.on("end", function() {
			var buffer = new Buffer(data, "binary");
			new PNG().parse(buffer, function(error, data) {
				if (error) {
					sendFailure("PNG parse error: " + error, imageNumber);

				} else {
					var bitmap = convertImage(data.data, 4, data.width, data.height);
					sendBitmap(bitmap, chunkSize, imageNumber);
				}
			});
		});
	});

	request.on("error", function(error) {
		sendFailure(error, imageNumber);
	});

	request.setTimeout(DOWNLOAD_TIMEOUT, function() {
		sendFailure("timeout", imageNumber);
	});
}

function endsWith(str, suffix) {
	return str.indexOf(suffix, str.length - suffix.length) !== -1;
}

function getImage(url, chunkSize) {
	try {
		console.log("Image URL : " + url);

		imageNumber++;

		var extension = url.toLowerCase().split(".").pop();

		xhrTimeout = setTimeout(function() {
			sendFailure("timeout", imageNumber);
		}, DOWNLOAD_TIMEOUT);

		switch (extension) {
			case "pbi":
				getPbiImage(url, chunkSize, imageNumber);
				break;

			case "gif":
				getGifImage(url, chunkSize, imageNumber);
				break;

			case "jpg":
			case "jpeg":
				getJpegImage(url, chunkSize, imageNumber);
				break;

			case "png":
				getPngImage(url, chunkSize, imageNumber);
				break;

			default:
				getJpegImage(url, chunkSize, imageNumber);
				break;
		}
	} catch (ex) {
		sendFailure(ex, imageNumber);
	}
}