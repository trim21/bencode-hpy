from setuptools import find_packages, setup, Extension

import os.path
import re
from glob import glob

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
    "bencode_hpy._bencode",
    sources=glob("./src/bencode_hpy/*.c"),
    include_dirs=["./src/bencode_hpy"],
)


def get_version():
    filename = os.path.join(os.path.dirname(__file__), "./src/bencode_hpy/version.h")
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
    setup_requires=["hpy>=0.9.0,<1.0.0", "wheel"],
    py_modules=[],
    version=get_version(),
    description="A fast and correct bencode encoder and decoder",
    hpy_ext_modules=[module1],
    author="Trim21",
    packages=find_packages("src"),
    package_dir={"": "src"},
    author_email="trim21.me@gmail.com",
    download_url="http://github.com/trim21/bencode-hpy",
    license="MIT License",
    platforms=["any"],
    classifiers=CLASSIFIERS,
)
