#include "TreeNSearch"
#include <vector>
#include <array>
#include <iostream>
#include <random>
int main() {
  
    const int n_points = 13999;
    const float box_size = 10.0f;

    std::cout << "Step 1: generating points\n" ;

    std::mt19937 rng(42);  
    std::uniform_real_distribution<float> dist(0.0f, box_size);

    std::vector<std::array<float, 3>> points(n_points);
    for (int i = 0; i < n_points; i++) {
        points[i] = { dist(rng), dist(rng), dist(rng) };
    }
    //std::vector<std::array<float, 3>> points = {{0.0f, 0.5f, 0.5f},
    //                                            {1.0f, 1.0f, 1.0f},
    //                                            {0.3f, 0.3f, 0.3f},
    //                                            {0.7f, 0.7f, 0.7f},
    //                                            {0.9f, 0.9f, 0.9f}};
    const float radius = 1.0f;
    tns::TreeNSearch nsearch;
   
    nsearch.set_search_radius(radius);
  
    const int set_0 = nsearch.add_point_set(points[0].data(), points.size());
  
    nsearch.set_active_search(set_0, set_0);
  
    nsearch.run();
  
    /*for (int i = 0; i < points.size(); i++) {
        //std::cout << "---\n";
        const tns::NeighborList neighborlist = nsearch.get_neighborlist(set_0, set_0, i);
        //std::cout << "yoyo\n";
        std::cout << "Point " << i << " has " << neighborlist.size() << " neighbors: ";

        for (int loc_j = 0; loc_j < neighborlist.size(); loc_j++) {
            std::cout << neighborlist[loc_j] << " ";
        }
        std::cout << "\n";
    }*/

    std::cout << "TreeNSearch linked and ran successfully.\n";
    return 0;
    
}