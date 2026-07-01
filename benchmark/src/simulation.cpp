#include "TreeNSearch"
#include "point_set.h"
#include "simulation.h"
#include <random>
#include <chrono>
#include <iostream>
void Simulation::SetDataSet(DataSet data_set, int num_of_points) {
    this->point_set.SetPointSet(data_set, num_of_points);

}
double Simulation::RunBruteForce() {
    const std::vector<std::array<float, 3>>& pts = this->point_set.GetPoints();
    const int point_set_size = this->point_set.GetPointSetSize();
    const float search_radius_square = this->point_set.GetSearchRadius() * this->point_set.GetSearchRadius();
    /*for (int i = 0; i < point_set_size; i++) {
        std::cout << "[" << pts[i][0] << ", " << pts[i][1] << ", " << pts[i][2] << "]\n";
    }*/
    int count = 0;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < point_set_size; i++) {
        for (int j = i + 1; j < point_set_size; j++) {
            float dx = pts[i][0] - pts[j][0];
            float dy = pts[i][1] - pts[j][1];
            float dz = pts[i][2] - pts[j][2];
            float dist2 = dx*dx + dy*dy + dz*dz;
            if (dist2 <= search_radius_square)
                count++;

        }

    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(end - start).count();
}
double Simulation::RunTreeNSearch() {

}