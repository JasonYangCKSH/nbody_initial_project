#pragma once
#include "TreeNSearch"
#include "point_set.h"
#include <random>
class Simulation {
private:
    PointSet point_set;
    
public:
    // DataSet
    void SetDataSet();

    // Method Comparison
    tns::NeighborList RunBruteForce();
    tns::NeighborList RunTreeNSearch();

};