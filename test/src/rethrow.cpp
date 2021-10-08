#include "test.h"

static zpp::throwing<int> throw_exception()
{
    co_yield std::runtime_error("My runtime error!");

    [] { FAIL(); }();
}

static zpp::throwing<int> throw_error()
{
    co_yield std::errc::invalid_argument;

    [] { FAIL(); }();
}

TEST(rethrow, rethrow)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {

        co_await zpp::try_catch([]() -> zpp::throwing<void> {
            co_await throw_exception();
        }, [](const std::runtime_error &) -> zpp::throwing<void> {
            co_yield zpp::rethrow;
        });

        [] { FAIL(); }();
    }, [](const std::runtime_error & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
    }, []() {
        FAIL();
    });
}

TEST(rethrow, rethrow_value_catch_exact)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {

        co_await zpp::try_catch([]() -> zpp::throwing<void> {
            co_await throw_error();
        }, [](const std::runtime_error &) -> zpp::throwing<void> {
            co_yield zpp::rethrow;
        });

        [] { FAIL(); }();
    }, [](std::errc error) {
        EXPECT_EQ(error, std::errc::invalid_argument);
    }, []() {
        FAIL();
    });
}

TEST(rethrow, rethrow_value_catch_error)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {

        co_await zpp::try_catch([]() -> zpp::throwing<void> {
            co_await throw_error();
        }, [](const std::runtime_error &) -> zpp::throwing<void> {
            co_yield zpp::rethrow;
        });

        [] { FAIL(); }();
    }, [](zpp::error error) {
        EXPECT_EQ(&error.domain(), &zpp::err_domain<std::errc>);
        EXPECT_EQ(std::errc{error.code()}, std::errc::invalid_argument);
    }, []() {
        FAIL();
    });
}

TEST(rethrow, nested_rethrow)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {

        co_await zpp::try_catch([]() -> zpp::throwing<void> {
            co_await throw_exception();
        }, [](const std::runtime_error &) -> zpp::throwing<void> {
            co_await []() -> zpp::throwing<void> {
                co_yield zpp::rethrow;
            }();
        });

        [] { FAIL(); }();
    }, [](const std::runtime_error & error) {
        EXPECT_STREQ(error.what(), "My runtime error!");
    }, []() {
        FAIL();
    });
}

TEST(rethrow, rethrow_without_execption)
{
    return zpp::try_catch([]() -> zpp::throwing<void> {
        co_yield zpp::rethrow;

        [] { FAIL(); }();
    }, [](zpp::rethrow_error) {
        SUCCEED();
    }, []() {
        FAIL();
    });

}
