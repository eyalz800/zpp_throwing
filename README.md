zpp::throwing
=============

Abstract
--------
C++ exceptions and RTTI are often not enabled in many environments, require outstanding library and ABI specific support, and sometimes introduce
unwanted cost (i.e exceptions failure path is often considered very slow, RTTI often grows binary size and adds type names to the program binaries which may hurt confidentiality).
Most implementations of C++ exceptions also use the RTTI implementation, enlarging the price to be paid for using exceptions.

Exceptions are however very convenient, particularily the automatic error propagation which helps makes code clearer and easier to read, write and maintain.
In the journey to try and get a working alternative to standard C++ exceptions for error handling, there are many modern return value based facilities out there,
and even macro based utilities to do the "automatic" propagation. There are also known papers and proposals to improve/introduce new and better form of exceptions
in C++ which I hope will become part of future standard.

So far there is no solution existing today that I know of, that is "close enough" to the experience of using plain C++ exceptions, that I consider clean, meaing,
with automatic and invisible error propagaion, and without polluting the code with macros.

Motivation
----------
With this library I am trying to provide a `coroutine` based implementation that resembles
C++ exceptions as close as I could get. I made sure that it works also
whenn compiling without exceptions and RTTI (i.e `-fno-exceptions -fno-rtti`).

I have not set a goal that this would be a valid solution for performance critical code, however, I am looking for
advice if available how to improve the performance and reduce overhead.

Contents
--------

### Introduction / Hello World
Let's take a look at the following code:
```cpp
zpp::throwing<int> foo(bool success)
{
    if (!success) {
        // Throws an exception.
        co_yield std::runtime_error("My runtime error");
    }

    // Returns a value.
    co_return 1337;
}
```

Foo returns a `zpp::throwing<int>` which means it can throw an exception
in our implementation, but on success it returns an `int`.
```cpp
int main()
{
    return zpp::try_catch([]() -> zpp::throwing<int> {
        std::cout << "Hello World\n";
        std::cout << co_await foo(false) << '\n';;
        co_return 0;
    }).catches([&](const std::exception & error) {
        std::cout << "std exception caught: " << error.what() << '\n';
        return 1;
    }, [&]() {
        std::cout << "Unknown exception\n";
        return 1;
    });
}
```
The `zpp::try_catch` function attempts to execute the function object that is sent to it, and when an exception is thrown,
the following `catches` function will invoke the function object that receives an exception from the appropriate type, similar to a C++
catch block. The last function object can optionally receive no parameters and as such
functions as `catch (...)` to catch all exceptions.

In this example `foo` was called with `false`, and hence it throws the `std::runtime_error` exception which
is caught at the first lambda function sent to `catches`. This lambda then returns `1` which eventually
being returned from `main`.

An abbreviated way of the above is to omit the `catches` like so:
```cpp
int main()
{
    return zpp::try_catch([]() -> zpp::throwing<int> {
        std::cout << "Hello World\n";
        std::cout << co_await foo(false) << '\n';;
        co_return 0;
    }, [&](const std::exception & error) {
        std::cout << "std exception caught: " << error.what() << '\n';
        return 1;
    }, [&]() {
        std::cout << "Unknown exception\n";
        return 1;
    });
}
```


### Throwing Exceptions From Within `catches`
Like in normal catch block, it is possible to throw from the lambdas sent to `catches`:
```cpp
zpp::throwing<std::string> bar(bool success)
{
    co_return co_await zpp::try_catch([&]() -> zpp::throwing<std::string> {
        auto foo_result = co_await foo(success);
        std::cout << "Foo succeeded" << foo_result << '\n';
        co_return "foo succeeded";
    }).catches([&] (const std::runtime_error & error) -> zpp::throwing<std::string> {
        std::cout << "Runtime error caught: " << error.what() << '\n';
        co_yield std::runtime_error("Foo really failed");
    });
}
```

Note that the lambda function sent to `catches` now returns a `zpp::throwing<std::string>`
which allows it to throw exceptions using `co_yield`. It also makes the `catches` function
be a coroutine by itself, which we await with `co_await`.

It is even possible to rethrow the caught excpetion using `zpp::rethrow`:
```cpp
zpp::throwing<std::string> bar(bool success)
{
    co_return co_await zpp::try_catch([&]() -> zpp::throwing<std::string> {
        auto foo_result = co_await foo(success);
        std::cout << "Foo succeeded" << foo_result << '\n';
        co_return "foo succeeded";
    }).catches([&] (const std::runtime_error & error) -> zpp::throwing<std::string> {
        cout << "Runtime error caught: " << error.what() << '\n';
        co_yield zpp::rethrow;
    });
}
```

You may observe that once we change `bar()` to catch `std::logic_error` instead,
the exception is actually caught by the `main()` function below as an `std::exception`:
```cpp
zpp::throwing<std::string> bar(bool success)
{
    co_return co_await zpp::try_catch([&]() -> zpp::throwing<std::string> {
        auto foo_result = co_await foo(success);
        std::cout << "Foo succeeded" << foo_result << '\n';
        co_return "foo succeeded";
    }).catches([&] (const std::logic_error & error) -> zpp::throwing<std::string> {
        std::cout << "Logic error caught: " << error.what() << '\n';
        co_yield std::runtime_error("Foo really failed");
    });
}

int main()
{
    return zpp::try_catch([]() -> zpp::throwing<int> {
        std::cout << "Hello World\n";
        std::cout << co_await bar(false) << '\n';;
        co_return 0;
    }).catches([&](const std::exception & error) {
        std::cout << "std exception caught: " << error.what() << '\n';
        return 1;
    }, [&]() {
        std::cout << "Unknown exception\n";
        return 1;
    });
}
```

### Registering Custom Exception Types
Since at the time of writing, there is no way in C++ to iterate base classes of a given type, manual registration of
exception types is required. The following example shows how to do it:
```cpp
struct my_custom_exception { virtual ~my_custom_exception() = default; };
struct my_custom_derived_exception : my_custom_exception {};

template <>
struct zpp::define_exception<my_custom_exception>
{
    using type = zpp::define_exception_bases<>;
};

template <>
struct zpp::define_exception<my_custom_derived_exception>
{
    using type = zpp::define_exception_bases<my_custom_exception>;
};
```

And then throw it like so:
```cpp
zpp::throwing<int> foo(bool success)
{
    if (!success) {
        // Throws an exception.
        co_yield my_custom_derived_exception();
    }

    // Returns a value.
    co_return 1337;
}
```

### Throwing Values (Inspired by P0709)
```cpp
int main()
{
    zpp::try_catch([]() -> zpp::throwing<void> {
        // Throws an exception.
        co_yield std::errc::invalid_argument;
    }).catches([](zpp::error error) {
        std::cout << "Error: " << error.code() <<
            " [" << error.domain().name() << "]: " << error.message() << '\n';
    }, [](){
        /* catch all */
    });
}
```
In the `main` function above an error value is thrown, from a predefined error
domain of `std::errc`.

In order to define your own error domains, the following example is provided:
```cpp
enum class my_error
{
    success = 0,
    operation_not_permitted = 1,
    general_failure = 2,
};

template <>
inline constexpr auto zpp::err_domain<my_error> = zpp::make_error_domain(
        "my_error", my_error::success, [](auto code) constexpr->std::string_view {
    switch (code) {
    case my_error::operation_not_permitted:
        return "Operation not permitted.";
    case my_error::general_failure:
        return "General failure.";
    default:
        return "Unspecified error.";
    }
});
```

You can even catch the error enumeration directly, like so:
```cpp
int main()
{
    zpp::try_catch([]() -> zpp::throwing<void> {
        // Throws an exception.
        co_yield std::errc::invalid_argument;
    }).catches([](std::errc error) {
        switch(error) {
        case std::errc::invalid_argument: {
            std::cout << "Caught the invalid argument directly by enumeration type\n"
                << "And here is the message: " << zpp::error(error).message() << '\n';
            break;
        }
        default: {
            zpp::error my_error = error;
            std::cout << "Error: " << my_error.code() <<
                " [" << my_error.domain().name() << "]: " << my_error.message() << '\n';
        }
        }
    }, [](){
        /* catch all */
    });
}
```

### Optimize Calls Between Translation Units
Currently for HALO optimization to work, the ramp function of the coroutine needs to be
accessible to inline for the compiler. It means that between different translation units
the coroutines may have to allocate memory for the state object. To overcome this, it is possible
to use `zpp::thrown<Type>`, in the following way:
```cpp
// a.cpp
static zpp::throwing<int> a_foo_impl()
{
    co_yield std::errc::address_in_use;
    co_return 0;
}

zpp::thrown<int> a_foo()
{
    // Implicitly converted to zpp::thrown.
    return a_foo_impl();
}

// main.cpp
int main()
{
    return zpp::try_catch([]() -> zpp::throwing<int> {
        // Awaiting the thrown type.
        std::cout << co_await a_foo() << '\n';
    }).catches([](zpp::error error) {
        std::cout << "Error: " << error.code() <<
            " [" << error.domain().name() << "]: " << error.message() << '\n';
        return 1;
    }, [&]() {
        std::cout << "Unknown exception\n";
        return 1;
    });
}
```

Similarily, we could avoid declaring `a_foo_impl` and do the following:
```cpp
zpp::thrown<int> a_foo()
{
    return [&]() -> zpp::throwing<int> {
        co_yield std::errc::address_in_use;
        co_return 0;
    }();
}
```

The example above uses the implicit conversion of `zpp::throwing<Type>` to `zpp::thrown<Type>`.
Returning this `zpp::thrown` with normal `return` makes makes `a_foo` a normal function.
As such does not attempt to allocate a coroutine state object.
`zpp::thrown<Type>` exposes `operator co_await` to be able to get the
value or propagate the error in case of failure.

### Throwing Exceptions with `co_yield` vs `co_return`
There are really two options the library offers to throw an exception, and you can actually
throw also with `co_return`. The library will understand whether you are actually returning
a value or throwing, by the type of the return expression.

What you need to know is that `co_return` seems to generate less code because it cannot be resumed,
but it cannot be used for `zpp::throwing<void>`. If you want to throw exceptions with `co_return`
in void returning functions, use `zpp::throwing<zpp::void_t>` instead, and `co_return zpp::void_v`.

Example:
```cpp
zpp::throwing<int> foo(bool success)
{
    if (!success) {
        // Throws an exception, with `co_return`.
        co_return std::runtime_error("My runtime error");
    }

    // ...

    if (!success) {
        // Throwing values with `co_return` is also possible.
        co_return std::errc::invalid_argument;
    }

    // Returns a value.
    co_return 1337;
}
```

### Fully-Working Example
As a final example, here is a full program to play with:
```cpp
#include "zpp_throwing.h"
#include <string>
#include <iostream>

zpp::throwing<int> foo(bool success)
{
    if (!success) {
        // Throws an exception.
        co_yield std::runtime_error("My runtime error");
    }

    // Returns a value.
    co_return 1337;
}

zpp::throwing<std::string> bar(bool success)
{
    co_return co_await zpp::try_catch([&]() -> zpp::throwing<std::string> {
        auto foo_result = co_await foo(success);
        std::cout << "Foo succeeded" << foo_result << '\n';
        co_return "foo succeeded";
    }).catches([&] (const std::runtime_error & error) -> zpp::throwing<std::string> {
        std::cout << "Runtime error caught: " << error.what() << '\n';
        co_return "foo failed";
    });
}

zpp::throwing<std::string> foobar()
{
    auto result = co_await bar(false);
    if (result.find("foo succeeded") == std::string::npos) {
        co_yield std::runtime_error("bar() apparently succeeded even though foo() failed");
    }

    co_return result;
}

int main()
{
    return zpp::try_catch([]() -> zpp::throwing<int> {
        std::cout << "Hello World\n";
        std::cout << co_await foobar() << '\n';;
        co_return 0;
    }).catches([&](const std::exception & error) {
        std::cout << "std exception caught: " << error.what() << '\n';
        return 1;
    }, [&]() {
        std::cout << "Unknown exception\n";
        return 1;
    });
}
```

Limitations / Caveats
---------------------
1. The code currently assumes that no exceptions can ever be thrown and as such
it is not recommended to use it in a project where exceptions are enabled.
2. Because coroutines cannot work with constructors, it means that an exception
cannot propagate from constructors natively, and it needs to be worked around, through a parameter
to the constructor or other means such as factory functions.
3. The code has gone only through very minimal testing, with recent clang compiler.
4. The code requires `C++20` and above.
5. You must catch every dynamic exception that you throw, otherwise a memory leak of the exception object
will occur, this is to optimize the non-trivial destruction that happens when propagating the exception.

Final Word
----------
Please submit any feedback you can via issues, I would gladly accept any suggestions to
improve the usage experience, performance, reference to other similar implementations,
and of course bug reports.
