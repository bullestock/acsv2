from cadquery.cq import T
from cadquery import Workplane, Vector, Solid

def _standOff(
        self: T,
        radius: float,
        height: float
) -> T:
    """Standoff

    Returns:
        the shape on the workplane stack with a new standoff
    """
    bore_direction = Vector(0, 0, -1)
    origin = Vector(0, 0, 0)
    standoff_body = Solid.makeCylinder(
        radius=radius,
        height=height,
        pnt=origin,
        dir=bore_direction,
    )
    
    return standoff_body.clean()

Workplane.standOff = _standOff
