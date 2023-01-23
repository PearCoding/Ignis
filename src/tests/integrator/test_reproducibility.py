from common import create_flat_scene, compute_scene
import numpy as np


def test_basic_reproducibility():
    scene = create_flat_scene()
    scene["lights"].append(
        {"type": "point", "name": "_light", "position": [0, 0, -2], "intensity": [1, 1, 1]})
    img1 = compute_scene(scene, spp=1, spi=1, seed=42)
    img2 = compute_scene(scene, spp=1, spi=1, seed=42)
    np.testing.assert_array_equal(img1, img2)


def test_basic_reproducibility_spi_independent():
    scene = create_flat_scene()
    scene["lights"].append(
        {"type": "point", "name": "_light", "position": [0, 0, -2], "intensity": [1, 1, 1]})
    img1 = compute_scene(scene, spp=1, spi=1, seed=42)
    img2 = compute_scene(scene, spp=4, spi=4, seed=42)
    assert not np.allclose(img1, img2)
