#include "test.h"

namespace
{

zpp::throwing<int> integer_divide(int x, int y)
{
    if (y == 0) {
        co_yield std::overflow_error("Divide by zero!");
    } else if (x % y != 0) {
        co_yield std::range_error("Result is not an integer!");
    }
    co_return x / y;
}

zpp::throwing<bool> integer_divide_equals(int x, int y, int z)
{
    if (co_await integer_divide(x, y) == z) {
        co_return true;
    }
    co_return false;
}

int test_integer_divide_equals(int x, int y, int z)
{
    return zpp::try_catch([&]() -> zpp::throwing<int> {
        if (co_await integer_divide_equals(x, y, z)) {
            co_return 1;
        } else {
            co_return 2;
        }
    }, [&](const std::overflow_error &) {
        return 3;
    }, [&](const std::range_error &) {
        return 4;
    }, [&]() {
        return 5;
    });
}

}

TEST(sanity_integer_divide, integer_divide)
{
    EXPECT_EQ(test_integer_divide_equals(4, 2, 2), 1);
    EXPECT_EQ(test_integer_divide_equals(4, 2, 1), 2);
    EXPECT_EQ(test_integer_divide_equals(4, 0, 2), 3);
    EXPECT_EQ(test_integer_divide_equals(4, 3, 2), 4);
}

