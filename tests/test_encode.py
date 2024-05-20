import collections
from typing import Any

import pytest

import bencode_hpy
from bencode_hpy import BencodeEncodeError


def test_exception_when_strict():
    invalid_obj = None
    with pytest.raises(BencodeEncodeError):
        bencode_hpy.bencode(invalid_obj)


def test_encode_str():
    coded = bencode_hpy.bencode("ThisIsAString")
    assert coded == b"13:ThisIsAString", "Failed to encode string from str."


def test_encode_int():
    coded = bencode_hpy.bencode(42)
    assert coded == b"i42e", "Failed to encode integer from int."


def test_encode_bytes():
    b = b"TheseAreSomeBytes"
    coded = bencode_hpy.bencode(b)
    s = bytes(str(len(b)), "utf-8")
    assert coded == s + b":" + b, "Failed to encode string from bytes."


def test_encode_list():
    s = ["a", "b", 3]
    coded = bencode_hpy.bencode(s)
    assert coded == b"l1:a1:bi3ee", "Failed to encode list from list."


def test_encode_tuple():
    t = ("a", "b", 3)
    coded = bencode_hpy.bencode(t)
    assert coded == b"l1:a1:bi3ee", "Failed to encode list from tuple."


def test_encode_dict():
    od = collections.OrderedDict()
    od["kb"] = 2
    od["ka"] = "va"
    coded = bencode_hpy.bencode(od)
    assert coded == b"d2:ka2:va2:kbi2ee", "Failed to encode dictionary from dict."


def test_encode_complex():
    od = collections.OrderedDict()
    od["KeyA"] = ["listitemA", {"k": "v"}, 3]
    od["KeyB"] = {"k": "v"}
    od["KeyC"] = 3
    od["KeyD"] = "AString"
    expected_result = (
        b"d4:KeyAl9:listitemAd1:k1:vei3ee4:KeyBd1:k1:ve4:KeyCi3e4:KeyD7:AStringe"
    )
    coded = bencode_hpy.bencode(od)
    assert coded == expected_result, "Failed to encode complex object."


def test_encode():
    assert bencode_hpy.bencode(
        {
            "_id": "5973782bdb9a930533b05cb2",
            "isActive": True,
            "balance": "$1,446.35",
            "age": 32,
            "eyeColor": "green",
            "name": "Logan Keller",
            "gender": "male",
            "company": "ARTIQ",
            "email": "logankeller@artiq.com",
            "phone": "+1 (952) 533-2258",
            "friends": [
                {"id": 0, "name": "Colon Salazar"},
                {"id": 1, "name": "French Mcneil"},
                {"id": 2, "name": "Carol Martin"},
            ],
            "favoriteFruit": "banana",
        }
    ) == (
        b"d3:_id24:5973782bdb9a930533b05cb23:agei32e7:balance9"
        b":$1,446.357:company5:ARTIQ5:email21:logankeller@artiq.c"
        b"om8:eyeColor5:green13:favoriteFruit6:banana7:friendsld2"
        b":idi0e4:name13:Colon Salazared2:idi1e4:name13:French Mc"
        b"neiled2:idi2e4:name12:Carol Martinee6:gender4:male8:isA"
        b"ctivei1e4:name12:Logan Keller5:phone17:+1 (952) 533-2258e"
    )


def test_duplicated_type_keys():
    with pytest.raises(BencodeEncodeError):
        bencode_hpy.bencode({"string_key": 1, b"string_key": 2, "1": 2})


def test_dict_int_keys():
    with pytest.raises(BencodeEncodeError):
        bencode_hpy.bencode({1: 2})


@pytest.mark.parametrize(
    ["raw", "expected"],
    [
        (["", 1], b"l0:i1ee"),
        ({"": 2}, b"d0:i2ee"),
        ("", b"0:"),
        (True, b"i1e"),
        (False, b"i0e"),
        (-3, b"i-3e"),
        (4927586304, b"i4927586304e"),
        ([b"spam", b"eggs"], b"l4:spam4:eggse"),
        ({b"cow": b"moo", b"spam": b"eggs"}, b"d3:cow3:moo4:spam4:eggse"),
        ({b"spam": [b"a", b"b"]}, b"d4:spaml1:a1:bee"),
        ({}, b"de"),
    ],
)
def test_basic(raw: Any, expected: bytes):
    assert bencode_hpy.bencode(raw) == expected


def test_overflow():
    with pytest.raises(OverflowError):
        # b"i9223372036854775808e",  # longlong int +1
        bencode_hpy.bencode(9223372036854775808)

    with pytest.raises(OverflowError):
        # b"i18446744073709551616e",  # unsigned long long +1
        bencode_hpy.bencode(18446744073709551616)
