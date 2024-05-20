import hashlib
from pathlib import Path

import bencode3


def test_get_torrent_info_hash():
    with Path(__file__).joinpath(
        "../fixtures/ubuntu-22.04.2-desktop-amd64.iso.torrent.bin"
    ).resolve().open("rb") as f:
        data = bencode3.bdecode(f.read(), str_key=True)

        assert (
            hashlib.sha1(bencode3.bencode(data["info"])).hexdigest()
            == "a7838b75c42b612da3b6cc99beed4ecb2d04cff2"
        )
