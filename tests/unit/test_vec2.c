#include "unity.h"
#include "../src/shared/vec2.h"
#include <math.h>

void setUp(void) {}

void tearDown(void) {}

void test_vec2_add(void) {
    ShroomVec2 a = {1.0f, 2.0f};
    ShroomVec2 b = {3.0f, 4.0f};
    ShroomVec2 result = ShroomVec2Add(a, b);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, result.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 6.0f, result.y);
}

void test_vec2_sub(void) {
    ShroomVec2 a = {5.0f, 7.0f};
    ShroomVec2 b = {2.0f, 3.0f};
    ShroomVec2 result = ShroomVec2Sub(a, b);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, result.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 4.0f, result.y);
}

void test_vec2_scale(void) {
    ShroomVec2 v = {2.0f, 3.0f};
    ShroomVec2 result = ShroomVec2Scale(v, 2.5f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 5.0f, result.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 7.5f, result.y);
}

void test_vec2_length_sqr(void) {
    ShroomVec2 v = {3.0f, 4.0f};
    float length_sqr = ShroomVec2LengthSqr(v);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 25.0f, length_sqr);
}

void test_vec2_dot(void) {
    ShroomVec2 a = {1.0f, 2.0f};
    ShroomVec2 b = {3.0f, 4.0f};
    float dot = ShroomVec2Dot(a, b);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 11.0f, dot);
}

void test_vec2_add_zero(void) {
    ShroomVec2 a = {1.0f, 2.0f};
    ShroomVec2 zero = {0.0f, 0.0f};
    ShroomVec2 result = ShroomVec2Add(a, zero);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, result.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, result.y);
}

void test_vec2_scale_zero(void) {
    ShroomVec2 v = {2.0f, 3.0f};
    ShroomVec2 result = ShroomVec2Scale(v, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, result.y);
}

void test_vec2_scale_negative(void) {
    ShroomVec2 v = {2.0f, 3.0f};
    ShroomVec2 result = ShroomVec2Scale(v, -1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -2.0f, result.x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -3.0f, result.y);
}

void test_vec2_dot_perpendicular(void) {
    ShroomVec2 a = {1.0f, 0.0f};
    ShroomVec2 b = {0.0f, 1.0f};
    float dot = ShroomVec2Dot(a, b);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, dot);
}

void test_vec2_dot_parallel(void) {
    ShroomVec2 a = {1.0f, 0.0f};
    ShroomVec2 b = {2.0f, 0.0f};
    float dot = ShroomVec2Dot(a, b);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, dot);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_vec2_add);
    RUN_TEST(test_vec2_sub);
    RUN_TEST(test_vec2_scale);
    RUN_TEST(test_vec2_length_sqr);
    RUN_TEST(test_vec2_dot);
    RUN_TEST(test_vec2_add_zero);
    RUN_TEST(test_vec2_scale_zero);
    RUN_TEST(test_vec2_scale_negative);
    RUN_TEST(test_vec2_dot_perpendicular);
    RUN_TEST(test_vec2_dot_parallel);
    return UNITY_END();
}
