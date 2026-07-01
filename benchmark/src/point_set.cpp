#include <vector>
#include <array>
#include <random>
#include "point_set.h"

void PointSet::GenerateRandom(int points_num, float range_min, float range_max) {
    this->points.resize(points_num);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(range_min, range_max);
    for (int i = 0; i < points_num; i++) {
        points[i][0] = dist(rng);  // x
        points[i][1] = dist(rng);  // y
        points[i][2] = dist(rng);  // z
    }


}
void PointSet::SetPointSet(DataSet data_set, int num_of_points) {
    // same with TreeNSearch
    this->particle_radius = 2.0f / std::pow((float)num_of_points, 1.0f/3.0f);
    this->search_radius = 2.0f * particle_radius;


    switch(data_set) {
        case RANDOM:
            GenerateRandom(num_of_points);
            break;
        default:
            break;

    }
}

