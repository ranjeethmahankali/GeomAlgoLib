import pygalfunc as pgf
import pygalview as pgv

minCoord, = pgf.numberf32(-1.)
maxCoord, = pgf.numberf32(1.)
minpt, = pgf.vec3(minCoord, minCoord, minCoord)
maxpt, = pgf.vec3(maxCoord, maxCoord, maxCoord)
box, = pgf.box3(minpt, maxpt)

npts, = pgv.slideri32("Point count", 5, 50, 25)

cloud, = pgf.randomPointCloudFromBox(box, npts)
sphere, *_ = pgf.boundingSphere(cloud)

pgv.show("cloud", cloud)
pgv.show("sphere", sphere)
