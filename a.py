from hpy.debug import LeakDetector

print((0).to_bytes().decode())

# with LeakDetector():
import _bencode

print(_bencode.encode(b"bb"))

print(_bencode.encode(1))

print(_bencode.decode(b"bb"))

print(_bencode.decode("test"))
