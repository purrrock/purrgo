#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "purrgo/purrgo_parse.h"

#define ASSERT_EQ(actual, expected) \
    do { \
        if ((actual) != (expected)) { \
            fprintf(stderr, "Assertion failed: %s:%d: %d != %d\n", __FILE__, __LINE__, (int)(actual), (int)(expected)); \
            exit(1); \
        } \
    } while (0)

static void test_zero(void) {
    ASSERT_EQ(purrgo_parse_int32("0"), 0);
}

static void test_positive(void) {
    ASSERT_EQ(purrgo_parse_int32("12345"), 12345);
}

static void test_negative(void) {
    ASSERT_EQ(purrgo_parse_int32("-9876"), -9876);
}

static void test_leading_spaces_tabs(void) {
    ASSERT_EQ(purrgo_parse_int32("   \t  42"), 42);
    ASSERT_EQ(purrgo_parse_int32("\t-73"), -73);
}

static void test_explicit_positive(void) {
    ASSERT_EQ(purrgo_parse_int32("+123"), 123);
}

static void test_explicit_negative(void) {
    ASSERT_EQ(purrgo_parse_int32("-123"), -123);
}

static void test_int32_max(void) {
    ASSERT_EQ(purrgo_parse_int32("2147483647"), 2147483647);
}

static void test_int32_min(void) {
    // Note: Use -2147483647 - 1 to avoid compiler warnings about unary minus on unsigned
    ASSERT_EQ(purrgo_parse_int32("-2147483648"), -2147483647 - 1);
}

static void test_positive_overflow(void) {
    ASSERT_EQ(purrgo_parse_int32("2147483648"), 2147483647);
    ASSERT_EQ(purrgo_parse_int32("3000000000"), 2147483647);
}

static void test_negative_overflow(void) {
    ASSERT_EQ(purrgo_parse_int32("-2147483649"), -2147483647 - 1);
    ASSERT_EQ(purrgo_parse_int32("-3000000000"), -2147483647 - 1);
}

static void test_very_long_positive(void) {
    ASSERT_EQ(purrgo_parse_int32("9999999999999999999999999"), 2147483647);
}

static void test_very_long_negative(void) {
    ASSERT_EQ(purrgo_parse_int32("-9999999999999999999999999"), -2147483647 - 1);
}

static void test_non_digit_termination(void) {
    ASSERT_EQ(purrgo_parse_int32("123abc456"), 123);
    ASSERT_EQ(purrgo_parse_int32("-45xyz"), -45);
    ASSERT_EQ(purrgo_parse_int32("  +789\n"), 789);
}

int main(void) {
    test_zero();
    test_positive();
    test_negative();
    test_leading_spaces_tabs();
    test_explicit_positive();
    test_explicit_negative();
    test_int32_max();
    test_int32_min();
    test_positive_overflow();
    test_negative_overflow();
    test_very_long_positive();
    test_very_long_negative();
    test_non_digit_termination();
    printf("All purrgo_parse tests passed.\n");
    return 0;
}
