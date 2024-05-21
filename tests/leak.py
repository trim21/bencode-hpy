import sys
import time
import tracemalloc
import gc

from bencode_hpy import BencodeEncodeError, bencode


s = tracemalloc.start()


data = [{"1": 1}] * 5000

while True:
    for c in data:
        try:
            bencode(c)
        except BencodeEncodeError:
            pass

    gc.collect()
    v = tracemalloc.get_tracemalloc_memory()
    print(v)
    if v > 10610992 * 2:
        time.sleep(1000)
        sys.exit(1)
