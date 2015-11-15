var DOWNLOAD_TIMEOUT = 20000;

function sendBitmap(bitmap, chunkSize){
  sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_BEGIN": bitmap.length});

  for(var i=0; i < bitmap.length; i += chunkSize){
    sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_DATA": bitmap.slice(i, i + chunkSize)});
  }

  sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_END": "done"});
}

function convertImage(rgbaPixels, numComponents, width, height){

  var watch_info;
  if(Pebble.getActiveWatchInfo) {
    watch_info = Pebble.getActiveWatchInfo() || { 'platform' : 'aplite'};
  } else {
    watch_info = { 'platform' : 'aplite'};
  }

  var ratio = Math.min(144 / width,168 / height);
  var ratio = Math.min(ratio,1);

  var final_width = Math.floor(width * ratio);
  var final_height = Math.floor(height * ratio);
  var final_pixels = [];
  var bitmap = [];

  if(watch_info.platform === 'aplite') {
    var grey_pixels = greyScale(rgbaPixels, width, height, numComponents);
    ScaleRect(final_pixels, grey_pixels, width, height, final_width, final_height, 1);
    floydSteinberg(final_pixels, final_width, final_height, pebble_nearest_color_to_black_white);
    bitmap = toPBI(final_pixels, final_width, final_height);
  }
  else {
    ScaleRect(final_pixels, rgbaPixels, width, height, final_width, final_height, numComponents);
    floydSteinberg(final_pixels, final_width, final_height, pebble_nearest_color_to_pebble_palette);
    var png = generatePngForPebble(final_width, final_height, final_pixels);
    for(var i=0; i<png.length; i++){
      bitmap.push(png.charCodeAt(i));
    }
  }

  return bitmap;
}

function getPbiImage(url, chunkSize){
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.responseType = "arraybuffer";
  xhr.onload = function() {
    if(xhr.status == 200 && xhr.response) {
      clearTimeout(xhrTimeout); // got response, no more need in timeout
      var data = new Uint8Array(xhr.response);
      var bitmap = [];
      for(var i=0; i<data.byteLength; i++) {
        bitmap.push(data[i]);
      }
      sendBitmap(bitmap, chunkSize);
    } else {
      sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_BEGIN": 0});
      sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_END": "done"});
    }
  };

  var xhrTimeout = setTimeout(function() {
    sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_BEGIN": 0});
    sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_END": "done"});
  }, DOWNLOAD_TIMEOUT);

  xhr.send(null);
}

function getGifImage(url, chunkSize){
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.responseType = "arraybuffer";
  xhr.onload = function() {
    clearTimeout(xhrTimeout); // got response, no more need in timeout

    var data = new Uint8Array(xhr.response || xhr.mozResponseArrayBuffer);
    var gr = new GifReader(data);
    console.log("Gif size : "+ gr.width  +" " + gr.height);

    var pixels = [];
    gr.decodeAndBlitFrameRGBA(0, pixels);

    var bitmap = convertImage(pixels, 4, gr.width, gr.height);

    sendBitmap(bitmap, chunkSize);
  };

  var xhrTimeout = setTimeout(function() {
    sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_BEGIN": 0});
    sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_END": "done"});
  }, DOWNLOAD_TIMEOUT);

  xhr.send(null);
}

function getJpegImage(url, chunkSize){
  var j = new JpegImage();
  j.onload = function() {
    clearTimeout(xhrTimeout); // got response, no more need in timeout

    console.log("Jpeg size : " + j.width + "x" + j.height);

    var pixels = j.getData(j.width, j.height);

    var bitmap = convertImage(pixels, 3, j.width, j.height);

    sendBitmap(bitmap, chunkSize);    
  };

  var xhrTimeout = setTimeout(function() {
    sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_BEGIN": 0});
    sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_END": "done"});
  }, DOWNLOAD_TIMEOUT);

  try{
    j.load(url);
  }catch(e){
    console.log("Error : " + e);
  }
}

function getPngImage(url, chunkSize){
  var xhr = new XMLHttpRequest();
  xhr.open("GET", url, true);
  xhr.responseType = "arraybuffer";
  xhr.onload = function() {
    clearTimeout(xhrTimeout); // got response, no more need in timeout

    var data = new Uint8Array(xhr.response || xhr.mozResponseArrayBuffer);

    var png     = new PNG(data);
    var width   = png.width;
    var height  = png.height;
    var palette = png.palette;
    var pixels  = png.decodePixels();
    var bitmap  = [];

    if(palette.length > 0){
      var png_arr = [];
      for(var i=0; i<pixels.length; i++) {
        png_arr.push(palette[3*pixels[i]+0] & 0xFF);
        png_arr.push(palette[3*pixels[i]+1] & 0xFF);
        png_arr.push(palette[3*pixels[i]+2] & 0xFF);
      }
      bitmap = convertImage(png_arr, 3, width, height);
    }
    else {
      var components = pixels.length /( width*height);
      bitmap = convertImage(pixels, components, width, height);
    }

    sendBitmap(bitmap, chunkSize);
  };

  var xhrTimeout = setTimeout(function() {
    sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_BEGIN": 0});
    sendAppMessageEx(NET_IMAGE_QUEUE, {"NETIMAGE_END": "done"});
  }, DOWNLOAD_TIMEOUT);

  xhr.send(null);
}

function endsWith(str, suffix) {
    return str.indexOf(suffix, str.length - suffix.length) !== -1;
}

function getImage(url, chunkSize){
  console.log("Image URL : "+ url);

  if(endsWith(url, ".pbi")){
    getPbiImage(url, chunkSize);
  }
  else if(endsWith(url, ".gif") || endsWith(url, ".GIF")){
    getGifImage(url, chunkSize);
  }
  else if(endsWith(url, ".jpg") || endsWith(url, ".jpeg") || endsWith(url, ".JPG") || endsWith(url, ".JPEG")){
    getJpegImage(url, chunkSize);
  }
  else if(endsWith(url, ".png") || endsWith(url, ".PNG")){
    getPngImage(url, chunkSize);
  }
  else {
    getJpegImage(url, chunkSize);
  }
}