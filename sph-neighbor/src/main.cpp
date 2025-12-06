#include <iostream>
#include <Eigen/Dense>
#include <omp.h>
#include <fstream>
#include <random>
#include "Particle.h"
#include "UniformGrid.h"
#include "algorithm.h"
int main() {
    Algorithm algo;
    // 建立粒子資料
    std::vector<Particle> particles(10000);


    algo.initGalaxyParticles(particles);
    //-----------------------------------------------------------------
    // 設定 uniform grid 配置
    GridConfig cfg;
    cfg.cellSideLength = 5;  
    cfg.origin = Eigen::Vector3d(0, 0, 0);
    cfg.nx = 100;
    cfg.ny = 100;
    cfg.nz = 100;

    // 建立 Grid
    UniformGrid grid(cfg);

    // 建立空間索引
    grid.rebuildIndex(particles);

    // 搜尋鄰居資料結構
    std::vector<std::vector<int>> neighbors;

    // 搜尋半徑
    double search_r = 0.5;

    grid.findNeighbors(particles, neighbors, search_r);

    // 印出每個粒子的鄰居
    for (size_t i = 0; i < particles.size(); i++) {
        std::cout << "Particle " << i << " neighbors: ";

        for (int j : neighbors[i]) {
            std::cout << j << " ";
        }
        std::cout << std::endl;
    }


    //-----------------------------------------------------------------
    std::ofstream fout("particles.csv");
    fout << "x,y,z\n";
    for (auto &p : particles) {
        fout << p.position.x() << "," << p.position.y() << "," << p.position.z()<< "\n";
    }
    fout.close();
    return 0;
}
