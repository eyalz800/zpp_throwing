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
    using integral_type = int;

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
    virtual std::string_view
    message(integral_type code) const noexcept = 0;

    /**
     * Returns true if success code, else false.
     */
    bool success(integral_type code) const
    {
        return code == m_success_code;
    }

protected:
    /**
     * Creates an error domain whose success code is 'success_code'.
     */
    constexpr error_domain(integral_type success_code) :
        m_success_code(success_code)
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
    integral_type m_success_code{};
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
    using integral_type = error_domain::integral_type;

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
     * Constructs an error from an error code enumeration/integral type,
     * the domain is given explicitly in this overload.
     */
    template <typename ErrorCode>
    error(ErrorCode error_code, const error_domain & domain) :
        m_domain(std::addressof(domain)),
        m_code(static_cast<integral_type>(error_code))
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
    explicit operator bool() const noexcept
    {
        return m_domain->success(m_code);
    }

    /**
     * Returns true if the error indicates success, else false.
     */
    bool success() const noexcept
    {
        return m_domain->success(m_code);
    }

    /**
     * Returns true if the error indicates failure, else false.
     */
    bool failure() const noexcept
    {
        return !m_domain->success(m_code);
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
    integral_type m_code{};
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
 * Determine the throwing state via error domain placeholders.
 * @{
 */
enum class rethrow_error
{
};

template <>
inline constexpr auto err_domain<rethrow_error> = zpp::make_error_domain(
    {}, rethrow_error{}, [](auto) constexpr->std::string_view {
        return {};
    });

enum class throwing_exception
{
};

template <>
inline constexpr auto err_domain<throwing_exception> =
    zpp::make_error_domain(
        {}, throwing_exception{}, [](auto) constexpr->std::string_view {
            return {};
        });
/**
 * @}
 */

/**
 * Yield `rethrow` for rethrowing exceptions.
 */
struct rethrow_t
{
};
constexpr rethrow_t rethrow;

/**
 * Return void for void returning coroutines.
 */
struct void_t
{
};
constexpr void_t void_v;

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

template <typename Allocator>
struct exceptioin_object_delete
{
    void operator()(exception_object * pointer)
    {
        if constexpr (std::is_void_v<Allocator>) {
            delete pointer;
        } else {
            Allocator allocator;
            std::allocator_traits<Allocator>::destroy(allocator, pointer);
            std::allocator_traits<Allocator>::deallocate(
                allocator,
                reinterpret_cast<std::byte *>(pointer),
                sizeof(*pointer));
        }
    }
};

template <typename Allocator>
using exception_ptr =
    std::unique_ptr<exception_object, exceptioin_object_delete<Allocator>>;

template <typename Type, typename Allocator>
auto make_exception_ptr(auto &&... arguments)
{
    if constexpr (std::is_void_v<Allocator>) {
        return exception_ptr<Allocator>(
            new Type(std::forward<decltype(arguments)>(arguments)...));
    } else {
        Allocator allocator;
        auto allocated = std::allocator_traits<Allocator>::allocate(
            allocator, sizeof(Type));
        if (!allocated) {
            return exception_ptr<Allocator>(nullptr);
        }

        std::allocator_traits<Allocator>::construct(
            allocator,
            reinterpret_cast<Type *>(allocated),
            std::forward<decltype(arguments)>(arguments)...);

        return exception_ptr<Allocator>(
            reinterpret_cast<Type *>(allocated));
    }
}

template <typename Type, typename Allocator>
auto make_exception_object(auto &&... arguments)
{
    if constexpr (std::is_void_v<Allocator>) {
        return static_cast<exception_object *>(
            new Type(std::forward<decltype(arguments)>(arguments)...));
    } else {
        Allocator allocator;
        auto allocated = std::allocator_traits<Allocator>::allocate(
            allocator, sizeof(Type));
        if (!allocated) {
            return exception_ptr<Allocator>(nullptr);
        }

        std::allocator_traits<Allocator>::construct(
            allocator,
            reinterpret_cast<Type *>(allocated),
            std::forward<decltype(arguments)>(arguments)...);

        return static_cast<exception_object *>(
            reinterpret_cast<Type *>(allocated));
    }
}

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

    constexpr type_info_entry(void * (*function)(void *) noexcept) :
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
    return nullptr;
}

template <typename Type>
struct catch_type
    : catch_type<decltype(&std::remove_cv_t<
                          std::remove_reference_t<Type>>::operator())>
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
 * The exit condition of the coroutine - A value, or error/exception.
 */
template <typename Type, typename ExceptionType, typename Allocator>
struct exit_condition
{
    using exception_type = ExceptionType;
    using error_type = class error;

    exit_condition() :
        m_error_domain(std::addressof(err_domain<rethrow_error>))
    {
    }

    template <typename..., typename Dependent = Type>
    explicit exit_condition(void_t) requires std::is_void_v<Dependent>
        : m_error_domain(nullptr)
    {
    }

    explicit exit_condition(auto && value) :
        m_error_domain(nullptr),
        m_return_value(std::forward<decltype(value)>(value))
    {
    }

    template <typename OtherExceptionType>
    exit_condition(
        exit_condition<Type, OtherExceptionType, Allocator> &&
            other) noexcept(std::is_void_v<Type> ||
                            std::is_nothrow_move_constructible_v<Type>)
    {
        if (other.is_value()) {
            if constexpr (!std::is_void_v<Type>) {
                if constexpr (!std::is_reference_v<Type>) {
                    ::new (std::addressof(m_return_value))
                        Type(std::move(other.m_return_value));
                } else {
                    m_return_value = other.m_return_value;
                }
            }
        } else {
            if (other.is_exception()) {
                ::new (std::addressof(m_exception))
                    exception_type(std::move(other.m_exception));
            } else {
                m_error_code = other.m_error_code;
            }
            m_error_domain = other.m_error_domain;
        }
    }

    ~exit_condition()
    {
        if constexpr (!std::is_void_v<Type> &&
                      !std::is_trivially_destructible_v<Type>) {
            if constexpr (!std::is_trivially_destructible_v<
                              exception_type>) {
                if (is_value()) {
                    m_return_value.~Type();
                } else if (is_exception()) {
                    m_exception.~exception_type();
                }
            } else {
                if (is_value()) {
                    m_return_value.~Type();
                }
            }
        } else if constexpr (!std::is_trivially_destructible_v<
                                 exception_type>) {
            if (is_exception()) {
                m_exception.~exception_type();
            }
        }
    }

    bool is_exception() const noexcept
    {
        return m_error_domain ==
               std::addressof(err_domain<throwing_exception>);
    }

    bool is_value() const noexcept
    {
        return !m_error_domain;
    }

    bool success() const noexcept
    {
        return !m_error_domain;
    }

    bool failure() const noexcept
    {
        return m_error_domain;
    }

    bool is_error() const noexcept
    {
        return !is_value() && !is_exception();
    }

    auto is_rethrow() const noexcept
    {
        return m_error_domain == std::addressof(err_domain<rethrow_error>);
    }

    explicit operator bool() const noexcept
    {
        return success();
    }

    decltype(auto) value() && noexcept
    {
        if constexpr (std::is_same_v<Type, decltype(m_return_value)>) {
            return std::forward<Type>(m_return_value);
        } else {
            return std::forward<Type>(*m_return_value);
        }
    }

    decltype(auto) value() & noexcept
    {
        if constexpr (std::is_same_v<Type, decltype(m_return_value)>) {
            return (m_return_value);
        } else {
            return (*m_return_value);
        }
    }

    auto & exception() noexcept
    {
        return *m_exception;
    }

    auto error() const noexcept
    {
        return error_type{m_error_code, *m_error_domain};
    }

    /**
     * Exits with a value.
     * Must not call exit functions exactly once.
     */
    template <typename..., typename Dependent = Type>
    void
    exit_with_value(auto && other) requires(!std::is_void_v<Dependent>)
    {
        if constexpr (!std::is_void_v<Type>) {
            if constexpr (!std::is_reference_v<Type>) {
                ::new (std::addressof(m_return_value))
                    Type(std::forward<decltype(other)>(other));
            } else {
                m_return_value = std::addressof(other);
            }
        }
        m_error_domain = nullptr;
    }

    template <typename..., typename Dependent = Type>
    void exit_with_value() requires std::is_void_v<Dependent>
    {
        m_error_domain = nullptr;
    }

    /**
     * Exits with exception.
     * Must not call exit functions exactly once.
     */
    template <typename Exception>
    auto exit_with_exception(Exception && exception) noexcept
    {
        using type = std::remove_cv_t<std::remove_reference_t<Exception>>;

        // Define the exception object that will be type erased.
        struct exception_holder : public exception_object
        {
            exception_holder(Exception && exception) :
                m_exception(std::forward<Exception>(exception))
            {
            }

            auto dynamic_object() noexcept
                -> struct dynamic_object override
            {
                // Return the type id of the exception object and its
                // address.
                return {detail::type_id<type>(),
                        std::addressof(m_exception)};
            }

            ~exception_holder() override = default;

            type m_exception;
        };

        ::new (std::addressof(m_exception)) exception_type(
            make_exception_object<exception_holder, Allocator>(
                std::forward<Exception>(exception)));

        m_error_domain = std::addressof(err_domain<throwing_exception>);

        if constexpr (std::is_void_v<Allocator>) {
            // Nothing to be done.

        } else if constexpr (noexcept(std::declval<Allocator>().allocate(
                                 std::size_t{}))) {
            if (!m_exception) {
                exit_with_error(std::errc::not_enough_memory);
            }
        }
    }

    /**
     * This must not hold a value or an exception.
     * Must not call exit functions exactly once.
     */
    void exit_rethrow() noexcept
    {
        m_error_domain = std::addressof(err_domain<rethrow_error>);
    }

    /**
     * Exits with an error value.
     * Must not call exit functions exactly once.
     */
    void exit_with_error(const error_type & error) noexcept
    {
        m_error_domain = std::addressof(error.domain());
        m_error_code = error.code();
    }

    /**
     * Propagates an exception/error.
     * Must not call exit functions exactly once, `other` must have an
     * error or an exception.
     */
    template <typename OtherType, typename OtherExceptionType>
    void exit_propagate(
        exit_condition<OtherType, OtherExceptionType, Allocator> &
            other) noexcept
    {
        if (other.is_exception()) {
            if constexpr (!std::is_same_v<ExceptionType,
                                          OtherExceptionType> &&
                          requires { other.m_exception.release(); }) {
                ::new (std::addressof(m_exception))
                    exception_type(other.m_exception.release());
            } else {
                ::new (std::addressof(m_exception))
                    exception_type(std::move(other.m_exception));
            }
        } else {
            // Since we don't have a value, or exception, error code is the
            // active member of the union.
            m_error_code = other.m_error_code;
        }
        m_error_domain = other.m_error_domain;
    }

    const error_domain * m_error_domain{};
    union
    {
        int m_error_code{};
        exception_type m_exception;
        std::conditional_t<
            std::is_void_v<Type>,
            std::nullptr_t,
            std::conditional_t<
                std::is_reference_v<Type>,
                std::add_pointer_t<std::remove_reference_t<Type>>,
                Type>>
            m_return_value;
    };
};

/**
 * Use as the return type of the function, throw exceptions
 * by using `co_yield` / `co_return`. Using `co_yield` is clearer
 * but in some cases may generate less code than `co_return`.
 * If you want to throw exceptions using `co_return` in void returning
 * functions, use `zpp::throwing<zpp::void_t>` instead, and `co_return
 * zpp::void_v` to exit normally). Call throwing functions by `co_await`,
 * and return values using `co_return`. Use the `zpp::try_catch` function
 * to execute a function object and then to catch exceptions thrown from
 * it.
 */
template <typename Type, typename Allocator = void>
class [[nodiscard]] throwing
{
public:
    template <typename, typename>
    friend class throwing;

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
        template <typename, typename>
        friend class throwing;

        struct suspend_destroy
        {
            constexpr bool await_ready() noexcept { return false; }
            void await_suspend(auto handle) noexcept { handle.destroy(); }
            [[noreturn]] void await_resume() noexcept { while (true); }
        };

        auto get_return_object()
        {
            return throwing{static_cast<promise_type &>(*this)};
        }

        auto initial_suspend() noexcept
        {
            return suspend_never{};
        }

        auto final_suspend() noexcept
        {
            return suspend_never{};
        }

        void unhandled_exception()
        {
            std::terminate();
        }

        /**
         * Throw and destroy calling coroutine.
         */
        template <typename Value>
        auto yield_value(Value && value)
        {
            throw_it(std::forward<Value>(value));
            return suspend_destroy{};
        }

        /**
         * Throw an exception.
         */
        template <typename Value>
        void throw_it(Value && value) requires requires
        {
            define_exception<
                std::remove_cv_t<std::remove_reference_t<Value>>>();
        }
        {
            m_return_object->m_condition.exit_with_exception(
                std::forward<Value>(value));
        }

        /**
         * Rethrow from exising.
         */
        template <typename ExitCondition>
        void throw_it(
            std::tuple<const rethrow_t &, ExitCondition &> error_condition)
        {
            m_return_object->m_condition.exit_propagate(
                std::get<1>(error_condition));
        }

        /**
         * Rethrow the current set exception.
         */
        void throw_it(rethrow_t)
        {
            m_return_object->m_condition.exit_rethrow();
        }

        /**
         * Throw an error.
         */
        void throw_it(const error & error)
        {
            m_return_object->m_condition.exit_with_error(error);
        }

    protected:
        ~basic_promise_type() = default;

        throwing * m_return_object{};
    };

    template <typename Base>
    struct throwing_allocator : public Base
    {
        void * operator new(std::size_t size)
        {
            Allocator allocator;
            return std::allocator_traits<Allocator>::allocate(allocator,
                                                              size);
        }

        void operator delete(void * pointer, std::size_t size) noexcept
        {
            Allocator allocator;
            std::allocator_traits<Allocator>::deallocate(
                allocator, static_cast<std::byte *>(pointer), size);
        }

    protected:
        ~throwing_allocator() = default;
    };

    template <typename Base>
    struct noexcept_allocator : public Base
    {
        void * operator new(std::size_t size) noexcept
        {
            Allocator allocator;
            return std::allocator_traits<Allocator>::allocate(allocator,
                                                              size);
        }

        void operator delete(void * pointer, std::size_t size) noexcept
        {
            Allocator allocator;
            std::allocator_traits<Allocator>::deallocate(
                allocator, static_cast<std::byte *>(pointer), size);
        }

        static auto get_return_object_on_allocation_failure()
        {
            return throwing(nullptr);
        }

    protected:
        ~noexcept_allocator() = default;
    };

    /**
     * Add the return void functionality to base.
     */
    template <typename Base>
    struct promise_type_void : public Base
    {
        using Base::Base;

        void return_void()
        {
            Base::m_return_object->m_condition.exit_with_value();
        }
    };

    /**
     * Add the return value functionality to base.
     */
    template <typename Base>
    struct promise_type_nonvoid : public Base
    {
        using Base::Base;

        template <typename T>
        void return_value(T && value)
        {
            if constexpr (requires {
                              Base::throw_it(std::forward<T>(value));
                          }) {
                Base::throw_it(std::forward<T>(value));
            } else {
                Base::m_return_object->m_condition.exit_with_value(
                    std::forward<T>(value));
            }
        }
    };

    static constexpr bool is_noexcept_allocator =
        noexcept(std::declval<std::conditional_t<std::is_void_v<Allocator>,
                                                 std::allocator<std::byte>,
                                                 Allocator>>()
                     .allocate(std::size_t{}));

    /**
     * The actual promise type, which adds the appropriate
     * return strategy to the basic promise type.
     */
    using promise_type = std::conditional_t<
        std::is_void_v<Type>,
        std::conditional_t<
            std::is_void_v<Allocator>,
            promise_type_void<basic_promise_type>,
            std::conditional_t<
                is_noexcept_allocator,
                promise_type_void<noexcept_allocator<basic_promise_type>>,
                promise_type_void<
                    throwing_allocator<basic_promise_type>>>>,
        std::conditional_t<
            std::is_void_v<Allocator>,
            promise_type_nonvoid<basic_promise_type>,
            std::conditional_t<is_noexcept_allocator,
                               promise_type_nonvoid<
                                   noexcept_allocator<basic_promise_type>>,
                               promise_type_nonvoid<throwing_allocator<
                                   basic_promise_type>>>>>;

    /**
     * Constructor for out of memory scenario.
     */
    explicit throwing(std::nullptr_t) noexcept
    {
        m_condition.exit_with_error(std::errc::not_enough_memory);
    }

    /**
     * Construct from the coroutine handle.
     */
    explicit throwing(promise_type & promise) noexcept
    {
        promise.m_return_object = this;
    }

    /**
     * Construct directly from a value.
     */
    throwing(auto && value) requires
        std::is_convertible_v<decltype(value), Type> ||
        (std::is_void_v<Type> && std::is_same_v<
            std::remove_cv_t<std::remove_reference_t<decltype(value)>>,
            void_t>) :
        m_condition(std::forward<decltype(value)>(value))
    {
    }

    /**
     * Construct directly from an error/exception.
     */
    throwing(auto && value) requires requires(promise_type & promise)
    {
        promise.throw_it(std::forward<decltype(value)>(value));
    }
    {
        if constexpr (requires {
                          define_exception<std::remove_cv_t<
                              std::remove_reference_t<decltype(value)>>>();
                      }) {
            m_condition.exit_with_exception(
                std::forward<decltype(value)>(value));
        } else {
            m_condition.exit_with_error(
                std::forward<decltype(value)>(value));
        }
    }

    /**
     * Rethrow an exception.
     */
    constexpr throwing(rethrow_t) noexcept
    {
    }

    /**
     * Await is ready if there is no exception.
     */
    bool await_ready()
    {
        return m_condition.success();
    }

    /**
     * Suspend execution only if there is an exception to be thrown.
     */
    template <typename PromiseType>
    void await_suspend(coroutine_handle<PromiseType> outer_handle) noexcept
    {
        outer_handle.promise().m_return_object->m_condition.exit_propagate(
            m_condition);
        outer_handle.destroy();
    }

    /**
     * Return the stored value on resume.
     */
    decltype(auto) await_resume() noexcept
    {
        if constexpr (std::is_void_v<Type>) {
            return;
        } else {
            return std::move(m_condition).value();
        }
    }

    /**
     * Returns true if value is stored, otherwise, an
     * exception/error is stored.
     */
    explicit operator bool() const noexcept
    {
        return m_condition.success();
    }

    /**
     * Returns true if value is stored, otherwise, an
     * exception/error is stored.
     */
    bool success() const noexcept
    {
        return m_condition.success();
    }

    /**
     * Returns true if exception/error is stored, otherwise,
     * value is stored.
     */
    bool failure() const noexcept
    {
        return m_condition.failure();
    }

    /**
     * Returns the stored value, the behavior
     * is undefined if there is an exception/error stored.
     */
    decltype(auto) value() && noexcept
    {
        if constexpr (std::is_void_v<Type>) {
            return;
        } else {
            return std::move(m_condition).value();
        }
    }

    /**
     * Returns the stored value, the behavior
     * is undefined if there is an exception/error stored.
     */
    decltype(auto) value() & noexcept
    {
        if constexpr (std::is_void_v<Type>) {
            return;
        } else {
            return m_condition.value();
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
    >
    requires IsThrowing ||
        (... ||
         (
             requires {
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
            typename std::invoke_result_t<
                decltype(std::get<sizeof...(Clauses)>(
                    std::declval<std::tuple<Clause, Clauses...>>()))>;
        })throwing catch_exception_object(const dynamic_object & exception,
                                          Clause && clause,
                                          Clauses &&... clauses)
    {
        if constexpr (std::is_void_v<CatchType>) {
            static_assert(0 == sizeof...(Clauses),
                          "Catch all clause must be the last one.");
            if constexpr (IsThrowing) {
                auto result = std::forward<Clause>(clause)();
                if (!result.is_rethrow()) [[likely]] {
                    if (exception.address) {
                        exception_ptr<Allocator>(
                            std::addressof(m_condition.exception()));
                    }
                    return result;
                } else [[unlikely]] {
                    return std::move(*this);
                }
            } else {
                if (exception.address) {
                    exception_ptr<Allocator>(
                        std::addressof(m_condition.exception()));
                }
                if constexpr (std::is_void_v<decltype(std::forward<Clause>(
                                  clause)())>) {
                    std::forward<Clause>(clause)();
                    return void_v;
                } else {
                    return std::forward<Clause>(clause)();
                }
            }
        } else if constexpr (requires {
                                 std::forward<Clause>(clause)(
                                     m_condition.error());
                             }) {
            if (exception.address) {
                if constexpr (0 != sizeof...(Clauses)) {
                    return catch_exception_object(
                        exception, std::forward<Clauses>(clauses)...);
                } else {
                    return std::move(*this);
                }
            }

            if constexpr (IsThrowing) {
                auto result =
                    std::forward<Clause>(clause)(m_condition.error());
                if (!result.is_rethrow()) [[likely]] {
                    return result;
                } else [[unlikely]] {
                    return std::move(*this);
                }
            } else {
                if constexpr (std::is_void_v<decltype(std::forward<Clause>(
                                  clause)(m_condition.error())

                                                          )>) {
                    std::forward<Clause>(clause)(m_condition.error());
                    return void_v;
                } else {
                    return std::forward<Clause>(clause)(
                        m_condition.error());
                }
            }
        } else if constexpr (requires { error{CatchType{}}; }) {
            if (exception.address ||
                std::addressof(m_condition.error().domain()) !=
                    std::addressof(err_domain<CatchType>)) {
                if constexpr (0 != sizeof...(Clauses)) {
                    return catch_exception_object(
                        exception, std::forward<Clauses>(clauses)...);
                } else {
                    return std::move(*this);
                }
            }

            if constexpr (IsThrowing) {
                auto result = std::forward<Clause>(clause)(
                    CatchType{m_condition.error().code()});
                if (!result.is_rethrow()) [[likely]] {
                    return result;
                } else [[unlikely]] {
                    return std::move(*this);
                }
            } else {
                if constexpr (std::is_void_v<decltype(std::forward<Clause>(
                                  clause)(CatchType{
                                  m_condition.error().code()}))>) {
                    std::forward<Clause>(clause)(
                        CatchType{m_condition.error().code()});
                    return void_v;
                } else {
                    return std::forward<Clause>(clause)(
                        CatchType{m_condition.error().code()});
                }
            }
        } else if constexpr (requires { define_exception<CatchType>(); }) {
            CatchType * catch_object = nullptr;
            if (exception.address) {
                catch_object = static_cast<CatchType *>(
                    detail::dyn_cast(detail::type_id<CatchType>(),
                                     exception.address,
                                     exception.type_id));
            }

            if (!catch_object) {
                if constexpr (0 != sizeof...(Clauses)) {
                    return catch_exception_object(
                        exception, std::forward<Clauses>(clauses)...);
                } else {
                    return std::move(*this);
                }
            }

            if constexpr (IsThrowing) {
                auto result = std::forward<Clause>(clause)(*catch_object);
                if (!result.is_rethrow()) [[likely]] {
                    exception_ptr<Allocator>(
                        std::addressof(m_condition.exception()));
                    return result;
                } else [[unlikely]] {
                    return std::move(*this);
                }
            } else {
                exception_ptr<Allocator> exception_disposer(
                    std::addressof(m_condition.exception()));
                if constexpr (std::is_void_v<decltype(std::forward<Clause>(
                                  clause)(*catch_object))>) {
                    std::forward<Clause>(clause)(*catch_object);
                    return void_v;
                } else {
                    return std::forward<Clause>(clause)(*catch_object);
                }
            }
        } else {
            static_assert(std::is_void_v<CatchType>,
                          "Invalid catch clause.");
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
    >
    requires(!(
        IsThrowing ||
        (... ||
         (
             requires {
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
            typename std::invoke_result_t<
                decltype(std::get<sizeof...(Clauses)>(
                    std::declval<std::tuple<Clause, Clauses...>>()))>;
        }))) Type catch_exception_object(const dynamic_object & exception,
                                         Clause && clause,
                                         Clauses &&... clauses)
    {
        if constexpr (std::is_void_v<CatchType>) {
            static_assert(!sizeof...(Clauses),
                          "Catch all object with no parameters must "
                          "be the last one.");
            if (exception.address) {
                exception_ptr<Allocator>(
                    std::addressof(m_condition.exception()));
            }
            return std::forward<Clause>(clause)();
        } else if constexpr (requires {
                                 std::forward<Clause>(clause)(
                                     m_condition.error());
                             }) {
            if (exception.address) {
                static_assert(0 != sizeof...(Clauses),
                              "Missing catch all block in non "
                              "throwing catches.");
                return catch_exception_object(
                    exception, std::forward<Clauses>(clauses)...);
            }

            return std::forward<Clause>(clause)(m_condition.error());
        } else if constexpr (requires { error{CatchType{}}; }) {
            if (exception.address ||
                std::addressof(m_condition.error().domain()) !=
                    std::addressof(err_domain<CatchType>)) {
                static_assert(0 != sizeof...(Clauses),
                              "Missing catch all block in non "
                              "throwing catches.");
                return catch_exception_object(
                    exception, std::forward<Clauses>(clauses)...);
            }

            return std::forward<Clause>(clause)(
                CatchType{m_condition.error().code()});
        } else if constexpr (requires { define_exception<CatchType>(); }) {
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

            exception_ptr<Allocator> exception_disposer(
                std::addressof(m_condition.exception()));
            return std::forward<Clause>(clause)(*catch_object);
        } else {
            static_assert(std::is_void_v<CatchType>,
                          "Invalid catch clause.");
        }
    }

    /**
     * Returns true if rethrowing, else false.
     */
    bool is_rethrow() const noexcept
    {
        return m_condition.is_rethrow();
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
    inline throwing catches(Clauses &&... clauses) requires requires
    {
        typename decltype(this->catch_exception_object(
            exception_object::null_dynamic_object,
            std::forward<Clauses>(clauses)...))::zpp_throwing_tag;
    }
    {
        // If there is no exception, skip.
        if (m_condition) [[likely]] {
            return std::move(*this);
        } else [[unlikely]] {
            // Follow to catch the exception.
            return catch_exception_object(
                // Increase chance `catches` gets inlined.
                [&] {
                    return m_condition.is_exception()
                               ? m_condition.exception().dynamic_object()
                               : exception_object::null_dynamic_object;
                }(),
                std::forward<Clauses>(clauses)...);
        }
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
        if (m_condition) [[likely]] {
            if constexpr (std::is_void_v<Type>) {
                return;
            } else {
                return std::move(*this).value();
            }
        } else [[unlikely]] {
            // Follow to catch the exception.
            return catch_exception_object(
                // Increase chance `catches` gets inlined.
                [&] {
                    return m_condition.is_exception()
                               ? m_condition.exception().dynamic_object()
                               : exception_object::null_dynamic_object;
                }(),
                std::forward<Clauses>(clauses)...);
        }
    }

private :
    /**
     * The exit condition of the function.
     */
    exit_condition<Type, exception_object *, Allocator>
        m_condition{};
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
        return std::forward<Clause>(clause)();
    } else {
        static_assert(std::is_void_v<Clause>,
                      "try_catch clause must be a coroutine.");
    }
}

template <typename TryClause, typename... CatchClause>
decltype(auto) try_catch(
    TryClause && try_clause,
    CatchClause &&... catch_clause) requires(sizeof...(CatchClause) != 0)
{
    if constexpr (requires {
                      typename std::invoke_result_t<
                          TryClause>::zpp_throwing_tag;
                  }) {
        return std::forward<TryClause>(try_clause)().catches(
            std::forward<CatchClause>(catch_clause)...);
    } else {
        static_assert(std::is_void_v<TryClause>,
                      "Try clause must return throwing<Type>.");
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
