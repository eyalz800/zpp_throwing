#include "test.h"

TEST(return_value, return_int)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {
        auto return_1337 = []() -> zpp::throwing<int> { co_return 1337; };
        EXPECT_EQ(co_await return_1337(), 1337);
        co_return;
    }, []{
        FAIL();
    });
}

TEST(return_value, return_string)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {
        auto return_hello = []() -> zpp::throwing<std::string> { co_return "Hello"; };
        EXPECT_EQ(co_await return_hello(), "Hello");
        co_return;
    }, []{
        FAIL();
    });
}

TEST(return_value, return_move_only)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {
        auto return_p1337 = []() -> zpp::throwing<std::unique_ptr<int>> {
            co_return std::make_unique<int>(1337);
        };

        auto result = co_await return_p1337();
        EXPECT_NE(result, nullptr);
        EXPECT_EQ(*result, 1337);
        co_return;
    }, []{
        FAIL();
    });
}

TEST(return_value, return_reference)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {
        int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<int &> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_lvalue_reference_v<decltype(result)>);
        co_return;
    }, []{
        FAIL();
    });
}

TEST(return_value, return_rvalue_reference)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {
        int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<int &&> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_rvalue_reference_v<decltype(result)>);
        co_return;
    }, []{
        FAIL();
    });
}

TEST(return_value, return_const_reference)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {
        int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<const int &> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_lvalue_reference_v<decltype(result)>);
        EXPECT_TRUE(std::is_const_v<std::remove_reference_t<decltype(result)>>);
        co_return;
    }, []{
        FAIL();
    });
}

TEST(return_value, return_const_rvalue_reference)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {
        int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<const int &&> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_rvalue_reference_v<decltype(result)>);
        EXPECT_TRUE(std::is_const_v<std::remove_reference_t<decltype(result)>>);
        co_return;
    }, []{
        FAIL();
    });
}

TEST(return_value, return_const_reference_from_const)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {
        const int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<const int &> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_lvalue_reference_v<decltype(result)>);
        EXPECT_TRUE(std::is_const_v<std::remove_reference_t<decltype(result)>>);
        co_return;
    }, []{
        FAIL();
    });
}

TEST(return_value, return_const_rvalue_reference_from_const)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {
        const int value = 1337;
        auto return_ref_1337 = [&]() -> zpp::throwing<const int &&> {
            co_return value;
        };

        auto && result = co_await return_ref_1337();
        EXPECT_EQ(&value, &result);
        EXPECT_TRUE(std::is_rvalue_reference_v<decltype(result)>);
        EXPECT_TRUE(std::is_const_v<std::remove_reference_t<decltype(result)>>);
        co_return;
    }, []{
        FAIL();
    });
}
