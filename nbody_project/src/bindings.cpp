#include <emscripten/bind.h>
#include "nbody/body.h"
#include "nbody/brute_force.h"
#include "nbody/integrator.h"
#include <vector>
#include <cmath>

class WebSimulation {
public:
    WebSimulation(): calc_(1.0, 0.0), integrator_(calc_) {
        reset(100.0, 1.0, 1.0);
    }
    void reset(double mass0, double mass1, double r0) {
        
        bodies_.clear();
        bodies_.emplace_back(Body());
        bodies_.emplace_back(Body());

        bodies_[0].mass = mass0;
       

        bodies_[1].mass = mass1;
        bodies_[1].position = Vec3(r0, 0.0, 0.0);   

        
        double v_circ = std::sqrt(this->G * (bodies_[0].mass + bodies_[1].mass) / r0);
        
        calc_.computeAccelerations(bodies_);

        bodies_[1].velocity = Vec3(0.0, v_circ, 0.0);



    }
    void step(double dt) {
        integrator_.step(bodies_, dt);

    }
    double getX(int i) const { return bodies_[i].position.x; }
    double getY(int i) const { return bodies_[i].position.y; }
    int bodyCount() const { return bodies_.size(); }
private:
    const double G = 1.0;
    std::vector<Body> bodies_;
    BruteForceCalculator calc_;
    LeapfrogIntegrator integrator_;

};
EMSCRIPTEN_BINDINGS(nbody_module) {
    emscripten::class_<WebSimulation>("WebSimulation")
        .constructor<>()
        .function("reset", &WebSimulation::reset)
        .function("step", &WebSimulation::step)
        .function("getX", &WebSimulation::getX)
        .function("getY", &WebSimulation::getY)
        .function("bodyCount", &WebSimulation::bodyCount);
}