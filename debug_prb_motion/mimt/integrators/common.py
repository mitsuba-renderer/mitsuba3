import drjit as dr
import mitsuba as mi

def det_over_det(D):
    return dr.select(D != 0, dr.replace_grad(1, D/dr.detach(D)), 0)

def solid_to_surface_reparam_det(si: mi.SurfaceInteraction3f, x_prev: mi.Point3f, active: mi.Bool = True):
    """ Reparameterization determinant from solid angles to surface elements
    """

    d = si.p - x_prev

    distance_squared = dr.squared_norm(d)
    cos_theta = dr.abs_dot(si.n, -dr.normalize(d))

    # If the intersection point lies on an environment emitter, 
    # the surface parameterization is invalid (and si.is_valid() == False)
    det = dr.select(active & si.is_valid() & (distance_squared > 0), 
                    dr.norm(dr.cross(si.dp_du, si.dp_dv)) * dr.abs(cos_theta) / distance_squared, 1)

    return det

def sensor_to_solid_reparam_det(sensor: mi.Sensor, si: mi.SurfaceInteraction3f, ignore_near_plane: bool):
    """ Reparameterization determinant from sensor elements to solid angles 
        (for perspective cameras)
    """
    sensor_pos = sensor.world_transform() @ mi.Point3f(0)
    sensor_dir = sensor.world_transform() @ mi.Vector3f(0, 0, 1)
    
    # Jacobian determinant (sensor to solid angle)
    cos_phi = dr.abs_dot(dr.normalize(si.p - sensor_pos), sensor_dir)

    # If the sensor is non-differentiable and the resulting determinant D is used in a 
    # ratio D/detach(D), then the near plane factor cancels, so it can be omitted here.
    # (this should improve numerical stability for very close near planes)
    near_factor = 1 if ignore_near_plane else dr.square(sensor.near_clip())

    return dr.select(si.is_valid(), near_factor / cos_phi*cos_phi*cos_phi, 1.)

def sensor_to_surface_reparam_det(sensor: mi.Sensor, si: mi.SurfaceInteraction3f, ignore_near_plane: bool, active: mi.Bool = True):
    """ Reparameterization determinant from sensor elements to scene surface elements 
        (for perspective cameras)
    """
    sensor_pos = sensor.world_transform() @ mi.Point3f(0)
    sensor_dir = sensor.world_transform() @ mi.Vector3f(0, 0, 1)

    # The distance between the camera position and the point in the scene
    # cancels in the combined transformation from sensor to surface.
    d = si.p - sensor_pos
    v_dot_d = dr.abs_dot(sensor_dir, d)
    n_dot_d = dr.abs_dot(si.n, -d)

    # If the sensor is non-differentiable and the resulting determinant D is used in a 
    # ratio D/detach(D), then the near plane factor cancels, so it can be omitted here.
    # (this should improve numerical stability for very close near planes)
    near_factor = 1 if ignore_near_plane else dr.square(sensor.near_clip())

    det = near_factor * n_dot_d / (v_dot_d * v_dot_d * v_dot_d) * dr.norm(dr.cross(si.dp_du, si.dp_dv))

    return dr.select(active & si.is_valid(), det, 1.)

def film_to_sensor_reparam_det(sensor: mi.Sensor):
    near_clip = sensor.near_clip()
    x_fov = mi.traverse(sensor)["x_fov"][0]
    film = sensor.film()

    camera_to_sample = mi.perspective_projection(
        film.size(),
        film.crop_size(),
        film.crop_offset(),
        x_fov,
        near_clip,
        sensor.far_clip()
    )

    # The determinant is the ratio of sensor area in "film space"
    # to the sensor area in camera space. The former is 1 because
    # the sensor is a unit square [0, 1]^2 in film space. Assuming
    # that the sensor is centered around the camera's optical axis, 
    # the sensor area in camera space can be determined by measuring
    # the axis-aligned offsets of the top-left film position (0, 0) 
    # in camera space (p_min = inverse(P) * (0, 0, 0, 1)^T):
    #
    # p_min___________ _
    #     |           || abs(p_min[1]
    #     |     ·     |-
    #     |___________|
    #     |-----|
    #  abs(p_min[0])
    # 
    # The sensor area then is
    # 2*abs(p_min[0]) * 2*abs(p_min[1]) = 4 * abs(p_min[0] * p_min[1])
    sample_to_camera = camera_to_sample.inverse()
    p_min = sample_to_camera @ mi.Point3f(0, 0, 0)
    sensor_area = 4 * dr.abs(p_min[0] * p_min[1])
    
    return dr.rcp(sensor_area) # = film_area (=1) / sensor_area
