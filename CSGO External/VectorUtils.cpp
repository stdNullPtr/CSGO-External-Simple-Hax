#include "VectorUtils.hpp"

float Vector2D::operator[](const int i) const
{
    return ((float*)this)[i];
}

float& Vector2D::operator[](const int i)
{
    return reinterpret_cast<float*>(this)[i];
}

void Vector2D::Clear()
{
    this->x = this->y = 0.0f;
}

float Vector2D::LengthSqr() const
{
    return static_cast<float>(pow(this->x, 2) + pow(this->y, 2));
}

Vector2D Vector2D::operator+(const Vector2D& v) const
{
    return Vector2D(this->x + v.x, this->y + v.y);
}

Vector2D Vector2D::operator-(const Vector2D& v) const
{
    return Vector2D(this->x - v.x, this->y - v.y);
}

Vector2D Vector2D::operator*(const Vector2D& v) const
{
    return Vector2D(this->x * v.x, this->y * v.y);
}

Vector2D Vector2D::operator*(const float f) const
{
    return Vector2D(this->x * f, this->y * f);
}

Vector2D Vector2D::operator/(const Vector2D& v) const
{
    return Vector2D(this->x / v.x, this->y / v.y);
}

Vector2D Vector2D::operator/(const float f) const
{
    return Vector2D(this->x / f, this->y / f);
}

Vector2D& Vector2D::operator*=(const float f)
{
    this->x *= f;
    this->y *= f;

    return *this;
}

Vector2D& Vector2D::operator/=(const float f)
{
    this->x /= f;
    this->y /= f;

    return *this;
}

float Vector::operator[](const int i) const
{
    return ((float*)this)[i];
}

float& Vector::operator[](const int i)
{
    return reinterpret_cast<float*>(this)[i];
}

void Vector::clear()
{
    this->x = this->y = this->z = 0.0f;
}

float Vector::LengthSqr() const
{
    return static_cast<float>(pow(this->x, 2) + pow(this->y, 2) + pow(this->z, 2));
}

Vector Vector::operator+(const Vector& v) const
{
    return Vector(this->x + v.x, this->y + v.y, this->z + v.z);
}

Vector Vector::operator-(const Vector& v) const
{
    return Vector(this->x - v.x, this->y - v.y, this->z - v.z);
}

Vector Vector::operator*(const Vector& v) const
{
    return Vector(this->x * v.x, this->y * v.y, this->z * v.z);
}

Vector Vector::operator*(const float f) const
{
    return Vector(this->x * f, this->y * f, this->z * f);
}

Vector Vector::operator/(const Vector& v) const
{
    return Vector(this->x / v.x, this->y / v.y, this->z / v.z);
}

Vector Vector::operator/(const float f) const
{
    return Vector(this->x / f, this->y / f, this->z / f);
}

Vector& Vector::operator*=(const float f)
{
    this->x *= f;
    this->y *= f;
    this->z *= f;

    return *this;
}

Vector& Vector::operator/=(const float f)
{
    this->x /= f;
    this->y /= f;
    this->z /= f;

    return *this;
}