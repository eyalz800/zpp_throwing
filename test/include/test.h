#include "gtest/gtest.h"
#include "zpp_throwing.h"

struct fail_unless_triggered
{
    explicit fail_unless_triggered(int expected) :
        count(expected)
    {
    }

    fail_unless_triggered(const fail_unless_triggered &) = delete;
    fail_unless_triggered & operator=(const fail_unless_triggered &) = delete;

    ~fail_unless_triggered()
    {
        if (count) {
            [] { FAIL(); }();
        }
    }

    void trigger()
    {
        --count;
    }

    int count{};
};
