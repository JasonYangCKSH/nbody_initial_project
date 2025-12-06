#include <iostream>
#include <Eigen/Dense>
#include <omp.h>
#include <fstream>
#include <random>
#include "Particle.h"
#include "UniformGrid.h"
#include "algorithm.h"

// 產生銀河分佈的隨機粒子
void initGalaxyParticles(std::vector<Particle> &particles) {
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    std::normal_distribution<double> gauss(0.0, 1.0);

    const double diskRadius = 5.0;
    const double diskHeight = 0.1;
    const double bulgeRadius = 1.0;

    for (Particle &p : particles) {
        double choose = uni(rng);

        // 80% 的粒子在盤面
        if (choose < 0.8) {
            // radius 取指數衰減：r = -R * log(U)
            double R = diskRadius * (-std::log(uni(rng)));
            double theta = uni(rng) * 2.0 * M_PI;

            double x = R * std::cos(theta);
            double y = R * std::sin(theta);
            double z = gauss(rng) * diskHeight;

            p.setState(Eigen::Vector3d(x, y, z), Eigen::Vector3d(0,0,0));
        }
        // 20% 的粒子在核球
        else {
            // 核球：近似三維高斯
            double x = gauss(rng) * bulgeRadius * 0.5;
            double y = gauss(rng) * bulgeRadius * 0.5;
            double z = gauss(rng) * bulgeRadius * 0.5;

            p.setState(Eigen::Vector3d(x, y, z), Eigen::Vector3d(0,0,0));
        }
    }
}

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
