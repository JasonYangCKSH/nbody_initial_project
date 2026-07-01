# pragma once
#include <vector>
#include <array>
typedef enum DataSet{
    RANDOM,
    SIMPLE
} DataSet;
class PointSet {
private:
    std::vector<std::array<float, 3>> points;
    void GenerateRandom(int points_num, float range_min = -1.0f, float range_max = 1.0f);
public:
    PointSet(){}
    
    ~PointSet(){}
    
    
    
    void SetPointSet(DataSet data_set, int num_of_points);

};