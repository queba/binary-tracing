#ifndef DYAD_H
#define DYAD_H

#include <consthash/cityhash64.hxx>
#include <boost/format.hpp>
#include <boost/preprocessor/stringize.hpp>

#include <iostream>
#include <type_traits>
#include <cstdint>
#include <cstring>

template <typename T>
class TraceDecoratedValue
{
public:
    static const bool kHasDecode = true;

    constexpr TraceDecoratedValue()
        : _ref(nullptr)
    {}

    constexpr explicit TraceDecoratedValue(const T *v)
        : _ref(v)
    {
    }

    void encode(char *buf) const
    {
        if (_ref) std::memcpy(buf, _ref, sizeof(T));
    }

    static const T &decode(const char *buf, size_t &sz)
    {
        return *reinterpret_cast<const T *>(buf);
    }

    constexpr size_t size() const
    {
        return sizeof(T);
    }

private:
    const T *_ref;
};

template <typename T, typename... TRest>
constexpr size_t decorated_total_size(const T &x, const TRest &... val)
{
    return x.size() + decorated_total_size(val...);
}

template <typename T>
constexpr size_t decorated_total_size(const T &t)
{
    return t.size();
}

template <typename T>
constexpr TraceDecoratedValue<T> trace_decorate(const T &val)
{
    return TraceDecoratedValue<T>(&val);
}

template <size_t N>
TraceDecoratedValue<char [N]> trace_decorate(const char (&)[N])
{
    return TraceDecoratedValue<char [N]>();
}

template <uint64_t TraceTag>
class TraceControl
{
// Functions for online tracing
public:
    // offset is used for assertion
    template <typename T, typename... TRest>
    static void trace_encode(char *buf, size_t len, size_t offset,
            const TraceDecoratedValue<T> &v,
            const TraceDecoratedValue<TRest> &... x)
    {
        v.encode(buf + offset);
        trace_encode(buf, len, offset + v.size(), x...);
    }

    static void trace_encode(char *buf, size_t len, size_t offset)
    {
        assert(len == offset);
    }

    template <typename... T>
    static void trace_impl(const TraceDecoratedValue<T> &...x)
    {
        constexpr size_t total_size = decorated_total_size(x...);
        char buf[total_size];
        trace_encode(buf, total_size, 0, x...);
    }

    template <typename... T>
    static void trace(const char *fmt, const T &... x)
    {
        // add a new entry in the dyad section
        trace_impl(trace_decorate(&print<T...>), trace_decorate(x)...);
    }

// Functions for offline decoding
public:
    template <typename... T>
    static size_t print(std::ostream &os, const char *buf)
    {
        boost::format fmt = get_trace_format();
        size_t sz = print_impl<T...>(fmt, buf);
        os << fmt.str() << std::endl;
        return sz;
    }
private:
    static boost::format get_trace_format()
    {
        // Search through the dyad section to find the format string for a tag
        return boost::format();
    }

    template <typename T>
    static size_t decode_single(boost::format &fmt, const char *buf,
            const std::true_type &decode_yes)
    {
        size_t sz;
        const T &val = TraceDecoratedValue<T>::decode(buf, sz);
        fmt = fmt % val;
        return sz;
    }

    template <typename T>
    static size_t decode_single(boost::format &fmt, const char *buf,
            const std::false_type &decode_no)
    {
        return 0;
    }

    template <typename T>
    static size_t print_impl(boost::format &fmt, const char *buf)
    {
        typedef TraceDecoratedValue<T> DecoratedType;
        return decode_single<T>(fmt, buf,
                std::integral_constant<bool, DecoratedType::kHasDecode>());
    }

    template <typename T, typename TNext, typename... TRest>
    static size_t print_impl(boost::format &fmt, const char *buf)
    {
        size_t sz = print_impl<T>(fmt, buf);
        return sz + print_impl<TNext, TRest...>(fmt, buf + sz);
    }

};

#define TRACE(fmt, ...) \
    const static char format ## __LINE__ [] __attribute__((section("dyad"))) = \
        fmt; \
    TraceControl<consthash::city64(__FILE__ ":" BOOST_PP_STRINGIZE(__LINE__))>::trace(fmt, ##__VA_ARGS__);

#endif
