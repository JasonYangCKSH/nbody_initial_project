#ifndef NEIGHBOR_SEARCH_HPP
#define NEIGHBOR_SEARCH_HPP
#include<glm/glm.hpp>
#include "Barnes-HutOctree.hpp"
#include "body.hpp"
enum class NeighborMethod {
    BRUTE_FORCE,
    UNIFORM_GRID,
    OCTREE,
    NONE
};
class NeighborSearch {
private:
    NeighborMethod method;
public:
    NeighborSearch(): method(NONE){}
    NeighborSearch(NeighborMetHod nm): method(nm) {}
};
#endif 