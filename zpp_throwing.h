#ifndef ZPP_THROWING_H
#define ZPP_THROWING_H

#include <memory>
#include <new>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>

#if __has_include(<coroutine>)
#include <coroutine>
#else
#include <experimental/coroutine>
#endif

namespace zpp
{
namespace detail
{
enum reserved_index : std::size_t;

template <std::size_t Size>
struct reserved_storage
{
    static constexpr bool reserved(void * pointer) noexcept
    {
        return (std::begin(storage) <= pointer &&
                pointer < std::end(storage));
    }

    template <typename Type>
    static auto at(enum reserved_index index) noexcept
    {
        return const_cast<std::remove_const_t<Type *>>(
            reinterpret_cast<std::add_const_t<Type> *>(
                std::begin(storage) + index));
    }

    static auto reserved_index(auto pointer) noexcept
    {
        return (enum reserved_index)(
            reinterpret_cast<const char *>(pointer) - std::begin(storage));
    }

    constexpr static char storage[Size]{};
};

template <typename Type, std::size_t Size>
struct reserved_delete
{
    void operator()(Type * pointer)
    {
        if (!reserved_storage<Size>::reserved(pointer)) {
            delete pointer;
        }
    }
};

template <typename Type, std::size_t Size>
requires(!std::is_array_v<Type>) class reserved_ptr
    : private std::unique_ptr<Type, reserved_delete<Type, Size>>
{
public:
    using base = std::unique_ptr<Type, reserved_delete<Type, Size>>;
    using base::base;
    using base::get;
    using base::release;
    using base::reset;
    using base::operator*;
    using base::operator->;
    friend auto operator<=>(const reserved_ptr &,
                            const reserved_ptr &) = default;

    reserved_ptr(reserved_index index) :
        base(reserved_storage<Size>::template at<Type>(index))
    {
    }

    reserved_ptr(std::unique_ptr<Type> && other) : base(other.release())
    {
    }

    constexpr explicit operator bool() const noexcept
    {
        return get() != nullptr && !reserved();
    }

    void swap(reserved_ptr & other) noexcept
    {
        other.swap(*this);
    }

    auto reserved_index() const noexcept
    {
        return reserved_storage<Size>::reserved_index(get());
    }

    constexpr bool reserved() const noexcept
    {
        return reserved_storage<Size>::reserved(get());
    }

    void reset(enum reserved_index) noexcept
    {
        base::reset(reserved_storage<Size>::template at<Type>(index));
    }

    reserved_ptr & operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    reserved_ptr & operator=(std::unique_ptr<Type> && other) noexcept
    {
        reset(other.release());
        return *this;
    }
};

template <typename Type, std::size_t Size>
constexpr auto operator==(const reserved_ptr<Type, Size> & left,
                          std::nullptr_t) noexcept
{
    return left.get() == nullptr;
}

template <typename Type, std::size_t Size>
constexpr auto operator==(std::nullptr_t,
                          const reserved_ptr<Type, Size> & right) noexcept
{
    return right.get() == nullptr;
}

template <typename Type, std::size_t Size>
constexpr auto operator!=(const reserved_ptr<Type, Size> & left,
                          std::nullptr_t) noexcept
{
    return left.get() != nullptr;
}

template <typename Type, std::size_t Size>
constexpr auto operator!=(std::nullptr_t,
                          const reserved_ptr<Type, Size> & right) noexcept
{
    return right.get() != nullptr;
}

template <typename Type, std::size_t Size>
auto make_reserved(auto &&... arguments)
{
    return reserved_ptr<Type, Size>(
        new Type(std::forward<decltype(arguments)>(arguments)...));
}

template <typename Type, std::size_t Size>
constexpr auto operator==(const reserved_ptr<Type, Size> & left,
                          reserved_index right) noexcept
{
    return left.reserved() && left.reserved_index() == right;
}

template <typename Type, std::size_t Size>
constexpr auto operator==(reserved_index left,
                          const reserved_ptr<Type, Size> & right) noexcept
{
    return right.reserved() && right.reserved_index() == left;
}

template <typename Type, std::size_t Size>
constexpr auto operator!=(const reserved_ptr<Type, Size> & left,
                          reserved_index right) noexcept
{
    return !(left == right);
}

template <typename Type, std::size_t Size>
constexpr auto operator!=(reserved_index left,
                          const reserved_ptr<Type, Size> & right) noexcept
{
    return !(left == right);
}

template <typename Type, std::size_t Size>
auto make_reserved(reserved_index index)
{
    return reserved_ptr<Type, Size>(index);
}
} // namespace detail

/**
 * User defined error domain.
 */
template <typename ErrorCode>
std::conditional_t<std::is_void_v<ErrorCode>, ErrorCode, void> err_domain;

/**
 * The error domain which responsible for translating error codes to
 * error messages.
 */
class error_domain
{
public:
    /**
     * Returns the error domain name.
     */
    virtual std::string_view name() const noexcept = 0;

    /**
     * Return the error message for a given error code.
     * For success codes, it is unspecified what value is returned.
     * For convienience, you may return zpp::error::no_error for success.
     * All other codes must return non empty string views.
     */
    virtual std::string_view message(int code) const noexcept = 0;

    /**
     * Returns true if success code, else false.
     */
    bool success(int code) const
    {
        return code == m_success_code;
    }

protected:
    /**
     * Creates an error domain whose success code is 'success_code'.
     */
    constexpr error_domain(int success_code) : m_success_code(success_code)
    {
    }

    /**
     * Destroys the error domain.
     */
    ~error_domain() = default;

private:
    /**
     * The success code.
     */
    int m_success_code{};
};

/**
 * Creates an error domain, whose name and success
 * code are specified, as well as a message translation
 * logic that returns the error message for every error code.
 * Note: message translation must not throw.
 */
template <typename ErrorCode, typename Messages>
constexpr auto make_error_domain(std::string_view name,
                                 ErrorCode success_code,
                                 Messages && messages)
{
    // Create a domain with the name and messages.
    class domain final : public error_domain,
                         private std::remove_reference_t<Messages>
    {
    public:
        constexpr domain(std::string_view name,
                         ErrorCode success_code,
                         Messages && messages) :
            error_domain(std::underlying_type_t<ErrorCode>(success_code)),
            std::remove_reference_t<Messages>(
                std::forward<Messages>(messages)),
            m_name(name)
        {
        }

        std::string_view name() const noexcept override
        {
            return m_name;
        }

        std::string_view message(int code) const noexcept override
        {
            return this->operator()(ErrorCode{code});
        }

    private:
        std::string_view m_name;
    } domain(name, success_code, std::forward<Messages>(messages));

    // Return the domain.
    return domain;
}

/**
 * Represents an error to be initialized from an error code
 * enumeration.
 * The error code enumeration must have 'int' as underlying type.
 * Defining an error code enum and a domain for it goes as follows.
 * Example:
 * ```cpp
 * enum class my_error
 * {
 *     success = 0,
 *     operation_not_permitted = 1,
 *     general_failure = 2,
 * };
 *
 * template <>
 * inline constexpr auto zpp::err_domain<my_error> = zpp::make_error_domain(
 *         "my_error", my_error::success, [](auto code) constexpr->std::string_view {
 *     switch (code) {
 *     case my_error::operation_not_permitted:
 *         return "Operation not permitted.";
 *     case my_error::general_failure:
 *         return "General failure.";
 *     default:
 *         return "Unspecified error.";
 *     }
 * });
 * ```
 */
class error
{
public:
    /**
     * Disables default construction.
     */
    error() = delete;

    /**
     * Constructs an error from an error code enumeration, the
     * domain is looked by using argument dependent lookup on a
     * function named 'domain' that receives the error code
     * enumeration value.
     */
    template <typename ErrorCode>
    error(ErrorCode error_code) requires std::is_enum_v<ErrorCode>
        : m_domain(std::addressof(err_domain<ErrorCode>)),
          m_code(std::underlying_type_t<ErrorCode>(error_code))
    {
    }

    /**
     * Constructs an error from an error code enumeration, the
     * domain is given explicitly in this overload.
     */
    template <typename ErrorCode>
    error(ErrorCode error_code, const error_domain & domain) :
        m_domain(std::addressof(domain)),
        m_code(static_cast<int>(error_code))
    {
    }

    /**
     * Returns the error domain.
     */
    const error_domain & domain() const
    {
        return *m_domain;
    }

    /**
     * Returns the error code.
     */
    int code() const
    {
        return m_code;
    }

    /**
     * Returns the error message. Calling this on
     * a success error is implementation defined according
     * to the error domain.
     */
    std::string_view message() const
    {
        return m_domain->message(m_code);
    }

    /**
     * Returns true if the error indicates success, else false.
     */
    explicit operator bool() const
    {
        return m_domain->success(m_code);
    }

    /**
     * No error message value.
     */
    static constexpr std::string_view no_error{};

private:
    /**
     * The error domain.
     */
    const error_domain * m_domain{};

    /**
     * The error code.
     */
    int m_code{};
};

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

struct dynamic_object
{
    const void * type_id{};
    void * address{};
};

/**
 * Exception object type erasure.
 */
class exception_object
{
public:
    virtual struct dynamic_object dynamic_object() noexcept = 0;
    virtual ~exception_object() = 0;

    static constexpr struct dynamic_object null_dynamic_object = {};
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
 * Maximum value of an error code.
 */
static constexpr auto error_code_max = 255 - 1 /* reserved */;

/**
 * The promise stored value.
 */
template <typename Type>
class promised_value
{
public:
    promised_value() : m_error_domain()
    {
    }

    promised_value(promised_value && other) noexcept(
        noexcept(std::is_nothrow_move_assignable_v<Type>)) :
        m_error_state(std::move(other.m_error_state))
    {
        if (m_error_state == value_index) {
            if constexpr (!std::is_void_v<Type>) {
                ::new (std::addressof(m_value))
                    Type(std::move(other.m_value));
                if constexpr (!std::is_trivially_destructible_v<Type>) {
                    other.m_value.~Type();
                }
            }
        } else if (m_error_state.reserved()) {
            m_error_domain = other.m_error_domain;
        }
    }

    promised_value & operator=(promised_value other) noexcept(
        noexcept(std::is_nothrow_move_constructible_v<Type> &&
                     std::is_nothrow_move_assignable_v<Type>))
    {
        if (*this) {
            if (other) {
                m_value = std::move(other.m_value);
                m_error_state = value_index;
            } else {
                if constexpr (!std::is_trivially_destructible_v<Type>) {
                    m_value.~Type();
                }
                m_error_state = std::move(other.m_error_state);
                m_error_domain = other.m_error_domain;
            }
        } else if (other) {
            ::new (std::addressof(m_value)) Type(std::move(other.m_value));
            m_error_state = value_index;
        } else {
            m_error_state = std::move(other.m_error_state);
            m_error_domain = other.m_error_domain;
        }
        return *this;
    }

    ~promised_value()
    {
        if (m_error_state == value_index) {
            if constexpr (!std::is_void_v<Type> &&
                          !std::is_trivially_destructible_v<Type>) {
                m_value.~Type();
            }
        }
    }

    promised_value(const promised_value &) = delete;

    explicit operator bool() const noexcept
    {
        return m_error_state == value_index;
    }

    bool has_exception() const noexcept
    {
        return static_cast<bool>(m_error_state);
    }

    bool has_error() const noexcept
    {
        return m_error_state.reserved() && m_error_state != value_index;
    }

    decltype(auto) value()
    {
        if constexpr (std::is_same_v<Type, decltype(m_value)>) {
            return (m_value);
        } else {
            return (*m_value);
        }
    }

    /**
     * Sets a value. `this` should not hold a value.
     */
    template <typename..., typename Dependent = Type>
    void set_value(auto && other) requires(!std::is_void_v<Dependent>)
    {
        if constexpr (!std::is_void_v<Type>) {
            ::new (&m_value) Type(std::forward<decltype(other)>(other));
        }
        m_error_state = value_index;
    }

    template <typename..., typename Dependent = Type>
    void set_value() requires std::is_void_v<Dependent>
    {
        m_error_state = value_index;
    }

    auto & exception()
    {
        return *m_error_state;
    }

    auto is_rethrow() const
    {
        return m_error_state == nullptr;
    }

    void rethrow()
    {
        m_error_state = nullptr;
    }

    /**
     * Sets an exception object. `this` should not hold a value.
     */
    void
    set_exception(std::unique_ptr<exception_object> && exception) noexcept
    {
        m_error_state = std::move(exception);
    }

    /**
     * Sets an error value, error must not be larger than  `error_code_max,
     * and `this` should not hold a value.
     */
    void set_error(const error & error) noexcept
    {
        auto code = error.code();
        if (code > error_code_max) {
            code = error_code_max;
        }
        m_error_state = detail::reserved_index(1 + code);
        m_error_domain = &error.domain();
    }

    error get_error() const noexcept
    {
        return error(m_error_state.reserved_index() - 1, *m_error_domain);
    }

    /**
     * Propagates an exception/error value, both this and other must
     * have no value stored.
     */
    void propagate(auto && other) noexcept
    {
        m_error_state = std::move(other.m_error_state);
        m_error_domain = other.m_error_domain;
    }

    static constexpr auto value_index = detail::reserved_index(0);

    detail::reserved_ptr<exception_object, 1 + error_code_max + 1>
        m_error_state;

    union
    {
        const error_domain * m_error_domain{};
        std::conditional_t<std::is_void_v<Type>, std::nullptr_t, Type>
            m_value;
    };
};

/**
 * Use as the return type of the function, throw exceptions
 * by using `co_yield`, call throwing functions by `co_await`, and
 * return values using `co_return`.
 *
 * Use the `zpp::try_catch` function to execute a function object and then
 * to catch exceptions thrown from it.
 */
template <typename Type>
class [[nodiscard]] throwing
{
    template <typename>
    friend class result;

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
            return suspend_always{};
        }

        auto final_suspend() noexcept
        {
            return suspend_always{};
        }

        /**
         * Throw an exception, suspends the calling coroutine.
         */
        template <typename Value>
        auto yield_value(Value && value) requires requires
        {
            define_exception<std::remove_reference_t<Value>>();
        }
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
                    -> struct dynamic_object override
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
        template <typename Exception>
        auto yield_value(std::tuple<const set_current_exception_t &,
                                    Exception &> exception)
        {
            m_value.propagate(std::get<1>(exception));
            return suspend_never{};
        }

        /**
         * Rethrow from exising.
         */
        template <typename Exception>
        auto
        yield_value(std::tuple<const rethrow_t &, Exception &> exception)
        {
            m_value.propagate(std::get<1>(exception));
            return suspend_always{};
        }

        /**
         * Rethrow the current set exception.
         */
        auto yield_value(rethrow_t)
        {
            m_value.rethrow();
            return suspend_always{};
        }

        /**
         * Throw an error.
         */
        auto yield_value(const error & error)
        {
            m_value.set_error(error);
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

        promised_value<Type> m_value{};
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
            return throwing{
                coroutine_handle<promise_type>::from_promise(*this)};
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
            return throwing{
                coroutine_handle<promise_type>::from_promise(*this)};
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
    bool await_ready()
    {
        m_handle.resume();
        return static_cast<bool>(m_handle.promise());
    }

    /**
     * Suspend execution only if there is an exception to be thrown.
     */
    template <typename PromiseType>
    void await_suspend(coroutine_handle<PromiseType> outer_handle) noexcept
    {
        auto & value = m_handle.promise().value();

        // Propagate exception unless it is rethrow.
        if (!value.is_rethrow()) {
            outer_handle.promise().value().propagate(value);
        }
    }

    /**
     * Return the stored value on resume.
     */
    auto await_resume() noexcept
    {
        if constexpr (std::is_void_v<Type>) {
            return;
        } else {
            return std::move(m_handle.promise().value().value());
        }
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
    /**
     * Call the function and return the promised value.
     */
    auto invoke()
    {
        m_handle.resume();
        return std::move(m_handle.promise().value());
    }

    coroutine_handle<promise_type> m_handle;
};

/**
 * Represents a value that may contain an exception/error,
 */
template <typename Type>
class [[nodiscard]] result
{
    promised_value<Type> m_value{};

public:
    template <typename>
    friend class throwing;

    /**
     * Create the result from the throwing object.
     */
    result(throwing<Type> throwing) noexcept : m_value(throwing.invoke())
    {
    }

    /**
     * Returns true if value is stored, otherwise, an
     * exception/error is stored.
     */
    explicit operator bool() const noexcept
    {
        return static_cast<bool>(m_value);
    }

    /**
     * Await on the object, throw any stored
     * exception.
     */
    throwing<Type> operator co_await() noexcept
    {
        if (!m_value) {
            co_yield std::tie(rethrow, m_value);
        }
        co_return std::move(m_value.value());
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

private:
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
        (... ||
         (requires {
             typename std::invoke_result_t<Clauses>::zpp_throwing_tag;
         } ||
          requires {
              typename std::invoke_result_t<
                  Clauses,
                  std::conditional_t<
                      std::is_void_v<detail::catch_value_type_t<Clauses>>,
                      int,
                      detail::catch_value_type_t<Clauses>> &>::
                  zpp_throwing_tag;
          })) ||
        (!requires {
            typename std::invoke_result_t<decltype(
                std::get<sizeof...(Clauses)>(
                    std::declval<std::tuple<Clause, Clauses...>>()))>;
        })throwing<Type> catch_exception_object(const dynamic_object &
                                                    exception,
                                                Clause && clause,
                                                Clauses &&... clauses)
    {
        if constexpr (std::is_void_v<CatchType>) {
            static_assert(0 == sizeof...(Clauses),
                          "Catch all clause must be the last one.");
            if constexpr (IsThrowing) {
                co_yield std::tie(set_current_exception, m_value);
                co_return co_await std::forward<Clause>(clause)();
            } else {
                co_return std::forward<Clause>(clause)();
            }
        } else if constexpr (requires {
                                 std::forward<Clause>(clause)(
                                     m_value.get_error());
                             }) {
            if (exception.address) {
                if constexpr (0 != sizeof...(Clauses)) {
                    if constexpr (requires {
                                      typename decltype(
                                          catch_exception_object(
                                              exception,
                                              std::forward<Clauses>(
                                                  clauses)...))::
                                          zpp_throwing_tag;
                                  }) {
                        co_return co_await catch_exception_object(
                            exception, std::forward<Clauses>(clauses)...);
                    } else {
                        co_return catch_exception_object(
                            exception, std::forward<Clauses>(clauses)...);
                    }
                } else {
                    co_yield std::tie(rethrow, m_value);
                }
            }

            if constexpr (IsThrowing) {
                auto error = m_value.get_error();
                co_yield std::tie(set_current_exception, m_value);
                co_return co_await std::forward<Clause>(clause)(error);
            } else {
                co_return std::forward<Clause>(clause)(
                    m_value.get_error());
            }
        } else {
            CatchType * catch_object = nullptr;
            if (exception.address) {
                catch_object = static_cast<CatchType *>(
                    detail::dyn_cast(detail::type_id<CatchType>(),
                                     exception.address,
                                     exception.type_id));
            }

            if (!catch_object) {
                if constexpr (0 != sizeof...(Clauses)) {
                    if constexpr (requires {
                                      typename decltype(
                                          catch_exception_object(
                                              exception,
                                              std::forward<Clauses>(
                                                  clauses)...))::
                                          zpp_throwing_tag;
                                  }) {
                        co_return co_await catch_exception_object(
                            exception, std::forward<Clauses>(clauses)...);
                    } else {
                        co_return catch_exception_object(
                            exception, std::forward<Clauses>(clauses)...);
                    }
                } else {
                    co_yield std::tie(rethrow, m_value);
                }
            }

            if constexpr (IsThrowing) {
                co_yield std::tie(set_current_exception, m_value);
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
            }))) auto catch_exception_object(const dynamic_object &
                                                 exception,
                                             Clause && clause,
                                             Clauses &&... clauses)
    {
        if constexpr (std::is_void_v<CatchType>) {
            static_assert(!sizeof...(Clauses),
                          "Catch all object with no parameters must "
                          "be the last one.");
            return std::forward<Clause>(clause)();
        } else if constexpr (requires {
                                 std::forward<Clause>(clause)(
                                     m_value.get_error());
                             }) {
            if (exception.address) {
                static_assert(0 != sizeof...(Clauses),
                              "Missing catch all block in non "
                              "throwing catches.");
                return catch_exception_object(
                    exception, std::forward<Clauses>(clauses)...);
            }

            return std::forward<Clause>(clause)(m_value.get_error());
        } else {
            CatchType * catch_object = nullptr;
            if (exception.address) {
                catch_object = static_cast<CatchType *>(
                    detail::dyn_cast(detail::type_id<CatchType>(),
                                     exception.address,
                                     exception.type_id));
            }

            if (!catch_object) {
                static_assert(0 != sizeof...(Clauses),
                              "Missing catch all block in non "
                              "throwing catches.");
                return catch_exception_object(
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
    inline throwing<Type> catches(Clauses &&... clauses) requires requires
    {
        typename decltype(this->catch_exception_object(
            exception_object::null_dynamic_object,
            std::forward<Clauses>(clauses)...))::zpp_throwing_tag;
    }
    {
        // If there is no exception, skip.
        if (m_value) {
            if constexpr (std::is_void_v<Type>) {
                co_return;
            } else {
                co_return std::move(value());
            }
        }

        // Follow to catch the exception.
        co_return co_await catch_exception_object(
            // Increase chance `catches` gets inlined.
            [&] {
                return m_value.has_exception()
                           ? m_value.exception().dynamic_object()
                           : exception_object::null_dynamic_object;
            }(),
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
    inline Type catches(Clauses &&... clauses) requires(!requires {
        typename decltype(this->catch_exception_object(
            exception_object::null_dynamic_object,
            std::forward<Clauses>(clauses)...))::zpp_throwing_tag;
    })
    {
        // If there is no exception, skip.
        if (m_value) {
            if constexpr (std::is_void_v<Type>) {
                return;
            } else {
                return std::move(value());
            }
        }

        // Follow to catch the exception.
        return catch_exception_object(
            // Increase chance `catches` gets inlined.
            [&] {
                return m_value.has_exception()
                           ? m_value.exception().dynamic_object()
                           : exception_object::null_dynamic_object;
            }(),
            std::forward<Clauses>(clauses)...);
    }
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
    if constexpr (requires {
                      typename std::invoke_result_t<
                          Clause>::zpp_throwing_tag;
                  }) {
        return result(std::forward<Clause>(clause)());
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

template <>
struct define_exception<std::bad_alloc>
{
    using type = define_exception_bases<std::exception>;
};

template <>
struct define_exception<std::bad_weak_ptr>
{
    using type = define_exception_bases<std::exception>;
};

template <>
struct define_exception<std::bad_exception>
{
    using type = define_exception_bases<std::exception>;
};

template <>
struct define_exception<std::bad_cast>
{
    using type = define_exception_bases<std::exception>;
};

template <>
inline constexpr auto err_domain<std::errc> = zpp::make_error_domain(
    "std::errc", std::errc{0}, [](auto code) constexpr->std::string_view {
        switch (code) {
        case std::errc::address_family_not_supported:
            return "Address family not supported by protocol";
        case std::errc::address_in_use:
            return "Address already in use";
        case std::errc::address_not_available:
            return "Cannot assign requested address";
        case std::errc::already_connected:
            return "Transport endpoint is already connected";
        case std::errc::argument_list_too_long:
            return "Argument list too long";
        case std::errc::argument_out_of_domain:
            return "Numerical argument out of domain";
        case std::errc::bad_address:
            return "Bad address";
        case std::errc::bad_file_descriptor:
            return "Bad file descriptor";
        case std::errc::bad_message:
            return "Bad message";
        case std::errc::broken_pipe:
            return "Broken pipe";
        case std::errc::connection_aborted:
            return "Software caused connection abort";
        case std::errc::connection_already_in_progress:
            return "Operation already in progress";
        case std::errc::connection_refused:
            return "Connection refused";
        case std::errc::connection_reset:
            return "Connection reset by peer";
        case std::errc::cross_device_link:
            return "Invalid cross-device link";
        case std::errc::destination_address_required:
            return "Destination address required";
        case std::errc::device_or_resource_busy:
            return "Device or resource busy";
        case std::errc::directory_not_empty:
            return "Directory not empty";
        case std::errc::executable_format_error:
            return "Exec format error";
        case std::errc::file_exists:
            return "File exists";
        case std::errc::file_too_large:
            return "File too large";
        case std::errc::filename_too_long:
            return "File name too long";
        case std::errc::function_not_supported:
            return "Function not implemented";
        case std::errc::host_unreachable:
            return "No route to host";
        case std::errc::identifier_removed:
            return "Identifier removed";
        case std::errc::illegal_byte_sequence:
            return "Invalid or incomplete multibyte or wide character";
        case std::errc::inappropriate_io_control_operation:
            return "Inappropriate ioctl for device";
        case std::errc::interrupted:
            return "Interrupted system call";
        case std::errc::invalid_argument:
            return "Invalid argument";
        case std::errc::invalid_seek:
            return "Illegal seek";
        case std::errc::io_error:
            return "Input/output error";
        case std::errc::is_a_directory:
            return "Is a directory";
        case std::errc::message_size:
            return "Message too long";
        case std::errc::network_down:
            return "Network is down";
        case std::errc::network_reset:
            return "Network dropped connection on reset";
        case std::errc::network_unreachable:
            return "Network is unreachable";
        case std::errc::no_buffer_space:
            return "No buffer space available";
        case std::errc::no_child_process:
            return "No child processes";
        case std::errc::no_link:
            return "Link has been severed";
        case std::errc::no_lock_available:
            return "No locks available";
        case std::errc::no_message:
            return "No message of desired type";
        case std::errc::no_protocol_option:
            return "Protocol not available";
        case std::errc::no_space_on_device:
            return "No space left on device";
        case std::errc::no_stream_resources:
            return "Out of streams resources";
        case std::errc::no_such_device_or_address:
            return "No such device or address";
        case std::errc::no_such_device:
            return "No such device";
        case std::errc::no_such_file_or_directory:
            return "No such file or directory";
        case std::errc::no_such_process:
            return "No such process";
        case std::errc::not_a_directory:
            return "Not a directory";
        case std::errc::not_a_socket:
            return "Socket operation on non-socket";
        case std::errc::not_a_stream:
            return "Device not a stream";
        case std::errc::not_connected:
            return "Transport endpoint is not connected";
        case std::errc::not_enough_memory:
            return "Cannot allocate memory";
#if ENOTSUP != EOPNOTSUPP
        case std::errc::not_supported:
            return "Operation not supported";
#endif
        case std::errc::operation_canceled:
            return "Operation canceled";
        case std::errc::operation_in_progress:
            return "Operation now in progress";
        case std::errc::operation_not_permitted:
            return "Operation not permitted";
        case std::errc::operation_not_supported:
            return "Operation not supported";
#if EAGAIN != EWOULDBLOCK
        case std::errc::operation_would_block:
            return "Resource temporarily unavailable";
#endif
        case std::errc::owner_dead:
            return "Owner died";
        case std::errc::permission_denied:
            return "Permission denied";
        case std::errc::protocol_error:
            return "Protocol error";
        case std::errc::protocol_not_supported:
            return "Protocol not supported";
        case std::errc::read_only_file_system:
            return "Read-only file system";
        case std::errc::resource_deadlock_would_occur:
            return "Resource deadlock avoided";
        case std::errc::resource_unavailable_try_again:
            return "Resource temporarily unavailable";
        case std::errc::result_out_of_range:
            return "Numerical result out of range";
        case std::errc::state_not_recoverable:
            return "State not recoverable";
        case std::errc::stream_timeout:
            return "Timer expired";
        case std::errc::text_file_busy:
            return "Text file busy";
        case std::errc::timed_out:
            return "Connection timed out";
        case std::errc::too_many_files_open_in_system:
            return "Too many open files in system";
        case std::errc::too_many_files_open:
            return "Too many open files";
        case std::errc::too_many_links:
            return "Too many links";
        case std::errc::too_many_symbolic_link_levels:
            return "Too many levels of symbolic links";
        case std::errc::value_too_large:
            return "Value too large for defined data type";
        case std::errc::wrong_protocol_type:
            return "Protocol wrong type for socket";
        default:
            return "Unspecified error";
        }
    });

} // namespace zpp

#endif // ZPP_THROWING_H
