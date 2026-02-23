#ifndef PARTICLE_H
#define PARTICLE_H
#include <Eigen/Dense>
struct Particle {

    Eigen::Vector3d position;
    Eigen::Vector3d velocity;
    double mass;
    double h; // smoothing length

    
    Particle() : position(0,0,0), velocity(0,0,0), mass(1.0), h(1.0) {}
    void update(double dt, const Eigen::Vector3d &acceleration);
    void setState(const Eigen::Vector3d& pos, const Eigen::Vector3d& vel);
};
#endif