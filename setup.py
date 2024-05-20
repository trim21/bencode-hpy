from glob import glob

from setuptools import find_packages, setup, Extension

module = Extension(
    "bencode_hpy._bencode",
    sources=glob("./src/bencode_hpy/*.c"),
    include_dirs=["./src/bencode_hpy"],
)

setup(
    name="bencode-hpy",
    setup_requires=[
        'hpy>=0.9.0,<1.0.0; implementation_name == "cpython"',
        "wheel",
    ],
    py_modules=[],
    version='0.0.1',
    description="A fast and correct bencode encoder and decoder",
    hpy_ext_modules=[module],
    author="Trim21",
    python_requires=">=3.8,<4",
    packages=find_packages("src"),
    package_dir={"": "src"},
    author_email="trim21.me@gmail.com",
    download_url="http://github.com/trim21/bencode-hpy",
    license="MIT License",
    platforms=["any"],
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: BSD License",
        "Programming Language :: C",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Programming Language :: Python :: 3.12",
    ],
)
