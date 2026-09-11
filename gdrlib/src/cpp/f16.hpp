#pragma once

#include <meshoptimizer.h>
#include <pod_types.hpp>

namespace cpp
{
    struct f16
    {
    private:
        u16 m_data {0};

    public:
        f16() = default;

        ~f16() = default;

        f16(const f32 value)
            : m_data(meshopt_quantizeHalf(value))
        {
        }

        [[nodiscard]] f32 to_f32() const { return meshopt_dequantizeHalf(m_data); }

        f16& operator=(const f32 value)
        {
            m_data = meshopt_quantizeHalf(value);
            return *this;
        }

        f16& operator+=(const f16 other)
        {
            m_data = meshopt_quantizeHalf(this->to_f32() + other.to_f32());
            return *this;
        }

        f16& operator-=(const f16 other)
        {
            m_data = meshopt_quantizeHalf(this->to_f32() - other.to_f32());
            return *this;
        }

        f16& operator*=(const f16 other)
        {
            m_data = meshopt_quantizeHalf(this->to_f32() - other.to_f32());
            return *this;
        }

        f16& operator/=(const f16 other)
        {
            m_data = meshopt_quantizeHalf(this->to_f32() - other.to_f32());
            return *this;
        }

        [[nodiscard]] f16 operator+(const f16 other) const { return {this->to_f32() + other.to_f32()}; }

        [[nodiscard]] f16 operator-(const f16 other) const { return {this->to_f32() - other.to_f32()}; }

        [[nodiscard]] f16 operator*(const f16 other) const { return {this->to_f32() * other.to_f32()}; }

        [[nodiscard]] f16 operator/(const f16 other) const { return {this->to_f32() / other.to_f32()}; }

        [[nodiscard]] bool operator==(const f16& other) const { return m_data == other.m_data; }
    };
}
