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
    void GenerateRandom(int points_num);
public:
    PointSet(){}
    ~PointSet(){}
    
    
    
    void SetPointSet(DataSet data_set);

};