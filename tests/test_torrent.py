import hashlib
from pathlib import Path

import bencode_hpy


def test_get_torrent_info_hash():
    with Path(__file__).joinpath(
        "../fixtures/ubuntu-22.04.2-desktop-amd64.iso.torrent.bin"
    ).resolve().open("rb") as f:
        data = bencode_hpy.bdecode(f.read())

        # print(data)

        assert (
            hashlib.sha1(bencode_hpy.bencode(data[b"info"])).hexdigest()
            == "a7838b75c42b612da3b6cc99beed4ecb2d04cff2"
        )
