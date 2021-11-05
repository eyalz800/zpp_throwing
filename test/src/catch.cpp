#include "test.h"

static zpp::throwing<void> throw_exception()
{
    co_yield std::runtime_error("My runtime error!");

    [] { FAIL(); }();
}

TEST(catch, test_catch_base)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_exception();

        [] { FAIL(); }();
    }, [&] (const std::exception & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
        trigger.trigger();
    }, [&]() {
        FAIL();
    });
}

TEST(catch, test_catch_exact)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_exception();

        [] { FAIL(); }();
    }, [&](const std::runtime_error & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
        trigger.trigger();
    }, [&]() {
        FAIL();
    });
}

TEST(catch, test_catch_derived)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_exception();

        [] { FAIL(); }();
    }, [&](const std::range_error &) {
        FAIL();
    }, [&]() {
        trigger.trigger();
    });
}

TEST(catch, test_catch_unrelated)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_exception();

        [] { FAIL(); }();
    }, [&](std::errc) {
        FAIL();
    }, [&](zpp::error) {
        FAIL();
    }, [&]() {
        trigger.trigger();
    });
}

TEST(catch, test_catch_derived_exact_fallback)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_exception();

        [] { FAIL(); }();
    }, [&](const std::range_error &) {
        FAIL();
    }, [&](const std::runtime_error & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
        trigger.trigger();
    }, [&]() {
        FAIL();
    });
}

TEST(catch, test_catch_derived_base_fallback)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_exception();

        [] { FAIL(); }();
    }, [&](const std::range_error &) {
        FAIL();
    }, [&](const std::exception & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
        trigger.trigger();
    }, [&]() {
        FAIL();
    });
}

TEST(catch, test_catch_derived_base_fallback_order_priority)
{
    fail_unless_triggered trigger{2};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();
        co_await throw_exception();

        [] { FAIL(); }();
    }, [&](const std::range_error &) {
        FAIL();
    }, [&](const std::exception & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
        trigger.trigger();
    }, [&](const std::runtime_error &) {
        FAIL();
    }, [&]() {
        FAIL();
    });
}

TEST(catch, test_uncaght_propagate)
{
    fail_unless_triggered trigger{3};
    return zpp::try_catch([&]() -> zpp::throwing<void> {
        trigger.trigger();

        return zpp::try_catch([&]() -> zpp::throwing<void> {
            trigger.trigger();
            co_yield std::runtime_error("My runtime error!");

            [] { FAIL(); }();
        }, [](const std::range_error &) {
            FAIL();
        });

    }, [&](const std::runtime_error & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
        trigger.trigger();
    }, [&]() {
        FAIL();
    });
}

