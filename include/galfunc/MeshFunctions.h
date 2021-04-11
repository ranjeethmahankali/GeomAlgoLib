#pragma once

#include <galcore/ObjLoader.h>
#include <galfunc/GeomFunctions.h>
#include <galfunc/Types.h>

namespace gal {
namespace func {

GAL_FUNC_DECL(((float, x, "x coordinate"),
               (float, y, "y coordinate"),
               (float, z, "z coordinate")),
              meshCentroid,
              true,
              3,
              "Gets the centroid of a mesh",
              (gal::Mesh, mesh, "The mesh"));

GAL_FUNC_DECL(((gal::Mesh, mesh, "Loaded mesh")),
              loadObjFile,
              true,
              1,
              "Loads a mesh from an obj file",
              (std::string, filepath, "The path to the obj file"));

GAL_FUNC_DECL(((gal::Mesh, mesh, "Input mesh")),
              scaleMesh,
              true,
              2,
              "Scales the mesh. Returns a new instance.",
              (gal::Mesh, mesh, "Scaled mesh"),
              (float, scale, "Scale"));

GAL_FUNC_DECL(((gal::Mesh, mesh, "Clipped mesh")),
              clipMesh,
              true,
              2,
              "Clips the given mesh with the plane. Returns a new mesh.",
              (gal::Mesh, mesh, "mesh to clip"),
              (gal::Plane, plane, "Plane to clip with"));

GAL_FUNC_DECL(((gal::Mesh, resultMesh, "Mesh with the queried faces")),
              meshSphereQuery,
              true,
              2,
              "Queries the mesh face rtree with the given sphere and "
              "returns the new sub-mesh",
              (gal::Mesh, mesh, "Mesh to query"),
              (gal::Sphere, sphere, "Sphere to query the faces with"));

GAL_FUNC_DECL(((gal::PointCloud, outCloud, "Result point cloud")),
              closestPointsOnMesh,
              true,
              2,
              "Creates the result point cloud by closest-point-querying the mesh with "
              "the given point cloud",
              (gal::Mesh, mesh, "Mesh"),
              (gal::PointCloud, inCloud, "Query point cloud"));

GAL_FUNC_DECL(((gal::Box3, bounds, "Bounds of the mesh")),
              meshBbox,
              true,
              1,
              "Gets the bounding box of the mesh",
              (gal::Mesh, mesh, "Mesh"));

}  // namespace func
}  // namespace gal