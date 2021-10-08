#include "test.h"

static zpp::throwing<void> throw_error()
{
    co_yield std::errc::invalid_argument;

    [] { FAIL(); }();
}

TEST(catch_values, test_catch_error)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_error();

        [] { FAIL(); }();
    }, [] (zpp::error error) {
        EXPECT_EQ(&error.domain(), &zpp::err_domain<std::errc>);
        EXPECT_EQ(std::errc{error.code()}, std::errc::invalid_argument);
    }, []() {
        FAIL();
    });
}

TEST(catch_values, test_catch_exact)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_error();

        [] { FAIL(); }();
    }, [] (std::errc error) {
        EXPECT_EQ(error, std::errc::invalid_argument);
    }, []() {
        FAIL();
    });
}

TEST(catch_values, test_catch_unrelated)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_error();

        [] { FAIL(); }();
    }, [] (const std::exception &) {
        FAIL();
    }, [] (zpp::rethrow_error) {
        FAIL();
    }, []() {
        SUCCEED();
    });
}

TEST(catch_values, test_catch_exact_fallback)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_error();

        [] { FAIL(); }();
    }, [] (const std::exception &) {
        FAIL();
    }, [] (std::errc error) {
        EXPECT_EQ(error, std::errc::invalid_argument);
    }, []() {
        FAIL();
    });
}

TEST(catch_values, test_catch_error_fallback)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_error();

        [] { FAIL(); }();
    }, [] (const std::exception &) {
        FAIL();
    }, [] (zpp::rethrow_error) {
        FAIL();
    }, [] (zpp::error error) {
        EXPECT_EQ(&error.domain(), &zpp::err_domain<std::errc>);
        EXPECT_EQ(std::errc{error.code()}, std::errc::invalid_argument);
    }, []() {
        FAIL();
    });
}

TEST(catch_values, test_catch_exact_base_fallback_order_priority)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_error();

        [] { FAIL(); }();
    }, [] (const std::exception &) {
        FAIL();
    }, [] (zpp::rethrow_error) {
        FAIL();
    }, [] (std::errc error) {
        EXPECT_EQ(error, std::errc::invalid_argument);
    }, [] (zpp::error) {
        FAIL();
    }, []() {
        FAIL();
    });
}

TEST(catch_values, test_uncaght_propagate)
{
    zpp::try_catch([&]() -> zpp::throwing<void> {

        return zpp::try_catch([&]() -> zpp::throwing<void> {
            co_yield std::errc::invalid_argument;

            [] { FAIL(); }();
        }, [](const std::exception &) {
            FAIL();
        });

    }, [] (zpp::error error) {
        EXPECT_EQ(&error.domain(), &zpp::err_domain<std::errc>);
        EXPECT_EQ(std::errc{error.code()}, std::errc::invalid_argument);
    }, []() {
        FAIL();
    });
}

