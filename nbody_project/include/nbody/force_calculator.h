#pragma once
#include "body.h"
#include <vector>

// 抽象介面：定義「怎麼算力」這件事的統一入口
// 之後 BruteForceCalculator 跟 BarnesHutCalculator 都會繼承這個介面
class ForceCalculator {
public:
        
    
    virtual void computeAccelerations(std::vector<Body> &bodies) = 0;

    virtual ~ForceCalculator() = default;
};