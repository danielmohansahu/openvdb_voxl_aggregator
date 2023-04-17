/* Simple ground plane extraction operation.
 *
 * Ground plane extraction operations.
 */

// STL
#include <concepts>
#include <optional>

// OpenVDB
#include <openvdb/points/PointAttribute.h>

// NanoVDB
#include <nanovdb/util/OpenToNanoVDB.h>
#include <nanovdb/util/CudaDeviceBuffer.h>

// OVM
#include <openvdb_voxel_mapper/operations/ground_plane_extraction.h>

namespace ovm::ops
{

std::optional<Map> ground_plane_extraction_geometric(const openvdb::points::PointDataGrid::Ptr& grid)
{
  using namespace openvdb::points;
  
  // sanity check inputs
  if (!grid || grid->empty())
    return std::nullopt;

  // initialize output map dimensions and pose from the grid's bounding box
  const auto bbox = grid->evalActiveVoxelBoundingBox();
  ovm::Map result {bbox, grid->transform()};

  // Iterate over all the leaf nodes in the grid.
  for (auto leaf = grid->tree().cbeginLeaf(); leaf; ++leaf)
  {
    // Create a read-only AttributeHandle. Position always uses Vec3f.
    const AttributeHandle<openvdb::Vec3f> handle(leaf->constAttributeArray("P"));

    // Iterate over the point indices in the leaf.
    for (auto idx = leaf->beginIndexOn(); idx; ++idx)
    {
      // Extract the voxel-space position of the point.
      const openvdb::Vec3f voxelPosition = handle.get(*idx);

      // Extract the index-space position of the voxel.
      const openvdb::Vec3d xyz = idx.getCoord().asVec3d();

      // Compute the world-space position of the point.
      const openvdb::Vec3f worldPosition = grid->transform().indexToWorld(voxelPosition + xyz);
      
      // update 2D map value with the lowest Z value found so far
      const auto coords = xyz - bbox.min();
      auto& val = result.map.coeffRef(coords.y(), coords.x());
      val = (std::isnan(val)) ? worldPosition.z() : std::min(val, worldPosition.z());
    }
  }

  return result;
}

extern "C" void launch_ground_plane_kernel(const nanovdb::NanoGrid<uint32_t>*,
                                           const nanovdb::NanoGrid<uint32_t>*,
                                           cudaStream_t stream,
                                           ovm::Map::MapT& result);

std::optional<Map> ground_plane_extraction_geometric_cuda(const openvdb::points::PointDataGrid::Ptr& grid)
{
  // sanity check inputs
  if (!grid || grid->empty())
    return std::nullopt;

  // initialize output map dimensions and pose from the grid's bounding box
  ovm::Map result {grid->evalActiveVoxelBoundingBox(), grid->transform()};

  // convert from OpenVDB to NanoVDB grid
  auto handle = nanovdb::openToNanoVDB<nanovdb::CudaDeviceBuffer>(*grid);

  // Create a CUDA stream to allow for asynchronous copy of pinned CUDA memory.
  cudaStream_t stream;
  cudaStreamCreate(&stream);

  // Copy the NanoVDB grid to the GPU asynchronously
  handle.deviceUpload(stream, false);

  // get a (raw) pointer to a NanoVDB grid of value type float on the CPU (uint32_t for PointDataGrid)
  auto* grid_cpu = handle.grid<uint32_t>();
  // get a (raw) pointer to a NanoVDB grid of value type float on the GPU (uint32_t for PointDataGrid)
  auto* grid_gpu = handle.deviceGrid<uint32_t>();

  // sanity check
  if (!grid_cpu || !grid_gpu)
    throw std::runtime_error("Failed to access grid of expected type!");

  // execute core method on the GPU
  launch_ground_plane_kernel(grid_gpu, grid_cpu, stream, result.map);

  // Destroy the CUDA stream
  cudaStreamDestroy(stream);
  
  // @TODO!
  return std::nullopt;
}

} // namespace ovm::ops