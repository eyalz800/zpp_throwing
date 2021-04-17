#ifndef ZPP_THROWING_H
#define ZPP_THROWING_H

#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>

#if __has_include(<coroutine>)
#include <coroutine>
#else
#include <experimental/coroutine>
#endif

namespace zpp
{
#if __has_include(<coroutine>)
template <typename... Arguments>
using coroutine_handle = std::coroutine_handle<Arguments...>;
using suspend_always = std::suspend_always;
using suspend_never = std::suspend_never;
#else
template <typename... Arguments>
using coroutine_handle = std::experimental::coroutine_handle<Arguments...>;
using suspend_always = std::experimental::suspend_always;
using suspend_never = std::experimental::suspend_never;
#endif

/**
 * Yield `rethrow` for rethrowing exceptions.
 */
struct rethrow_t
{
};
constexpr rethrow_t rethrow;

/**
 * Set the current exception without throwing it, for internal use.
 */
struct set_current_exception_t
{
};
constexpr set_current_exception_t set_current_exception;

/**
 * Exception object type erasure.
 */
class exception_object
{
public:
    struct dynamic_object
    {
        const void * type_id{};
        void * address{};
    };

    virtual dynamic_object dynamic_object() noexcept = 0;
    virtual ~exception_object() = 0;
};

inline exception_object::~exception_object() = default;

/**
 * Define base classes for a particular exception.
 */
template <typename... Bases>
struct define_exception_bases
{
};

/**
 * Define an exception to be supported by the framework.
 * ```cpp
 * template<>
 * struct zpp::define_exception<my_exception>
 * {
 *     using type = zpp::define_exception_bases<exception_base_1,
 * exception_base_2>;
 * };
 * ```
 */
template <typename Type>
struct define_exception;

template <typename Type>
using define_exception_t = typename define_exception<Type>::type;

namespace detail
{
union type_info_entry
{
    constexpr type_info_entry(std::size_t number) : number(number)
    {
    }

    constexpr type_info_entry(const void * pointer) : pointer(pointer)
    {
    }

    constexpr type_info_entry(void * (*function)(void *)noexcept) :
        function(function)
    {
    }

    std::size_t number;
    const void * pointer;
    void * (*function)(void *);
};

template <typename Source, typename Destination>
void * erased_static_cast(void * source) noexcept
{
    return static_cast<Destination *>(static_cast<Source *>(source));
}

template <typename Source, typename Destination>
constexpr auto make_erased_static_cast() noexcept
{
    return &erased_static_cast<Source, Destination>;
}

template <typename Type>
constexpr auto type_id() noexcept -> const void *;

template <typename Type, typename... Bases>
struct type_information
{
    static_assert((... && std::is_base_of_v<Bases, Type>),
                  "Bases must be base classes of Type.");

    // Construct the type information.
    static constexpr type_info_entry info[] = {
        sizeof...(Bases),    // Number of source classes.
        type_id<Bases>()..., // Source classes type information.
        make_erased_static_cast<Type,
                                Bases>()..., // Casts from derived to
                                             // base.
    };
};

template <typename Type>
struct type_information<Type>
{
    // Construct the type information.
    static constexpr type_info_entry info[] = {
        std::size_t{}, // Number of source classes.
    };
};

template <typename Type, typename... Bases>
constexpr auto type_id(define_exception_bases<Bases...>) noexcept
{
    return &type_information<Type, Bases...>::info;
}

template <typename Type>
constexpr auto type_id(define_exception_bases<>) noexcept
{
    return &type_information<Type>::info;
}

template <typename Type>
constexpr auto type_id() noexcept -> const void *
{
    using type = std::remove_cv_t<std::remove_reference_t<Type>>;
    return detail::type_id<type>(define_exception_t<type>{});
}

inline void * dyn_cast(const void * base,
                       void * most_derived_pointer,
                       const void * most_derived)
{
    // If the most derived and the base are the same.
    if (most_derived == base) {
        return most_derived_pointer;
    }

    // Fetch the type info entries.
    auto type_info_entries =
        reinterpret_cast<const type_info_entry *>(most_derived);

    // The number of base types.
    auto number_of_base_types = type_info_entries->number;

    // The bases type information.
    auto bases = type_info_entries + 1;

    // The erased static cast function matching base.
    auto erased_static_cast = bases + number_of_base_types;

    for (std::size_t index = 0; index < number_of_base_types; ++index) {
        // Converting the most derived to the type whose id is
        // bases[index].number and perform the conversion from this
        // type.
        auto result = dyn_cast(
            base,
            erased_static_cast[index].function(most_derived_pointer),
            bases[index].pointer);

        // If the conversion succeeded, return the result.
        if (result) {
            return result;
        }
    }

    // No conversion was found.
    return 0;
}

template <typename Type>
struct catch_type
    : catch_type<decltype(
          &std::remove_cv_t<std::remove_reference_t<Type>>::operator())>
{
};

template <typename ReturnType, typename Argument>
struct catch_type<ReturnType (*)(Argument)>
{
    using type = Argument;
};

template <typename Type, typename ReturnType, typename Argument>
struct catch_type<ReturnType (Type::*)(Argument)>
{
    using type = Argument;
};

template <typename Type, typename ReturnType, typename Argument>
struct catch_type<ReturnType (Type::*)(Argument) const>
{
    using type = Argument;
};

template <typename ReturnType, typename Argument>
struct catch_type<ReturnType (*)(Argument) noexcept>
{
    using type = Argument;
};

template <typename Type, typename ReturnType, typename Argument>
struct catch_type<ReturnType (Type::*)(Argument) noexcept>
{
    using type = Argument;
};

template <typename Type, typename ReturnType, typename Argument>
struct catch_type<ReturnType (Type::*)(Argument) const noexcept>
{
    using type = Argument;
};

template <typename ReturnType>
struct catch_type<ReturnType (*)()>
{
    using type = void;
};

template <typename Type, typename ReturnType>
struct catch_type<ReturnType (Type::*)()>
{
    using type = void;
};

template <typename Type, typename ReturnType>
struct catch_type<ReturnType (Type::*)() const>
{
    using type = void;
};

template <typename ReturnType>
struct catch_type<ReturnType (*)() noexcept>
{
    using type = void;
};

template <typename Type, typename ReturnType>
struct catch_type<ReturnType (Type::*)() noexcept>
{
    using type = void;
};

template <typename Type, typename ReturnType>
struct catch_type<ReturnType (Type::*)() const noexcept>
{
    using type = void;
};

template <typename Type>
using catch_type_t = typename catch_type<Type>::type;

template <typename Type>
struct catch_value_type
{
    using type =
        std::remove_cv_t<std::remove_reference_t<catch_type_t<Type>>>;
};

template <typename Type>
using catch_value_type_t = typename catch_value_type<Type>::type;

} // namespace detail

/**
 * Use as the return type of the function, throw exceptions
 * by using `co_yield`, call throwing functions by `co_await`, and
 * return values using `co_return`.
 *
 * Use the `zpp::try_catch` function to execute a function object and then
 * to catch exceptions thrown from it.
 */
template <typename Type>
class throwing
{
    struct empty_value
    {
        ~empty_value() = default;
    };

    template <typename Derived>
    struct real_value
    {
    public:
        decltype(auto) value()
        {
            if constexpr (std::is_same_v<Type, decltype(m_value)>) {
                return (m_value);
            } else {
                return (*m_value);
            }
        }

        template <typename..., typename Dependent = Type>
        void set_value(auto && value) requires(!std::is_void_v<Dependent>)
        {
            if constexpr (!std::is_void_v<Type>) {
                m_value = std::forward<decltype(value)>(value);
            }
            static_cast<Derived &>(*this).is_exception = false;
        }

        template <typename..., typename Dependent = Type>
        void set_value() requires std::is_void_v<Dependent>
        {
            static_cast<Derived &>(*this).is_exception = false;
        }

    protected:
        ~real_value() = default;

        using stored_type = std::conditional_t<
            std::is_trivially_constructible_v<Type> &&
                std::is_nothrow_move_constructible_v<Type>,
            Type,
            std::optional<Type>>;

        stored_type m_value;
    };

    /**
     * The promise stored value.
     */
    class value_type : public std::conditional_t<std::is_void_v<Type>,
                                                 empty_value,
                                                 real_value<value_type>>
    {
        friend real_value<value_type>;

    public:
        auto & exception()
        {
            return *m_exception;
        }

        auto && get_exception_ptr()
        {
            return std::move(m_exception);
        }

        auto is_rethrow() const
        {
            return is_exception && !m_exception;
        }

        void rethrow()
        {
            is_exception = true;
            m_exception = nullptr;
        }

        void set_exception(auto && value)
        {
            m_exception = std::forward<decltype(value)>(value);
            is_exception = true;
        }

        void propagate_exception(auto && value)
        {
            m_exception = value.get_exception_ptr();
            is_exception = true;
        }

        explicit operator bool() const
        {
            return !is_exception;
        }

    private:
        std::unique_ptr<exception_object> m_exception;
        bool is_exception{};
    };

public:
    struct zpp_throwing_tag
    {
    };

    /**
     * The promise type to be extended with return value / return void
     * functionality.
     */
    class basic_promise_type
    {
    public:
        template <typename>
        friend class throwing;

        auto initial_suspend() noexcept
        {
            return suspend_never{};
        }

        auto final_suspend() noexcept
        {
            return suspend_always{};
        }

        /**
         * Throw an exception, suspends the calling coroutine.
         */
        template <typename Value>
        auto yield_value(Value && value)
        {
            using type = std::remove_cv_t<std::remove_reference_t<Value>>;

            // Define the exception object that will be type erased.
            class exception : public exception_object
            {
            public:
                exception(Value && value) : m_value(std::move(value))
                {
                }

                auto dynamic_object() noexcept
                    -> struct exception_object::dynamic_object override
                {
                    // Return the type id of the exception object and its
                    // address.
                    return {detail::type_id<type>(),
                            std::addressof(m_value)};
                }

                ~exception() override = default;

                type m_value;
            };

            m_value.set_exception(
                std::make_unique<exception>(std::forward<Value>(value)));
            return suspend_always{};
        }

        /**
         * Sets the current exception for rethrow purposes.
         */
        auto yield_value(std::tuple<set_current_exception_t,
                                    std::unique_ptr<exception_object>>
                             current_exception)
        {
            m_value.set_exception(
                std::move(std::get<1>(current_exception)));
            return suspend_never{};
        }

        /**
         * Rethrow an existing exception.
         */
        auto yield_value(std::unique_ptr<exception_object> && exception)
        {
            m_value.set_exception(std::move(exception));
            return suspend_always{};
        }

        /**
         * Rethrow the current set exception.
         */
        auto yield_value(rethrow_t)
        {
            if (!m_value) {
                return suspend_always{};
            }
            m_value.rethrow();
            return suspend_always{};
        }

        void unhandled_exception()
        {
            std::terminate();
        }

    protected:
        auto & value() noexcept
        {
            return m_value;
        }

        explicit operator bool() const noexcept
        {
            return static_cast<bool>(m_value);
        }

        ~basic_promise_type() = default;

        value_type m_value{};
    };

    /**
     * Add the return void functionality to base.
     */
    template <typename Base>
    struct returns_void : public Base
    {
        using Base::Base;

        auto return_void()
        {
            Base::m_value.set_value();
            return suspend_never{};
        }

        auto get_return_object()
        {
            return throwing{coroutine_handle<promise_type>::from_promise(*this)};
        }
    };

    /**
     * Add the return value functionality to base.
     */
    template <typename Base>
    struct returns_value : public Base
    {
        using Base::Base;

        auto return_value(Type value)
        {
            Base::m_value.set_value(std::move(value));
            return suspend_never{};
        }

        auto get_return_object()
        {
            return throwing{coroutine_handle<promise_type>::from_promise(*this)};
        }
    };

    /**
     * The actual promise type, which adds the appropriate
     * return strategy to the basic promise type.
     */
    using promise_type =
        std::conditional_t<std::is_void_v<Type>,
                           returns_void<basic_promise_type>,
                           returns_value<basic_promise_type>>;

    /**
     * Represents the promised value that is stored within this object,
     * ones the coroutine finishes, and the handle is destroyed, the result
     * will be stored here for use with the `zpp::try_catch` logic.
     */
    class promised
    {
        value_type m_value{};

    public:
        friend throwing;

        /**
         * Returns true if value is stored, otherwise, an
         * exception is stored.
         */
        explicit operator bool() const noexcept
        {
            return static_cast<bool>(m_value);
        }

        /**
         * Returns the stored value, the behavior
         * is undefined if there is an exception stored.
         */
        decltype(auto) value() && noexcept
        {
            if constexpr (std::is_void_v<Type>) {
                return;
            } else {
                return std::move(m_value.value());
            }
        }

        /**
         * Returns the stored value, the behavior
         * is undefined if there is an exception stored.
         */
        decltype(auto) value() & noexcept
        {
            if constexpr (std::is_void_v<Type>) {
                return;
            } else {
                return m_value.value();
            }
        }

        /**
         * Returns the stored exception, the behavior
         * is undefined if there is a value stored.
         */
        auto && exception() && noexcept
        {
            return std::move(m_value.exception());
        }

        /**
         * Returns the stored exception, the behavior
         * is undefined if there is a value stored.
         */
        auto & exception() & noexcept
        {
            return m_value.exception();
        }

    private:
        /**
         * Create the promise from the value type.
         */
        explicit promised(value_type && value) noexcept :
            m_value(std::move(value))
        {
        }

        /**
         * Allows to catch exceptions. Each parameter is a catch clause
         * that receives one parameter of the exception to be caught. A
         * catch clause may itself be throwing. The last catch clause may
         * have no parameters and as such catches all exceptions. Must
         * return a value of the same type as the previously executed
         * coroutine that is being checked for exceptions. This overload is
         * for catch clauses that may themselves throw.
         */
        template <typename Clause,
                  typename... Clauses,
                  typename...,
                  typename CatchType = detail::catch_value_type_t<Clause>,
                  bool IsThrowing = requires
        {
            typename std::invoke_result_t<Clause>::zpp_throwing_tag;
        }
        || requires
        {
            typename std::invoke_result_t<
                Clause,
                std::conditional_t<std::is_void_v<CatchType>,
                                   int,
                                   CatchType> &>::zpp_throwing_tag;
        }
        > requires IsThrowing ||
            (... || (requires {
                 typename std::invoke_result_t<Clauses>::zpp_throwing_tag;
             } ||
                     requires {
                         typename std::invoke_result_t<
                             Clauses,
                             std::conditional_t<
                                 std::is_void_v<
                                     detail::catch_value_type_t<Clauses>>,
                                 int,
                                 detail::catch_value_type_t<Clauses>> &>::
                             zpp_throwing_tag;
                     })) ||
            (!requires {
                typename std::invoke_result_t<decltype(
                    std::get<sizeof...(Clauses)>(
                        std::declval<std::tuple<Clause, Clauses...>>()))>;
            })
                throwing<Type> catches_object(
                    const struct exception_object::dynamic_object &
                        exception,
                    Clause && clause,
                    Clauses &&... clauses)
        {
            if constexpr (std::is_void_v<CatchType>) {
                static_assert(0 == sizeof...(Clauses),
                              "Catch all clause must be the last one.");
                if constexpr (IsThrowing) {
                    co_yield{set_current_exception,
                             m_value.get_exception_ptr()};
                    co_return co_await std::forward<Clause>(clause)();
                } else {
                    co_return std::forward<Clause>(clause)();
                }
            } else {
                // Type id of the type to be caught by this catch clause.
                auto catch_type_id = detail::type_id<CatchType>();
                auto catch_object = static_cast<CatchType *>(
                    detail::dyn_cast(catch_type_id,
                                     exception.address,
                                     exception.type_id));
                if (!catch_object) {
                    if constexpr (0 != sizeof...(Clauses)) {
                        if constexpr (requires {
                                          typename decltype(catches_object(
                                              exception,
                                              std::forward<Clauses>(
                                                  clauses)...))::
                                              zpp_throwing_tag;
                                      }) {
                            co_return co_await catches_object(
                                exception,
                                std::forward<Clauses>(clauses)...);
                        } else {
                            co_return catches_object(
                                exception,
                                std::forward<Clauses>(clauses)...);
                        }
                    } else {
                        co_yield m_value.get_exception_ptr();
                    }
                }

                if constexpr (IsThrowing) {
                    co_yield{set_current_exception,
                             m_value.get_exception_ptr()};
                    co_return co_await std::forward<Clause>(clause)(
                        *catch_object);
                } else {
                    co_return std::forward<Clause>(clause)(*catch_object);
                }
            }
        }

        /**
         * Allows to catch exceptions. Each parameter is a catch clause
         * that receives one parameter of the exception to be caught. A
         * catch clause may itself be throwing. The last catch clause may
         * have no parameters and as such catches all exceptions. Must
         * return a value of the same type as the previously executed
         * coroutine that is being checked for exceptions. This overload is
         * for catch clauses that cannot throw.
         */
        template <typename Clause,
                  typename... Clauses,
                  typename...,
                  typename CatchType = detail::catch_value_type_t<Clause>,
                  bool IsThrowing = requires
        {
            typename std::invoke_result_t<Clause>::zpp_throwing_tag;
        }
        || requires
        {
            typename std::invoke_result_t<
                Clause,
                std::conditional_t<std::is_void_v<CatchType>,
                                   int,
                                   CatchType> &>::zpp_throwing_tag;
        }
        > requires(
              !(IsThrowing ||
                (... ||
                 (requires {
                     typename std::invoke_result_t<
                         Clauses>::zpp_throwing_tag;
                 } ||
                  requires {
                      typename std::invoke_result_t<
                          Clauses,
                          std::conditional_t<
                              std::is_void_v<
                                  detail::catch_value_type_t<Clauses>>,
                              int,
                              detail::catch_value_type_t<Clauses>> &>::
                          zpp_throwing_tag;
                  })) ||
                (!requires {
                    typename std::invoke_result_t<decltype(
                        std::get<sizeof...(Clauses)>(
                            std::declval<
                                std::tuple<Clause, Clauses...>>()))>;
                }))) auto catches_object(const struct exception_object::
                                             dynamic_object & exception,
                                         Clause && clause,
                                         Clauses &&... clauses)
        {
            if constexpr (std::is_void_v<CatchType>) {
                static_assert(!sizeof...(Clauses),
                              "Catch all object with no parameters must "
                              "be the last one.");
                return std::forward<Clause>(clause)();
            } else {
                auto catch_type_id = detail::type_id<CatchType>();
                auto catch_object = static_cast<CatchType *>(
                    detail::dyn_cast(catch_type_id,
                                     exception.address,
                                     exception.type_id));
                if (!catch_object) {
                    static_assert(0 != sizeof...(Clauses),
                                  "Missing catch all block in non "
                                  "throwing catches.");
                    return catches_object(
                        exception, std::forward<Clauses>(clauses)...);
                }

                return std::forward<Clause>(clause)(*catch_object);
            }
        }

    public:
        /**
         * Allows to catch exceptions. Each parameter is a catch clause
         * that receives one parameter of the exception to be caught. A
         * catch clause may itself be throwing. The last catch clause may
         * have no parameters and as such catches all exceptions. Must
         * return a value of the same type as the previously executed
         * coroutine that is being checked for exceptions. This overload is
         * for catch clauses that may themselves throw.
         */
        template <typename... Clauses>
        throwing<Type> catches(Clauses &&... clauses) requires requires
        {
            typename decltype(this->catches_object(
                this->exception().dynamic_object(),
                std::forward<Clauses>(clauses)...))::zpp_throwing_tag;
        }
        {
            // If there is no exception, skip.
            if (*this) {
                if constexpr (std::is_void_v<Type>) {
                    co_return;
                } else {
                    co_return std::move(value());
                }
            }

            // Follow to catch the exception.
            co_return co_await catches_object(
                exception().dynamic_object(),
                std::forward<Clauses>(clauses)...);
        }

        /**
         * Allows to catch exceptions. Each parameter is a catch clause
         * that receives one parameter of the exception to be caught. A
         * catch clause may itself be throwing. The last catch clause may
         * have no parameters and as such catches all exceptions. Must
         * return a value of the same type as the previously executed
         * coroutine that is being checked for exceptions. This overload is
         * for catch clauses that do not throw.
         */
        template <typename... Clauses>
        Type catches(Clauses &&... clauses) requires(!requires {
            typename decltype(this->catches_object(
                this->exception().dynamic_object(),
                std::forward<Clauses>(clauses)...))::zpp_throwing_tag;
        })
        {
            // If there is no exception, skip.
            if (*this) {
                if constexpr (std::is_void_v<Type>) {
                    return;
                } else {
                    return std::move(value());
                }
            }

            // Follow to catch the exception.
            return catches_object(exception().dynamic_object(),
                                  std::forward<Clauses>(clauses)...);
        }
    };

    /**
     * Construct from the coroutine handle.
     */
    explicit throwing(coroutine_handle<promise_type> handle) noexcept :
        m_handle(std::move(handle))
    {
    }

    /**
     * Move construct from another object.
     */
    throwing(throwing && other) noexcept :
        m_handle(std::move(other.m_handle))
    {
        other.m_handle = nullptr;
    }

    throwing(const throwing & other) = delete;
    throwing & operator=(throwing && other) noexcept = delete;
    throwing & operator=(const throwing & other) = delete;

    /**
     * Await is ready if there is no exception.
     */
    bool await_ready() noexcept
    {
        return static_cast<bool>(m_handle.promise());
    }

    /**
     * Suspend execution only if there is an exception to be thrown.
     */
    template <typename PromiseType>
    void await_suspend(coroutine_handle<PromiseType> outer_handle) noexcept
    {
        auto & value = m_handle.promise().value();
        auto & outer_promise = outer_handle.promise();

        if (!value.is_rethrow()) {
            // Throw.
            outer_promise.value().propagate_exception(value);
        } else if (outer_promise) {
            // Rethrow.
            outer_promise.value().rethrow();
        }
    }

    /**
     * Return the stored value on resume.
     */
    decltype(auto) await_resume() noexcept
    {
        if constexpr (std::is_void_v<Type>) {
            return;
        } else {
            return m_handle.promise().value().value();
        }
    }

    /**
     * Construct and return the promised value.
     */
    auto result() noexcept
    {
        return promised(std::move(m_handle.promise().value()));
    }

    /**
     * Destroy the coroutine handle.
     */
    ~throwing()
    {
        if (m_handle) {
            m_handle.destroy();
        }
    }

private:
    coroutine_handle<promise_type> m_handle;
};

/**
 * Use to try executing a function object and catch exceptions from it.
 * This also neatly makes sure in an implicit way that destructors are
 * called before the catch blocks are called, due to the returned coroutine
 * handle being implicitly destroyed within the function.
 */
template <typename Clause>
auto try_catch(Clause && clause)
{
    if constexpr (requires(Clause && block) {
                      &decltype(std::forward<Clause>(
                          clause)())::promise_type::get_return_object;
                  }) {
        return std::forward<Clause>(clause)().result();
    } else {
        static_assert(std::is_void_v<Clause>,
                      "try_catch clause must be a coroutine.");
    }
}

template <>
struct define_exception<std::exception>
{
    using type = define_exception_bases<>;
};

template <>
struct define_exception<std::runtime_error>
{
    using type = define_exception_bases<std::exception>;
};

template <>
struct define_exception<std::range_error>
{
    using type = define_exception_bases<std::runtime_error>;
};

template <>
struct define_exception<std::overflow_error>
{
    using type = define_exception_bases<std::runtime_error>;
};

template <>
struct define_exception<std::underflow_error>
{
    using type = define_exception_bases<std::runtime_error>;
};

template <>
struct define_exception<std::logic_error>
{
    using type = define_exception_bases<std::exception>;
};

template <>
struct define_exception<std::invalid_argument>
{
    using type = define_exception_bases<std::logic_error>;
};

template <>
struct define_exception<std::domain_error>
{
    using type = define_exception_bases<std::logic_error>;
};

template <>
struct define_exception<std::length_error>
{
    using type = define_exception_bases<std::logic_error>;
};

template <>
struct define_exception<std::out_of_range>
{
    using type = define_exception_bases<std::logic_error>;
};

} // namespace zpp

#endif // ZPP_THROWING_H
