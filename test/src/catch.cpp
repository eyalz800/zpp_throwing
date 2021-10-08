#include "test.h"

static zpp::throwing<void> throw_exception()
{
    co_yield std::runtime_error("My runtime error!");

    [] { FAIL(); }();
}

TEST(catch, test_catch_base)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_exception();

        [] { FAIL(); }();
    }, [] (const std::exception & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
    }, []() {
        FAIL();
    });
}

TEST(catch, test_catch_exact)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_exception();

        [] { FAIL(); }();
    }, [] (const std::runtime_error & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
    }, []() {
        FAIL();
    });
}

TEST(catch, test_catch_derived)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_exception();

        [] { FAIL(); }();
    }, [] (const std::range_error &) {
        FAIL();
    }, []() {
        SUCCEED();
    });
}

TEST(catch, test_catch_unrelated)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_exception();

        [] { FAIL(); }();
    }, [] (std::errc) {
        FAIL();
    }, [] (zpp::error) {
        FAIL();
    }, []() {
        SUCCEED();
    });
}

TEST(catch, test_catch_derived_exact_fallback)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_exception();

        [] { FAIL(); }();
    }, [] (const std::range_error &) {
        FAIL();
    }, [](const std::runtime_error & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
    }, []() {
        FAIL();
    });
}

TEST(catch, test_catch_derived_base_fallback)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_exception();

        [] { FAIL(); }();
    }, [] (const std::range_error &) {
        FAIL();
    }, [] (const std::exception & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
    }, []() {
        FAIL();
    });
}

TEST(catch, test_catch_derived_base_fallback_order_priority)
{
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        co_await throw_exception();

        [] { FAIL(); }();
    }, [] (const std::range_error &) {
        FAIL();
    }, [] (const std::exception & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
    }, [] (const std::runtime_error &) {
        FAIL();
    }, []() {
        FAIL();
    });
}

TEST(catch, test_uncaght_propagate)
{
    zpp::try_catch([&]() -> zpp::throwing<void> {

        return zpp::try_catch([&]() -> zpp::throwing<void> {
            co_yield std::runtime_error("My runtime error!");

            [] { FAIL(); }();
        }, [](const std::range_error &) {
            FAIL();
        });

    }, [](const std::runtime_error & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
    }, []() {
        FAIL();
    });
}

