#include <galcore/ConvexHull.h>
#include <galfunc/GeomFunctions.h>

namespace gal {
namespace func {

GAL_FUNC_DEFN(vec3,
              3,
              1,
              "Creates a 3D vector from coordinates",
              ((float, x, "x coordinate"),
               (float, y, "y coordinate"),
               (float, z, "z coordinate")),
              ((glm::vec3, vector, "3D vector")))
{
  vector->x = *x;
  vector->y = *y;
  vector->z = *z;
};

GAL_FUNC_DEFN(vec2,
              2,
              1,
              "Creates a 2D vector from coordinates",
              ((float, x, "x coordinate"), (float, y, "y coordinate")),
              ((glm::vec2, vector, "2D vector")))
{
  vector->x = *x;
  vector->y = *y;
};

GAL_FUNC_DEFN(plane,
              2,
              1,
              "Creates a plane with the given point and normal",
              ((glm::vec3, point, "Point"), (glm::vec3, normal, "Normal")),
              ((gal::Plane, plane, "The plane")))
{
  plane->origin(*point);
  plane->normal(*normal);
};

GAL_FUNC_DEFN(box3,
              2,
              1,
              "Creates a 3d box with the two given points",
              ((glm::vec3, min, "min point"), (glm::vec3, max, "max point")),
              ((gal::Box3, box, "Box")))
{
  box->min = *min;
  box->max = *max;
};

GAL_FUNC_DEFN(box2,
              2,
              1,
              "Creates a 2d box with the two given points",
              ((glm::vec2, min, "min point"), (glm::vec2, max, "max point")),
              ((gal::Box2, box, "Box")))
{
  box->min = *min;
  box->max = *max;
};

GAL_FUNC_DEFN(randomPointCloudFromBox,
              2,
              1,
              "Creates a random point cloud with points inside the given box",
              ((gal::Box3, box, "Box to sample from"),
               (int32_t, numPoints, "Number of points to sample")),
              ((gal::PointCloud, cloud, "Point cloud")))
{
  size_t nPts = size_t(*numPoints);
  cloud->reserve(nPts);
  box->randomPoints(nPts, std::back_inserter(*cloud));
};

GAL_FUNC_DEFN(pointCloudConvexHull,
              1,
              1,
              "Creates a convex hull from the given point cloud",
              ((gal::PointCloud, cloud, "Point cloud")),
              ((gal::Mesh, hull, "Convex hull")))
{
  *hull = gal::ConvexHull(cloud->begin(), cloud->end()).toMesh();
};

GAL_FUNC_DEFN(pointCloud3d,
              1,
              1,
              "Creates a point cloud from the list of points",
              ((std::vector<glm::vec3>, points, "points")),
              ((gal::PointCloud, cloud, "Point cloud")))
{
  cloud->resize(points->size());
  std::copy(points->begin(), points->end(), cloud->begin());
};

}  // namespace func
}  // namespace gal
