#include "UniformGrid.h"
#include <cmath>

UniformGrid::UniformGrid(const GridConfig &cfg)
    : cfg_(cfg)
{
    cell_map_.reserve(cfg.nx * cfg.ny * cfg.nz);
}

CellId UniformGrid::cellIdFromCoord(int ix, int iy, int iz) const {
    // 將 (ix, iy, iz) 轉成 64-bit key
    // 注意：此轉換需要保證不溢位且唯一
    // 常用方式：使用固定 multiplier（足夠大）
    const long long p1 = 73856093LL;
    const long long p2 = 19349663LL;
    const long long p3 = 83492791LL;
    return ix * p1 ^ iy * p2 ^ iz * p3;
}

CellId UniformGrid::cellIdFromPos(const Eigen::Vector3d &p) const {
    Eigen::Vector3d rel = p - cfg_.origin;

    int ix = static_cast<int>(std::floor(rel.x() / cfg_.cell_size));
    int iy = static_cast<int>(std::floor(rel.y() / cfg_.cell_size));
    int iz = static_cast<int>(std::floor(rel.z() / cfg_.cell_size));

    return cellIdFromCoord(ix, iy, iz);
}

void UniformGrid::rebuildIndex(const std::vector<Particle> &particles)
{
    cell_map_.clear();

    for (int i = 0; i < (int)particles.size(); i++) {
        CellId cid = cellIdFromPos(particles[i].position);
        cell_map_[cid].push_back(i);
    }
}

void UniformGrid::findNeighbors(const std::vector<Particle> &particles,
                                std::vector<std::vector<int>> &neighbors,
                                double search_radius) const
{
    neighbors.clear();
    neighbors.resize(particles.size());

    // 預先算 cell 計數
    int r = static_cast<int>(std::ceil(search_radius / cfg_.cell_size));

    for (int i = 0; i < (int)particles.size(); i++) {
        const Eigen::Vector3d &pi = particles[i].position;

        // 計算 i 所在 cell
        Eigen::Vector3d rel = pi - cfg_.origin;
        int ix = (int)std::floor(rel.x() / cfg_.cell_size);
        int iy = (int)std::floor(rel.y() / cfg_.cell_size);
        int iz = (int)std::floor(rel.z() / cfg_.cell_size);

        auto &neigh_list = neighbors[i];

        // 搜尋半徑範圍內 cell
        for (int dx = -r; dx <= r; dx++) {
            for (int dy = -r; dy <= r; dy++) {
                for (int dz = -r; dz <= r; dz++) {

                    CellId cid = cellIdFromCoord(ix + dx, iy + dy, iz + dz);

                    auto it = cell_map_.find(cid);
                    if (it == cell_map_.end()) continue;

                    const auto &bucket = it->second;
                    for (int j : bucket) {
                        // 排除自己
                        if (j == i) continue;

                        // 距離測試
                        if ((particles[j].position - pi).norm() <= search_radius) {
                            neigh_list.push_back(j);
                        }
                    }
                }
            }
        }
    }
}
