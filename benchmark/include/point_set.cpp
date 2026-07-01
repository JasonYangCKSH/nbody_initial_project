#include <vector>
#include <array>
#include <random>
#include "point_set.h"

void PointSet::GenerateRandom(int points_num, float range_min = -1.0f, float range_max = 1.0f) {
    this->points.resize(points_num);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(range_min, range_max);
    for (int i = 0; i < n_points; i++) {
        points[i][0] = dist(rng);  // x
        points[i][1] = dist(rng);  // y
        points[i][2] = dist(rng);  // z
    }


}
void PointSet::SetPointSet(DataSet data_set, int num_of_points) {
    switch(data_set) {
        case RANDOM:
            GenerateRandom(num_of_points);
            break;
        default:
            break;

    }
}