from common import load_api


def test_load_api():
    assert load_api() is not None
