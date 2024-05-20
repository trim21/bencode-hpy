from setuptools import setup, Extension

import os.path
import re

CLASSIFIERS = [
    "Development Status :: 5 - Production/Stable",
    "Intended Audience :: Developers",
    "License :: OSI Approved :: BSD License",
    "Programming Language :: C",
    "Programming Language :: Python :: 3",
    "Programming Language :: Python :: 3.10",
    "Programming Language :: Python :: 3.11",
    "Programming Language :: Python :: 3.12",
]

module1 = Extension(
    "_bencode",
    sources=[
        "./src/bencode3/bencode.c",
        "./src/bencode3/decode.c",
        "./src/bencode3/encode.c",
    ],
    include_dirs=["./src/bencode3"],
    extra_compile_args=["-D_GNU_SOURCE"],
)


def get_version():
    filename = os.path.join(os.path.dirname(__file__), "./src/bencode3/version.h")
    file = None
    try:
        file = open(filename)
        header = file.read()
    finally:
        if file:
            file.close()
    m = re.search(r'#define\s+BENCODE_VERSION\s+"(\d+\.\d+(?:\.\d+)?)"', header)
    assert m, "version.h must contain BENCODE_VERSION macro"
    return m.group(1)


setup(
    name="bencode-hpy",
    setup_requires=["hpy>=0.9.0,<1.0.0"],
    py_modules=[],
    version=get_version(),
    description="Ultra fast JSON encoder and decoder for Python",
    hpy_ext_modules=[module1],
    author="Jonas Tarnstrom",
    author_email="jonas.tarnstrom@esn.me",
    download_url="http://github.com/esnme/ultrajson",
    license="BSD License",
    platforms=["any"],
    url="http://www.esn.me",
    classifiers=CLASSIFIERS,
)
