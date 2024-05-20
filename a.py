from bencode_hpy import bdecode, bencode

r = bencode(
    {
        "kb": 2,
        "ka": "va",
    }
)

assert r == {b"cow": b"moo", b"spam": b"eggs"}, r

# assert bdecode(b"20:" + r) == r, "not equal"
