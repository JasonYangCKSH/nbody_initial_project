#include "algorithm.h"
#include <random>
void Algorithm::initGalaxyParticles(std::vector<Particle> &particles) {
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