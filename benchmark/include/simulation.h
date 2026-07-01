#pragma once
#include "TreeNSearch"
#include "point_set.h"
#include <random>
class Simulation {
private:
    PointSet point_set;
    
public:
    // DataSet: RANDOM, SIMPLE...
    void SetDataSet(DataSet data_set, int num_of_points);

    // Method Comparison
    double RunBruteForce();
    double RunTreeNSearch();

};