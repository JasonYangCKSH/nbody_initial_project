#include<iostream>
#include <bitset>
#include "../include/glm/glm.hpp"
static uint64_t expandBits3D(uint64_t vec) {
    vec &= 0x1FFFFFULL;
    vec = (vec | (vec << 32)) & 0x1F00000000FFFFULL;
    vec = (vec | (vec << 16)) & 0x1F0000FF0000FFULL;
    vec = (vec | (vec <<  8)) & 0x100F00F00F00F00FULL;
    vec = (vec | (vec <<  4)) & 0x10C30C30C30C30C3ULL;
    vec = (vec | (vec <<  2)) & 0x1249249249249249ULL;
    return vec;
}

static uint64_t encodeMorton3D(glm::ivec3 cellCoord) {
    constexpr int64_t OFFSET = 1 << 20; 
    uint64_t x = expandBits3D(static_cast<uint64_t>(cellCoord.x + OFFSET));
    uint64_t y = expandBits3D(static_cast<uint64_t>(cellCoord.y + OFFSET));
    uint64_t z = expandBits3D(static_cast<uint64_t>(cellCoord.z + OFFSET));
    
    return (z << 2) | (y << 1) | x;
}
inline uint32_t compactBy2(uint64_t x) {
    x &= 0x9249249249249249ULL;                  // 保留 bit 0, 3, 6, 9, 12, 15, 18, 21...
    x = (x ^ (x >> 2))  & 0x30c30c30c30c30c3ULL;
    x = (x ^ (x >> 4))  & 0xf00f00f00f00f00fULL;
    x = (x ^ (x >> 8))  & 0x00ff0000ff0000ffULL;
    x = (x ^ (x >> 16)) & 0xffff00000000ffffULL;
    x = (x ^ (x >> 32)) & 0x00000000001fffffULL; // 最終還原為 21-bit 整數
    return static_cast<uint32_t>(x);
}
void decodeMorton3D(uint64_t morton, int32_t& x, int32_t& y, int32_t& z) {
    constexpr int64_t OFFSET = 1 << 20;
    x = static_cast<int32_t>(compactBy2(morton))      - OFFSET;
    y = static_cast<int32_t>(compactBy2(morton >> 1)) - OFFSET;
    z = static_cast<int32_t>(compactBy2(morton >> 2)) - OFFSET;
}
int main(){
    glm::ivec3 pos = {-1, 3, 5};
    uint64_t ans = encodeMorton3D(pos);
    std::cout << std::bitset<64>(ans) << std::endl;
    int32_t x, y, z;
    decodeMorton3D(ans, x, y, z);
    std::cout << x << ", " << y << ", " << z << std::endl;
}