#pragma once
#include <cmath>
struct Vec3{
    double x, y, z;
    Vec3():x(0.0), y(0.0), z(0.0){}
    Vec3(double _x, double _y, double _z):x(_x), y(_y), z(_z){}

    Vec3 operator+(const Vec3& other) const {
        return Vec3(x + other.x, y + other.y, z + other.z);
    }

    Vec3 operator-(const Vec3& other) const {
        return Vec3(x - other.x, y - other.y, z - other.z);
    }

    Vec3 operator*(double scalar) const {
        return Vec3(x * scalar, y * scalar, z * scalar);
    }
    Vec3& operator+=(const Vec3& other) {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }
    double dot(const Vec3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }
    double norm() const {
        return std::sqrt(this->dot(*this));
    }

};