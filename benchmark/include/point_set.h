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
    float particle_radius;
    float search_radius;
    void GenerateRandom(int points_num, float range_min = -1.0f, float range_max = 1.0f);
public:
    PointSet(){}
    
    ~PointSet(){}
    
    
    
    void SetPointSet(DataSet data_set, int num_of_points);
    int GetPointSetSize() {return this->points.size();}
    const std::vector<std::array<float, 3>>& GetPoints() const { return this->points;}
    float GetSearchRadius() const { return search_radius; }
};