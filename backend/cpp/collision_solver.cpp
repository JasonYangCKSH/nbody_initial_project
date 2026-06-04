#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <tuple>
#include <vector>

constexpr double PI = 3.141592653589793238462643383279502884;

struct Particle {
    double x, y, z;
    double vx, vy, vz;
    double ax, ay, az;
    double r, rho, m;
    int color;
    double bright;
};

static double collisionCellSize(const std::vector<Particle>& particles) {
    double maxRadius = 0.0;
    for (const auto& p : particles) maxRadius = std::max(maxRadius, p.r);
    return std::max(1.0, maxRadius * 2.0);
}

static std::tuple<int, int, int> cellKey(const Particle& p, double cellSize) {
    return {
        static_cast<int>(std::floor(p.x / cellSize)),
        static_cast<int>(std::floor(p.y / cellSize)),
        static_cast<int>(std::floor(p.z / cellSize)),
    };
}

static void tryMerge(std::vector<Particle>& particles, std::set<int>& removed, int i, int j, bool& merged) {
    if (removed.count(i) || removed.count(j)) return;

    auto& a = particles[i];
    const auto& b = particles[j];
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double dz = b.z - a.z;
    const double dist2 = dx * dx + dy * dy + dz * dz;
    const double minDist = a.r + b.r;
    if (dist2 > minDist * minDist) return;

    const double mA = a.m;
    const double mB = b.m;
    const double mT = mA + mB;
    if (mT <= 0.0) return;

    const double rho = (a.rho * mA + b.rho * mB) / mT;
    a = {
        (a.x * mA + b.x * mB) / mT,
        (a.y * mA + b.y * mB) / mT,
        (a.z * mA + b.z * mB) / mT,
        (a.vx * mA + b.vx * mB) / mT,
        (a.vy * mA + b.vy * mB) / mT,
        (a.vz * mA + b.vz * mB) / mT,
        0.0, 0.0, 0.0,
        std::cbrt((3.0 * mT) / (4.0 * PI * rho)),
        rho,
        mT,
        mA > mB ? a.color : b.color,
        mA > mB ? a.bright : b.bright,
    };
    removed.insert(j);
    merged = true;
}

static bool mergeCollisions(std::vector<Particle>& particles) {
    if (particles.size() < 2) return false;

    const double cellSize = collisionCellSize(particles);
    std::map<std::tuple<int, int, int>, std::vector<int>> buckets;
    for (int i = 0; i < static_cast<int>(particles.size()); ++i) {
        buckets[cellKey(particles[i], cellSize)].push_back(i);
    }

    bool merged = false;
    std::set<int> removed;
    for (const auto& entry : buckets) {
        const auto [cx, cy, cz] = entry.first;
        const auto& listA = entry.second;
        for (int dz = -1; dz <= 1; ++dz)
        for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const auto keyB = std::make_tuple(cx + dx, cy + dy, cz + dz);
            if (keyB < entry.first) continue;

            auto it = buckets.find(keyB);
            if (it == buckets.end()) continue;
            const auto& listB = it->second;

            if (keyB == entry.first) {
                for (int ia = 0; ia < static_cast<int>(listA.size()); ++ia)
                for (int jb = ia + 1; jb < static_cast<int>(listA.size()); ++jb)
                    tryMerge(particles, removed, listA[ia], listA[jb], merged);
            } else {
                for (int i : listA)
                for (int j : listB)
                    tryMerge(particles, removed, i, j, merged);
            }
        }
    }

    if (merged) {
        std::vector<Particle> kept;
        kept.reserve(particles.size() - removed.size());
        for (int i = 0; i < static_cast<int>(particles.size()); ++i) {
            if (!removed.count(i)) kept.push_back(particles[i]);
        }
        particles.swap(kept);
    }
    return merged;
}

int main() {
    int n = 0;
    if (!(std::cin >> n)) return 1;

    std::vector<Particle> particles;
    particles.reserve(std::max(0, n));
    for (int i = 0; i < n; ++i) {
        Particle p{};
        if (!(std::cin >> p.x >> p.y >> p.z >> p.vx >> p.vy >> p.vz >> p.ax >> p.ay >> p.az
                  >> p.r >> p.rho >> p.m >> p.color >> p.bright)) {
            return 2;
        }
        particles.push_back(p);
    }

    const bool merged = mergeCollisions(particles);
    std::cout << (merged ? 1 : 0) << " " << particles.size() << "\n";
    std::cout << std::setprecision(17);
    for (const auto& p : particles) {
        std::cout << p.x << " " << p.y << " " << p.z << " "
                  << p.vx << " " << p.vy << " " << p.vz << " "
                  << p.ax << " " << p.ay << " " << p.az << " "
                  << p.r << " " << p.rho << " " << p.m << " "
                  << p.color << " " << p.bright << "\n";
    }
    return 0;
}
