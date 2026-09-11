#include <stdio.h>
#include <string.h>
#include "purrgo/purrgo_format.h"

int num_failures = 0;

#define EXPECT_STR_EQ(exp, act) \
    if (strcmp((exp), (act)) != 0) { \
        printf("FAIL at %d: expected '%s', got '%s'\n", __LINE__, (exp), (act)); \
        num_failures++; \
    }

#define EXPECT_EQ(exp, act) \
    if ((exp) != (act)) { \
        printf("FAIL at %d: expected %d, got %d\n", __LINE__, (int)(exp), (int)(act)); \
        num_failures++; \
    }

void test_basic_strings() {
    char buf[64];
    purrgo_snprintf(buf, sizeof(buf), "Hello, %s!", "World");
    EXPECT_STR_EQ("Hello, World!", buf);

    purrgo_snprintf(buf, sizeof(buf), "Hello, %10s!", "World");
    EXPECT_STR_EQ("Hello,      World!", buf);

    purrgo_snprintf(buf, sizeof(buf), "Hello, %-10s!", "World");
    EXPECT_STR_EQ("Hello, World     !", buf);

    purrgo_snprintf(buf, sizeof(buf), "%.*s", 3, "World");
    EXPECT_STR_EQ("Wor", buf);

    purrgo_snprintf(buf, sizeof(buf), "%s", NULL);
    EXPECT_STR_EQ("(null)", buf);
}

void test_integers() {
    char buf[64];

    purrgo_snprintf(buf, sizeof(buf), "%d", 42);
    EXPECT_STR_EQ("42", buf);

    purrgo_snprintf(buf, sizeof(buf), "%d", -42);
    EXPECT_STR_EQ("-42", buf);

    purrgo_snprintf(buf, sizeof(buf), "%04d", 42);
    EXPECT_STR_EQ("0042", buf);

    purrgo_snprintf(buf, sizeof(buf), "%04d", -42);
    EXPECT_STR_EQ("-042", buf);

    purrgo_snprintf(buf, sizeof(buf), "%4d", 42);
    EXPECT_STR_EQ("  42", buf);

    purrgo_snprintf(buf, sizeof(buf), "%-4d", 42);
    EXPECT_STR_EQ("42  ", buf);

    purrgo_snprintf(buf, sizeof(buf), "%u", 42);
    EXPECT_STR_EQ("42", buf);

    purrgo_snprintf(buf, sizeof(buf), "%u", -1);
    char exp_u[32];
    sprintf(exp_u, "%u", -1);
    EXPECT_STR_EQ(exp_u, buf);
}

void test_hex() {
    char buf[64];

    purrgo_snprintf(buf, sizeof(buf), "%x", 255);
    EXPECT_STR_EQ("ff", buf);

    purrgo_snprintf(buf, sizeof(buf), "%X", 255);
    EXPECT_STR_EQ("FF", buf);

    purrgo_snprintf(buf, sizeof(buf), "%04X", 255);
    EXPECT_STR_EQ("00FF", buf);
}

void test_char() {
    char buf[64];

    purrgo_snprintf(buf, sizeof(buf), "%c", 'A');
    EXPECT_STR_EQ("A", buf);

    purrgo_snprintf(buf, sizeof(buf), "%%");
    EXPECT_STR_EQ("%", buf);
}

void test_truncation() {
    char buf[5];
    int len = purrgo_snprintf(buf, sizeof(buf), "123456789");
    EXPECT_STR_EQ("1234", buf);
    EXPECT_EQ(9, len);
}

int main() {
    test_basic_strings();
    test_integers();
    test_hex();
    test_char();
    test_truncation();

    if (num_failures > 0) {
        printf("FAILED %d tests.\n", num_failures);
        return 1;
    }
    printf("All test_purrgo_format tests passed.\n");
    return 0;
}
