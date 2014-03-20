import SimpleHTTPServer, SocketServer
import urlparse
import BaseHTTPServer
import socket
from urllib import urlretrieve
from PIL import Image
from bitmapgen import *
import thread

mixin = SocketServer.ThreadingMixIn

PORT = 2635

image = 0

class RebbleServer(mixin, BaseHTTPServer.HTTPServer):

	def __init__(self):
		global PORT
		SocketServer.TCPServer.__init__(self, ('', PORT), MyHandler)

	def server_bind(self):
		if hasattr(socket, 'SOL_SOCKET') and hasattr(socket, 'SO_REUSEADDR'):
			self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
			BaseHTTPServer.HTTPServer.server_bind(self)

class MyHandler(SimpleHTTPServer.SimpleHTTPRequestHandler):
	def do_GET(self):
		global image

		# Parse query data & params to find out what was passed
		parsedParams = urlparse.urlparse(self.path)
		queryParsed = urlparse.parse_qs(parsedParams.query)

		if "url" in queryParsed:
			url = queryParsed["url"]

			filename = str(image) + ".jpg"

			try:
				url = url[0]

				#print url

				urlretrieve(url, filename)

				img = Image.open(filename)

				#Scale
				dims = (144, 168)
				#print 'resizing from ', img.size, ' to dims ', dims

				img.thumbnail(dims, Image.ANTIALIAS)
				#img = img.resize(dims, Image.ANTIALIAS)
				#print 'new size: ', img.size

				img = img.convert('1')

				#img = img.convert('P')
				#img = img.point(lambda p: p > 127 and 255, '1')

				img.save(str(image) + ".png")

				pb = PebbleBitmap(str(image) + ".png")
				#pb.convert_to_pbi(str(image) + ".pbi")

				self.send_response(200)
				self.send_header('Content-Type', 'application/text')
				self.end_headers()

				self.wfile.write(pb.pbi_header())
				self.wfile.write(pb.image_bits())
				self.wfile.close();
	
			except Exception, e:
				#print "nooo"
				#print e 
				self.send_response(404)
				self.end_headers()

			try:
				os.remove(filename)
				os.remove(str(image) + ".png")

			except Exception, e:
				pass

			image += 1

		print queryParsed

	def processMyRequest(self, query):

		self.send_response(200)
		self.send_header('Content-Type', 'image/pbi')
		self.end_headers()

		self.wfile.write("<?xml version='1.0'?>");
		self.wfile.write("<sample>Some XML</sample>");
		self.wfile.close();

httpd = RebbleServer();

print "serving at port", PORT
httpd.serve_forever()
