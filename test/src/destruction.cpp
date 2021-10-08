#include "test.h"

namespace
{

struct destruction_resource
{
    explicit destruction_resource(bool & is_destroyed) :
        is_destroyed(&is_destroyed)
    {
    }

    destruction_resource() = delete;
    destruction_resource(destruction_resource &&) = delete;
    destruction_resource(const destruction_resource &) = delete;
    destruction_resource & operator=(destruction_resource &&) = delete;
    destruction_resource & operator=(const destruction_resource &) = delete;

    ~destruction_resource()
    {
        *is_destroyed = true;
    }

    bool * is_destroyed;
};

struct destruction_exception
{
    explicit destruction_exception(bool & is_destroyed) :
        is_destroyed(&is_destroyed)
    {
    }

    destruction_exception() = delete;
    destruction_exception(destruction_exception && other) noexcept :
        is_destroyed(other.is_destroyed)
    {
        other.is_destroyed = nullptr;
    }

    destruction_exception(const destruction_exception &) = delete;
    destruction_exception & operator=(destruction_exception &&) = delete;
    destruction_exception & operator=(const destruction_exception &) = delete;

    ~destruction_exception()
    {
        if (is_destroyed) {
            if (*is_destroyed) {
                [] { FAIL(); }();
            }
            *is_destroyed = true;
        }
    }

    bool * is_destroyed;
};

}


template <>
struct zpp::define_exception<destruction_exception>
{
    using type = zpp::define_exception_bases<>;
};

TEST(destruction, with_exception)
{
    bool is_destroyed = false;

    return zpp::try_catch([&]() -> zpp::throwing<void> {
        destruction_resource resource(is_destroyed);
        co_yield std::runtime_error("My runtime error!");

        [] { FAIL(); }();
    }, [&] {
        EXPECT_EQ(is_destroyed, true);
    });
}

TEST(destruction, without_exception)
{
    bool is_destroyed = false;

    zpp::try_catch([&]() -> zpp::throwing<void> {
        destruction_resource resource(is_destroyed);
        co_return;
    }, [&] {
        FAIL();
    });

    EXPECT_EQ(is_destroyed, true);
}

TEST(destruction, exception_destruction)
{
    bool is_resource_destroyed = false;
    bool is_exception_destroyed = false;

    zpp::try_catch([&]() -> zpp::throwing<void> {
        destruction_resource resource(is_resource_destroyed);
        EXPECT_EQ(is_resource_destroyed, false);

        co_await [&]() -> zpp::throwing<void> {
            co_yield destruction_exception(is_exception_destroyed);

            [] { FAIL(); }();
        }();

        [] { FAIL(); }();
    }, [&](const destruction_exception &) {
        EXPECT_EQ(is_resource_destroyed, true);
        EXPECT_EQ(is_exception_destroyed, false);
    }, [&] {
        FAIL();
    });

    EXPECT_EQ(is_resource_destroyed, true);
    EXPECT_EQ(is_exception_destroyed, true);
}

TEST(destruction, exception_destruction_catch_all_nothrow)
{
    bool is_resource_destroyed = false;
    bool is_exception_destroyed = false;

    zpp::try_catch([&]() -> zpp::throwing<void> {
        destruction_resource resource(is_resource_destroyed);
        EXPECT_EQ(is_resource_destroyed, false);
        EXPECT_EQ(is_exception_destroyed, false);

        co_await [&]() -> zpp::throwing<void> {
            co_yield destruction_exception(is_exception_destroyed);

            [] { FAIL(); }();
        }();

        [] { FAIL(); }();
    }, [&] {
        EXPECT_EQ(is_resource_destroyed, true);
        EXPECT_EQ(is_exception_destroyed, true);
    });

    EXPECT_EQ(is_exception_destroyed, true);
    EXPECT_EQ(is_resource_destroyed, true);
}

TEST(destruction, exception_destruction_catch_all_may_throw)
{
    bool is_resource_destroyed = false;
    bool is_exception_destroyed = false;

    zpp::try_catch([&]() -> zpp::throwing<void> {
        return zpp::try_catch([&]() -> zpp::throwing<void> {
            destruction_resource resource(is_resource_destroyed);
            EXPECT_EQ(is_resource_destroyed, false);
            EXPECT_EQ(is_exception_destroyed, false);

            co_await [&]() -> zpp::throwing<void> {
                co_yield destruction_exception(is_exception_destroyed);

                [] { FAIL(); }();
            }();

            [] { FAIL(); }();
        }, [&]() -> zpp::throwing<void> {
            EXPECT_EQ(is_resource_destroyed, true);
            EXPECT_EQ(is_exception_destroyed, false);
            co_return;
        });
    }, []{
        FAIL();
    });

    EXPECT_EQ(is_exception_destroyed, true);
    EXPECT_EQ(is_resource_destroyed, true);
}
