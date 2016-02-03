// The MIT License (MIT)

// Copyright (c) 2016 Rapptz

// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef JSON_CANONICAL_HPP
#define JSON_CANONICAL_HPP

#include "jsonpp/error.hpp"
#include "jsonpp/value.hpp"
#include <sstream>

namespace json {

template<typename Type>
struct canonical_recipe {};

namespace detail {

struct to_json_algo;

struct canonical_to_json_type {
private:
    template<typename Type>
    using is_json = Or<is_null<Type>, is_bool<Type>, is_number<Type>, is_string<Type>, is_array<Type>, is_object<Type>>;

    template<typename Source, EnableIf<is_json<Source>> = 0>
    static value impl(Source const& source, int)
    { return source; }

    template<typename Source, DisableIf<is_json<Source>> = 0>
    static value impl(Source const& source, long)
    {
        DependOn<to_json_algo, Source> algo {};
        canonical_recipe<Source> {}(algo, source);
        return std::move(algo).result;
    }

public:
    template<typename Source>
    value operator()(Source const& source) const
    { return impl(source, 0); }
};

} // detail

constexpr detail::canonical_to_json_type canonical_to_json {};

namespace detail {

struct to_json_algo {
    object result;

    template<typename Source>
    void member(const char* name, Source const& source)
    {
        result.insert({ name, canonical_to_json(source) });
    }
};

struct from_json_algo;

template<typename T, typename Sfinae = int>
struct type_name;

template<typename T> struct type_name<T, EnableIf<is_null<T>>> {
    static constexpr const char* value = "null";
};
template<typename T> constexpr const char* type_name<T, EnableIf<is_null<T>>>::value;

template<typename T> struct type_name<T, EnableIf<is_bool<T>>> {
    static constexpr const char* value = "boolean";
};
template<typename T> constexpr const char* type_name<T, EnableIf<is_bool<T>>>::value;

template<typename T> struct type_name<T, EnableIf<is_number<T>>> {
    static constexpr const char* value = "number";
};
template<typename T> constexpr const char* type_name<T, EnableIf<is_number<T>>>::value;

template<typename T> struct type_name<T, EnableIf<is_string<T>>> {
    static constexpr const char* value = "string";
};
template<typename T> constexpr const char* type_name<T, EnableIf<is_string<T>>>::value;

template<typename T> struct type_name<T, EnableIf<is_object<T>>> {
    static constexpr const char* value = "object";
};
template<typename T> constexpr const char* type_name<T, EnableIf<is_object<T>>>::value;

template<typename T> struct type_name<T, EnableIf<is_array<T>>> {
    static constexpr const char* value = "array";
};
template<typename T> constexpr const char* type_name<T, EnableIf<is_array<T>>>::value;

template<typename Dest>
struct canonical_from_json_type {
private:
    template<typename Dep>
    using is_json = Or<
        is_null<DependOn<Dest, Dep>>,
        is_bool<DependOn<Dest, Dep>>,
        is_number<DependOn<Dest, Dep>>,
        is_string<DependOn<Dest, Dep>>,
        is_array<DependOn<Dest, Dep>>,
        is_object<DependOn<Dest, Dep>>
    >;

    template<typename Dep = void, EnableIf<is_json<Dep>> = 0>
    static void impl(value const& v, Dest& result, int)
    {
        if(!v.is<Dest>()) {
            std::ostringstream fmt;
            fmt << "expected a(n) " << type_name<Dest>::value << ", received a(n) " << v.type_name() << " instead";
            throw canonical_from_json_error { std::move(fmt).str() };
        }
        result = v.as<Dest>();
    }

    template<typename Dep = void, DisableIf<is_json<Dep>> = 0>
    static void impl(value const& v, Dest& result, long)
    {
        if(!v.is<object>()) {
            std::ostringstream fmt;
            fmt << "expected an object, received a(n) " << v.type_name() << " instead";
            throw canonical_from_json_error { std::move(fmt).str() };
        }

        auto&& obj = v.as<object>();
        DependOn<from_json_algo, Dep> algo { obj };
        canonical_recipe<Dest> {}(algo, result);
    }

public:
    Dest operator()(value const& v) const
    {
        Dest result;
        impl(v, result, 0);
        return result;
    }

    void operator()(value const& v, Dest& result) const
    {
        impl(v, result, 0);
    }
};

} // detail

template<typename Dest>
Dest canonical_from_json(value const& v)
{
    static constexpr detail::canonical_from_json_type<Dest> call;
    return call(v);
}

template<typename Dest>
void canonical_from_json(value const& v, Dest& result)
{
    static constexpr detail::canonical_from_json_type<Dest> call;
    call(v, result);
}

namespace detail {

struct from_json_algo {
    object obj;

    template<typename Value>
    void member(const char* name, Value& value) const
    {
        auto it = obj.find(name);
        if(it == obj.end()) {
            std::ostringstream fmt;
            fmt << "missing member '" << name << '\'';
            throw canonical_from_json_error { std::move(fmt).str() };
        }

        try {
            canonical_from_json<Value>(it->second, value);
        } catch(canonical_from_json_error& e) {
            std::ostringstream fmt;
            fmt << "bad member '" << name << "': " << std::move(e).message;
            e.message = std::move(fmt).str();
            throw;
        }
    }
};

} // detail

} // json

#endif // JSON_CANONICAL_HPP