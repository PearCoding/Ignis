from common import create_flat_scene, compute_scene_average
import pytest


def test_no_light():
    # Render a scene without light sources
    scene = create_flat_scene()
    value = compute_scene_average(scene)
    assert value == pytest.approx(0, 1e-8)


def test_point_light():
    # The surface is centered at (0,0,0) with width and height equal 2
    # Let the point light be at (0,0,-2) and the surface normal be (0,0,-1)
    # The problem simplifies to a symmetrical domain with four [0,1]^2 integration quartets
    # As only the average of the scene is asked for, the four is dropped due to the area of the plane being 4
    # Therefore, the solution is int(x,0,1, int(y,0,1, cos(theta)/dist^2) = int(x,0,1, int(y,0,1, 2/sqrt(x^2+y^2+4))^3) = atan(1/sqrt(24)) = 0.20135792
    # The light source emits 1 to every direction. In solid domain this is 1/(4pi). Also the pure white lambertian bsdf factor 1/pi can not be neglected
    # Therefore, the actual scene average is 0.005100456
    scene = create_flat_scene()
    scene["lights"].append(
        {"type": "point", "name": "_light", "position": [0, 0, -2], "intensity": [1, 1, 1]})
    value = compute_scene_average(scene)
    assert value == pytest.approx(0.005100456, 1e-4)


def test_spot_light():
    # Similar situation as for the point lights.
    # The cutoff and falloff is chosen such that no black regions appear.
    # Only a scaling is necessary, which is given by  4pi / (2pi * (1 - cos(cutoffAngle))) = 2/(1 - 1/sqrt(2)) = 6.82842712474
    # Therefore, the actual scene average is 0.005100456 * 6.82842712474 = 0.0348280920
    scene = create_flat_scene()
    scene["lights"].append(
        {"type": "spot", "name": "_light", "cutoff": 45, "falloff": 45, "position": [0, 0, -2], "direction": [0, 0, 1], "intensity": [1, 1, 1]})
    value = compute_scene_average(scene)
    assert value == pytest.approx(0.0348280920, 1e-4)


def test_env_light():
    scene = create_flat_scene()
    scene["lights"].append(
        {"type": "env", "name": "_light", "radiance": [1, 1, 1]})
    value = compute_scene_average(scene)
    assert value == pytest.approx(1, 1e-4)
