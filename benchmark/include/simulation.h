#pragma once
#include "TreeNSearch"
#include "point_set.h"
class Simulation {
private:
    PointSet point_set;
    tns::NeighborList nsearch;
public:
    // DataSet
    void GenerateRandom(int points_num);

    // Method Comparison
    tns::NeighborList RunBruteForce();
    tns::NeighborList RunTreeNSearch();

};