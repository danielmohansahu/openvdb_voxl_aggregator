/* types.h
 *
 * Commonly used type definitions.
 */

#pragma once

// Eigen
#include <Eigen/Core>

// OpenVDB
#include <openvdb/math/Coord.h>
#include <openvdb/math/Transform.h>

namespace ovm
{

// project configuration options
struct Options
{
  float voxel_size {0.5};               // voxel size, in meters
  float default_confidence {1.0};       // confidence, if unsupplied
  size_t default_label {0};             // label, if unsupplied
  size_t free_label {0};                // label for free space, e.g. ray trace cleared
  std::vector<size_t> ground_labels {}; // labels considered 'ground' for operations
}; // struct Options

// Axis-aligned map structure used as the final output of Mapping.
struct Map
{
  // typedefs
  typedef Eigen::MatrixXf MapT;
  typedef Eigen::Vector2f PoseT;
  
  MapT map;   // map of arbitrary float values
  PoseT pose; // XY origin of the map

  // convenience constructor from an openvdb bounding box
  Map(const openvdb::CoordBBox& bbox, const openvdb::math::Transform& tf)
  {
    // get dimensions and origin of grid bounding box
    const auto dimensions = bbox.dim();
    const auto origin = tf.indexToWorld(bbox.min());
    map = MapT::Constant(dimensions.y(), dimensions.x(), NAN);
    pose = PoseT {origin.x(), origin.y()};
  }

  // maintain default constructor
  Map() = default;
}; // struct Map

} // namespace ovm
