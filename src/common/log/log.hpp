// SPDX-License-Identifier: Apache-2.0
//
// Copyright (C) 2023 Renesas Electronics Corporation.
// Copyright (C) 2023 EPAM Systems, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef LOG_COMMON_HPP_
#define LOG_COMMON_HPP_

#include <stdio.h>
#include <string.h>

#include "config.hpp"
#include "utils/stringer.hpp"

/**
 * Helper macro to display debug log.
 */
#if CONFIG_LOG_LEVEL >= CONFIG_LOG_LEVEL_DEBUG
#define LOG_MODULE_DBG(module) aos::Log(module, aos::LogLevelEnum::eDebug)
#else
#define LOG_MODULE_DBG(module) true ? (void)0 : aos::LogVoid() & aos::Log(module, aos::LogLevelEnum::eDebug)
#endif

/**
 * Helper macro to display info log.
 */
#if CONFIG_LOG_LEVEL >= CONFIG_LOG_LEVEL_INFO
#define LOG_MODULE_INF(module) aos::Log(module, aos::LogLevelEnum::eInfo)
#else
#define LOG_MODULE_INF(module) true ? (void)0 : aos::LogVoid() & Log(module, aos::LogLevelEnum::eInfo)
#endif

/**
 * Helper macro to display warning log.
 */
#if CONFIG_LOG_LEVEL >= CONFIG_LOG_LEVEL_WARNING
#define LOG_MODULE_WRN(module) aos::Log(module, aos::LogLevelEnum::eWarning)
#else
#define LOG_MODULE_WRN(module) true ? (void)0 : aos::LogVoid() & Log(module, aos::LogLevelEnum::eWarning)
#endif

/**
 * Helper macro to display error log.
 */
#if CONFIG_LOG_LEVEL >= CONFIG_LOG_LEVEL_ERROR
#define LOG_MODULE_ERR(module) aos::Log(module, aos::LogLevelEnum::eError)
#else
#define LOG_MODULE_ERR(module) true ? (void)0 : aos::LogVoid() & Log(module, aos::LogLevelEnum::eError)
#endif

namespace aos {

/**
 * Log level types.
 */
class LogLevelType {
public:
    enum class Enum { eDebug, eInfo, eWarning, eError, eNumLevels };

    static Pair<const char* const*, size_t> GetStrings()
    {
        static const char* const cLogLevelStrings[static_cast<size_t>(Enum::eNumLevels)]
            = {"debug", "info", "warning", "error"};

        return Pair<const char* const*, size_t>(cLogLevelStrings, static_cast<size_t>(Enum::eNumLevels));
    };
};

typedef LogLevelType::Enum         LogLevelEnum;
typedef EnumStringer<LogLevelType> LogLevel;

/**
 * Log module types.
 */
class LogModuleType {
public:
    enum class Enum { eDefault, eSMLauncher, eIAMCertHandler, eNumModules };

    static Pair<const char* const*, size_t> GetStrings()
    {
        static const char* const cLogLevelStrings[static_cast<size_t>(Enum::eNumModules)]
            = {"default", "launcher", "certhandler"};

        return Pair<const char* const*, size_t>(cLogLevelStrings, static_cast<size_t>(Enum::eNumModules));
    };
};

typedef LogModuleType::Enum         LogModuleEnum;
typedef EnumStringer<LogModuleType> LogModule;

/**
 * Log line callback. Should be set in application to display log using application logging mechanism.
 */
typedef void (*LogCallback)(LogModule module, LogLevel level, const char* message);

/**
 * Implements log functionality.
 */
class Log {
public:
    Log(Log const&) = delete;
    void operator=(Log const&) = delete;

    /**
     * Constructs a new Log object.
     *
     * @param module log module type.
     * @param level log level type.
     */
    Log(LogModule module, LogLevel level)
        : mModule(module)
        , mLevel(level)
        , mCurrentLen(0)
    {
        mBuffer[0] = '\0';
    };

    /**
     * Destroys the Log object and calls log callback.
     */
    ~Log()
    {
        auto callback = GetCallback();

        if (callback != nullptr) {
            callback(mModule, mLevel, mBuffer);
        }
    }

    /**
     * Sets application log callback.
     *
     * @param callback
     */
    static void SetCallback(LogCallback callback) { GetCallback() = callback; }

    /**
     * Logs string.
     *
     * @param str string to log.
     * @return Log&
     */
    Log& operator<<(const char* str)
    {
        auto n = cLineSize - mCurrentLen - 1;
        auto l = strlen(str);

        if (l > n) {
            strncpy(&mBuffer[mCurrentLen], str, n);

            mBuffer[mCurrentLen + n] = '\0';
            l = n;

            addPeriods();
        } else {
            strcpy(&mBuffer[mCurrentLen], str);
        }

        mCurrentLen += l;

        return *this;
    };

    /**
     * Logs object that implements Stringer interface.
     *
     * @param stringer object to log.
     * @return Log&
     */
    Log& operator<<(const Stringer& stringer) { return *this << stringer.ToString(); };

    /**
     * Logs int.
     *
     * @param i integer to log.
     * @return Log&
     */
    Log& operator<<(int i)
    {
        auto n = cLineSize - mCurrentLen;

        auto l = snprintf(&mBuffer[mCurrentLen], n, "%d", i);
        if (l < 0) {
            return *this;
        }

        if (static_cast<size_t>(l) > n) {
            addPeriods();

            l = n;
        }

        mCurrentLen += l;

        return *this;
    };

private:
    static size_t constexpr cLineSize = CONFIG_LOG_LINE_SIZE;

    char      mBuffer[cLineSize];
    LogModule mModule;
    LogLevel  mLevel;
    size_t    mCurrentLen;

    static LogCallback& GetCallback()
    {
        static LogCallback sLogCallback;

        return sLogCallback;
    }

    void addPeriods()
    {
        if (cLineSize > 3) {
            strcpy(&mBuffer[cLineSize - 4], "...");
        }
    }
};

/**
 *
 */
class LogVoid {
public:
    void operator&(const Log&) { }
};

} // namespace aos

#endif
