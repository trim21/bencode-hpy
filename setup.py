from glob import glob

from setuptools import find_packages, setup, Extension

module = Extension(
    "bencode_hpy._bencode",
    sources=glob("./src/bencode_hpy/*.c"),
    include_dirs=["./src/bencode_hpy"],
)

setup(
    setup_requires=["hpy>=0.9.0,<1.0.0", "wheel"],
    hpy_ext_modules=[module],
    packages=find_packages("src"),
    package_dir={"": "src"},
    package_data={"": ["*.h", "*.c", "*.pyi", "py.typed"]},
    platforms=["any"],
)
