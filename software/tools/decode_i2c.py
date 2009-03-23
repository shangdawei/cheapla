# a simple I2C protocol decoder

import struct, sys

# CHANGE HERE: bit definitions of SCL, SDA
SCL = 0x400
SDA = 0x2000

class Protocol:
	def __init__(self):
		self.last = None
		self.last_ts = None

	def rising_edge(self, v):
		return not (self.last & v) and (self.this & v)

	def falling_edge(self, v):
		return (self.last & v) and not (self.this & v)

	def high(self, v):
		return self.this & v

	def low(self, v):
		return not (self.this & v)

	def handleSample(self, data, timestamp):
		self.this = data
		self.timestamp = data
		if self.last is not None:
			self.tick()
		self.last = data
		self.last_ts = timestamp

	def loopFile(self, file):
		while True:
			r = file.read(8)
			if len(r) != 8:
				break
			(this, timestamp) = struct.unpack("<LL", r)
			
			p.handleSample(this, timestamp)

class I2CDecoder(Protocol):
	def tick(self):
		#print self.high(SCL) and "1" or "0", self.high(SDA) and "1" or "0", self.timestamp - self.last_ts
		if self.falling_edge(SDA) and self.high(SCL): 
			print "START"
			self.bits = 0
			self.data = 0
		if self.rising_edge(SDA) and self.high(SCL): print "STOP"
		if self.rising_edge(SCL):
	#		print high(SDA) and "1" or "0",
			self.bits += 1
			if self.bits == 9:
				print "%02x" % self.data,
				if self.high(SDA):
					print "(NACK)"
				self.data = 0
				self.bits = 0
			else:
				self.data <<= 1
				if self.high(SDA):
					self.data |= 1

p = I2CDecoder()

p.loopFile(sys.stdin)
