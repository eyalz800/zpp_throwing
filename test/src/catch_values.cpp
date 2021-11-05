#include "test.h"

static zpp::throwing<void> throw_error()
{
    co_yield std::errc::invalid_argument;

    [] { FAIL(); }();
}

TEST(catch_values, test_catch_error)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_error();

        [] { FAIL(); }();
    }, [&](zpp::error error) {
        EXPECT_EQ(&error.domain(), &zpp::err_domain<std::errc>);
        EXPECT_EQ(std::errc{error.code()}, std::errc::invalid_argument);
        trigger.trigger();
    }, [&]() {
        FAIL();
    });
}

TEST(catch_values, test_catch_exact)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_error();

        [] { FAIL(); }();
    }, [&](std::errc error) {
        EXPECT_EQ(error, std::errc::invalid_argument);
        trigger.trigger();
    }, [&]() {
        FAIL();
    });
}

TEST(catch_values, test_catch_unrelated)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_error();

        [] { FAIL(); }();
    }, [&](const std::exception &) {
        FAIL();
    }, [&] (zpp::rethrow_error) {
        FAIL();
    }, [&]() {
        trigger.trigger();
    });
}

TEST(catch_values, test_catch_exact_fallback)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_error();

        [&]{ FAIL(); }();
    }, [&](const std::exception &) {
        FAIL();
    }, [&](std::errc error) {
        EXPECT_EQ(error, std::errc::invalid_argument);
        trigger.trigger();
    }, [&]() {
        FAIL();
    });
}

TEST(catch_values, test_catch_error_fallback)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_error();

        [] { FAIL(); }();
    }, [&] (const std::exception &) {
        FAIL();
    }, [&] (zpp::rethrow_error) {
        FAIL();
    }, [&] (zpp::error error) {
        EXPECT_EQ(&error.domain(), &zpp::err_domain<std::errc>);
        EXPECT_EQ(std::errc{error.code()}, std::errc::invalid_argument);
        trigger.trigger();
    }, []() {
        FAIL();
    });
}

TEST(catch_values, test_catch_exact_base_fallback_order_priority)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_error();

        [] { FAIL(); }();
    }, [&](const std::exception &) {
        FAIL();
    }, [&](zpp::rethrow_error) {
        FAIL();
    }, [&](std::errc error) {
        EXPECT_EQ(error, std::errc::invalid_argument);
        trigger.trigger();
    }, [&](zpp::error) {
        FAIL();
    }, [&]() {
        FAIL();
    });
}

TEST(catch_values, test_uncaght_propagate)
{
    fail_unless_triggered trigger{3};
    zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();

        return zpp::try_catch([&]() -> zpp::throwing<void> {
            trigger.trigger();
            co_yield std::errc::invalid_argument;

            [] { FAIL(); }();
        }, [&](const std::exception &) {
            FAIL();
        });

    }, [&](zpp::error error) {
        EXPECT_EQ(&error.domain(), &zpp::err_domain<std::errc>);
        EXPECT_EQ(std::errc{error.code()}, std::errc::invalid_argument);
        trigger.trigger();
    }, [&]() {
        FAIL();
    });
}

