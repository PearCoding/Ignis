from common import load_api, compute_scene_average
import pytest


def test_load_api():
    assert load_api() is not None


def test_empty_scene():
    # Render a scene without any geometry
    value = compute_scene_average({})
    assert value == pytest.approx(0, 1e-8)
