#ifndef UNIFORMGRID_H
#define UNIFORMGRID_H
#include <vector>
#include <unordered_map>
#include "Particle.h"

using CellId = long long;  // 64-bit cell identifier

struct GridConfig {
    double cell_size;  // cell side length
    Eigen::Vector3d origin;  // grid origin position
    int nx, ny, nz;  // cell counts in each dimension
};

class UniformGrid {
private:
    GridConfig cfg_;
    std::unordered_map<CellId, std::vector<int>> cell_map_;  // cell_id -> particle indices
    CellId cellIdFromCoord(int ix, int iy, int iz) const;  // convert (ix, iy, iz) to CellId
    CellId cellIdFromPos(const Eigen::Vector3d &p) const;  // convert position to CellId
public:
    UniformGrid(const GridConfig &cfg);
    void rebuildIndex(const std::vector<Particle> &particles);  // every time when update particle positions
    void findNeighbors(const std::vector<Particle> &particles,
                       std::vector<std::vector<int>> &neighbors, double search_radius) const;

};
#endif