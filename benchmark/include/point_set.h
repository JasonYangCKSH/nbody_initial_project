# pragma once
#include <vector>
#include <array>

class PointSet {
private:
    std::vector<std::array<float, 3>> points;
public:
    PointSet(){}

    void SetPointSet(std::vector<std::array<float, 3>> _points);
};