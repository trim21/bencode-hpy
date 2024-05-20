import pytest

from hpy.debug.pytest import hpy_debug
from hpy.debug.pytest import LeakDetector


@pytest.fixture(autouse=True)
def hpy_debug_fixture():
    return hpy_debug


@pytest.fixture(autouse=True)
def hpy_leak():
    with LeakDetector() as ld:
        yield
