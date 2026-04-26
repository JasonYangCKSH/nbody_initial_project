#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include "body.hpp"
#include "NeighborSearchOctree.hpp"
enum class NeighborMethod {
    BRUTE_FORCE,
    UNIFORM_GRID,
    OCTREE

};

struct NeighborPair {
    int i, j;
};
class NeighborSearch {
public:


    NeighborSearch(): method(), cell_size(0.0f){}
    explicit NeighborSearch(NeighborMethod nm, float _cell_size = 0.0f): method(nm), cell_size(_cell_size){}

    std::vector<NeighborPair> FindPairs(const std::vector<Body>& bodies) {
        switch (method) {
            case(NeighborMethod::BRUTE_FORCE):  return BruteForce(bodies);
            case(NeighborMethod::UNIFORM_GRID): return UniformGrid(bodies);
            case(NeighborMethod::OCTREE): return OctreeSearch(bodies);
            default:    return {};
        }
    }

private:
    NeighborMethod method;
    float cell_size;  // uniform_grid 專用

    // 1. Brute Force
    std::vector<NeighborPair> BruteForce(const std::vector<Body>& bodies) {
        if (bodies.empty()) return {};
        std::vector<NeighborPair> pairs;
        for (int i = 0; i < (int)bodies.size(); i++) {
            for (int j = i + 1; j < (int)bodies.size(); j++) {
                float combinedRadius = bodies[i].radius + bodies[j].radius;
                glm::vec3 d = bodies[j].pos - bodies[i].pos;
                float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
                if (dist2 > combinedRadius * combinedRadius) continue;
                pairs.push_back({i, j});
            }
        }
        return pairs;
    }


     // 2. Uniform Grid
    std::vector<NeighborPair> UniformGrid(const std::vector<Body>& bodies) {
        // [cell_size] is important
        if (bodies.empty()) return {};
        this->cell_size = 0.0f;
        std::vector<NeighborPair> pairs;

        // Step1: build Cell and set up Cell size + insert body to cell
        float x_min = bodies[0].pos.x, x_max = x_min;
        float y_min = bodies[0].pos.y, y_max = y_min;
        float z_min = bodies[0].pos.z, z_max = z_min;
        for (const auto& b: bodies) {
            x_min = std::min(x_min, b.pos.x);
            x_max = std::max(x_max, b.pos.x);
            y_min = std::min(y_min, b.pos.y);
            y_max = std::max(y_max, b.pos.y);
            z_min = std::min(z_min, b.pos.z);
            z_max = std::max(z_max, b.pos.z);
            this->cell_size = std::max(this->cell_size, 2.0f * b.radius);
        }
        if (this->cell_size <= 0.0f) throw std::runtime_error("--cell_size went wrong--");
        
        
        // -------------------------------------
        std::vector<std::vector<int>> gridMap;
        // -------------------------------------
        int nx = std::max(1, (int)std::ceil((x_max - x_min) / this->cell_size));
        int ny = std::max(1, (int)std::ceil((y_max - y_min) / this->cell_size));
        int nz = std::max(1, (int)std::ceil((z_max - z_min) / this->cell_size));
        gridMap.resize(nx * ny * nz);
       
        for (int i = 0; i < (int)bodies.size(); i++){
           
            float posX = bodies[i].pos.x - x_min;
            float posY = bodies[i].pos.y - y_min;
            float posZ = bodies[i].pos.z - z_min;
            int cx = std::max(0, std::min((int)std::floor(posX / cell_size), nx - 1));
            int cy = std::max(0, std::min((int)std::floor(posY / cell_size), ny - 1));
            int cz = std::max(0, std::min((int)std::floor(posZ / cell_size), nz - 1));
    
            // insert
            int index = cx * ny * nz + cy * nz + cz;
            gridMap[index].push_back(i);
        }
        // Step2: find neighbor cell, and neighbor body
        for (int i = 0; i < (int)bodies.size(); i++) {
            float posX = bodies[i].pos.x - x_min;
            float posY = bodies[i].pos.y - y_min;
            float posZ = bodies[i].pos.z - z_min;
            int cx = std::max(0, std::min((int)std::floor(posX / cell_size), nx - 1));
            int cy = std::max(0, std::min((int)std::floor(posY / cell_size), ny - 1));
            int cz = std::max(0, std::min((int)std::floor(posZ / cell_size), nz - 1));
            for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
            for (int dz = -1; dz <= 1; dz++) {
                if (cx + dx < 0 || cx + dx >= nx) continue;
                if (cy + dy < 0 || cy + dy >= ny) continue;
                if (cz + dz < 0 || cz + dz >= nz) continue;
                //todo
                int neighbor_index = (cx + dx) * ny * nz + (cy+ dy) * nz + (cz + dz);
                for (int j : gridMap[neighbor_index]) {
                    if (j <= i) continue;
                    float combinedRadius = bodies[i].radius + bodies[j].radius;
                    glm::vec3 d = bodies[j].pos - bodies[i].pos;
                    float dist2 = d.x*d.x + d.y*d.y + d.z*d.z;
                    if (dist2 <= combinedRadius * combinedRadius) pairs.push_back({i, j});
                }
            }
        }
        return pairs;

    }

    // 3. Octree
    std::vector<NeighborPair> OctreeSearch(const std::vector<Body>& bodies) {
        std::vector<NeighborPair> pairs;
        NSOctree nsOctree;
        // 1.rebuild an octree
        nsOctree.Build(bodies);
        // 2. search neighbor
        std::vector<int> neighbors;
        for (int i = 0; i < (int)bodies.size(); i++) {
            neighbors.clear();
            // 
            nsOctree.Query(i, bodies[i].radius, neighbors);
            for (int j : neighbors)
                if (i < j) {
                    pairs.push_back({i, j});
                    //std::cout <<"Octree: ("<< i << ", " << j <<")"<< std::endl;
                }
        }
        return pairs;
    }

};
