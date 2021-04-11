#include <galfunc/GeomFunctions.h>

namespace gal {
namespace func {

GAL_FUNC_DEFN(((glm::vec3, vector, "3D vector")),
              vec3,
              true,
              3,
              "Creates a 3D vector from coordinates",
              (float, x, "x coordinate"),
              (float, y, "y coordinate"),
              (float, z, "z coordinate"))
{
  return std::make_tuple(std::make_shared<glm::vec3>(*x, *y, *z));
};

GAL_FUNC_DEFN(((gal::Sphere, sphere, "Sphere")),
              sphere,
              true,
              2,
              "Creates a new sphere",
              (glm::vec3, center, "Center"),
              (float, radius, "Radius"))
{
  return std::make_tuple(std::make_shared<gal::Sphere>(gal::Sphere {*center, *radius}));
};

}  // namespace func
}  // namespace gal