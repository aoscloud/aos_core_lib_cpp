/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_STRING_HPP_
#define AOS_STRING_HPP_

#include <aos/common/array.hpp>
#include <stdio.h>
#include <string.h>

namespace aos {

/**
 * String instance.
 */
class String : public Array<char> {
public:
    /**
     * Creates string.
     */
    using Array::Array;

    // TODO: automatically make const String from const char*.
    // cppcheck-suppress noExplicitConstructor
    /**
     * Constructs string from C string.
     *
     * @param str C string.
     */
    String(const char* str)
        : Array(const_cast<char*>(str), strlen(str))
    {
    }

    /**
     * Assigns C string.
     *
     * @param str C string.
     * @return String&.
     */
    String& operator=(const char* str)
    {
        auto len = strlen(str);

        if (len > MaxSize()) {
            assert(false);
            len = MaxSize();
        }

        memcpy(Get(), str, len);
        auto err = Resize(len);
        assert(err.IsNone());

        return *this;
    }

    /**
     * Returns C string representation.
     *
     * @return const char* C string.
     */
    const char* CStr()
    {
        if (Get()[Size()] != '\0') {
            Get()[Size()] = '\0';
        }

        return Get();
    }

    /**
     * Returns C string representation.
     *
     * @return const char* C string.
     */
    const char* CStr() const
    {
        if (Get()[Size()] != '\0') {
            return nullptr;
        }

        return Get();
    }

    /**
     * Appends string.
     *
     * @param str string to append with.
     * @return Error.
     */
    Error Append(const String& str) { return Array::Append(str); }

    /**
     * Checks if str equals to C string.
     *
     * @param cStr C string to compare with.
     * @return bool.
     */
    bool operator==(const char* cStr) const
    {
        if (strlen(cStr) != Size()) {
            return false;
        }

        return memcmp(Get(), cStr, Size()) == 0;
    };

    /**
     * Checks if str doesn't equal to C string.
     *
     * @param cStr C string to compare with.
     * @return bool.
     */
    bool operator!=(const char* cStr) const { return !operator==(cStr); };

    /**
     * Checks if str equals to another string.
     *
     * @param str string to compare with.
     * @return bool.
     */
    bool operator==(const String& str) const { return Array::operator==(str); };

    /**
     * Checks if str doesn't equal to another string.
     *
     * @param str string to compare with.
     * @return bool.
     */
    bool operator!=(const String& str) const { return Array::operator!=(str); };

    /**
     * Checks if C string equals to string.
     *
     * @param cStr C string to compare with.
     * @param srt string to compare with.
     * @return bool.
     */
    friend bool operator==(const char* cStr, const String& str) { return str.operator==(cStr); };

    /**
     * Checks if C string doesn't equal to string.
     *
     * @param cStr C string to compare with.
     * @param srt string to compare with.
     * @return bool.
     */
    friend bool operator!=(const char* cStr, const String& str) { return str.operator!=(cStr); };

    /**
     * Appends string operator.
     *
     * @param str string to append with.
     * @return String&.
     */
    String& operator+=(const String& str)
    {
        auto err = Array::Append(str);
        assert(err.IsNone());

        return *this;
    }

    /**
     * Converts int to string.
     *
     * @param value int va;ue.
     * @return Error.
     */
    String& ConvertInt(int value)
    {
        auto ret = snprintf(Get(), MaxSize() + 1, "%d", value);
        assert(ret >= 0 && static_cast<size_t>(ret) <= MaxSize());

        Resize(ret);

        return *this;
    }
};

/**
 * Static string instance.
 *
 * @tparam cMaxSize max static string size.
 */
template <size_t cMaxSize>
class StaticString : public String {
public:
    /**
     * Creates static string.
     */
    StaticString() { String::SetBuffer(mBuffer, 0, cMaxSize); }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates static string from another string.
     *
     * @param str initial value.
     */
    StaticString(const String& str)
    {
        String::SetBuffer(mBuffer, 0, cMaxSize);
        String::operator=(str);
    }

    /**
     * Assigns string to static string.
     *
     * @param str string to assign.
     * @return StaticString&.
     */
    StaticString& operator=(const String& str)
    {
        String::operator=(str);

        return *this;
    }

private:
    StaticBuffer<cMaxSize + 1> mBuffer;
};

/**
 * Dynamic string instance.
 *
 * @tparam cMaxSize max dynamic string size.
 */
template <size_t cMaxSize>
class DynamicString : public String {
public:
    /**
     * Create dynamic string.
     */
    DynamicString()
        : mBuffer(cMaxSize * +1)
    {
        String::SetBuffer(mBuffer, 0, cMaxSize);
    }

    // cppcheck-suppress noExplicitConstructor
    /**
     * Creates dynamic string from another string.
     *
     * @param str initial value.
     */
    DynamicString(const String& str)
    {
        String::SetBuffer(mBuffer, 0, cMaxSize);
        String::operator=(str);
    }

    /**
     * Assigns string to static string.
     *
     * @param str string to assign.
     * @return DynamicString&.
     */
    DynamicString& operator=(const String& str)
    {
        String::operator=(str);

        return *this;
    }

private:
    DynamicBuffer mBuffer;
};

} // namespace aos

#endif
