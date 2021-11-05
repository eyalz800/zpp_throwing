#include "test.h"

TEST(return_value, return_int)
{
    fail_unless_triggered trigger{1};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        auto return_1337 = []() -> zpp::throwing<int> { co_return 1337; };
        EXPECT_EQ(co_await return_1337(), 1337);
        co_return;
    }, [&]() {
        FAIL();
    });
}

TEST(return_value, return_string)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        auto return_hello = []() -> zpp::throwing<std::string> { co_return "Hello"; };
        EXPECT_EQ(co_await return_hello(), "Hello");
        trigger.trigger();
        co_return;
    }, [&]() {
        FAIL();
    });
}

TEST(return_value, return_move_only)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        auto return_p1337 = []() -> zpp::throwing<std::unique_ptr<int>> {
            co_return std::make_unique<int>(1337);
        };

        auto result = co_await return_p1337();
        EXPECT_NE(result, nullptr);
        EXPECT_EQ(*result, 1337);
        trigger.trigger();
        co_return;
    }, [&]() {
        FAIL();
    });
}

TEST(return_value, return_reference)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<int &> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_lvalue_reference_v<decltype(result)>);
        trigger.trigger();
        co_return;
    }, [&]() {
        FAIL();
    });
}

TEST(return_value, return_rvalue_reference)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<int &&> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_rvalue_reference_v<decltype(result)>);
        trigger.trigger();
        co_return;
    }, [&]() {
        FAIL();
    });
}

TEST(return_value, return_const_reference)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<const int &> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_lvalue_reference_v<decltype(result)>);
        EXPECT_TRUE(std::is_const_v<std::remove_reference_t<decltype(result)>>);
        trigger.trigger();
        co_return;
    }, [&]() {
        FAIL();
    });
}

TEST(return_value, return_const_rvalue_reference)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<const int &&> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_rvalue_reference_v<decltype(result)>);
        EXPECT_TRUE(std::is_const_v<std::remove_reference_t<decltype(result)>>);
        trigger.trigger();
        co_return;
    }, [&]() {
        FAIL();
    });
}

TEST(return_value, return_const_reference_from_const)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        const int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<const int &> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_lvalue_reference_v<decltype(result)>);
        EXPECT_TRUE(std::is_const_v<std::remove_reference_t<decltype(result)>>);
        trigger.trigger();
        co_return;
    }, [&]() {
        FAIL();
    });
}

TEST(return_value, return_const_rvalue_reference_from_const)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        const int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<const int &&> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_rvalue_reference_v<decltype(result)>);
        EXPECT_TRUE(std::is_const_v<std::remove_reference_t<decltype(result)>>);
        trigger.trigger();
        co_return;
    }, [&]() {
        FAIL();
    });
}
