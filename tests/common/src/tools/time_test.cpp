/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gmock/gmock.h>

#include "../log.hpp"
#include "aos/common/tools/time.hpp"

using namespace testing;
using namespace aos;
using namespace aos::time;

class TimeTest : public Test {
private:
    void SetUp() override
    {
        Log::SetCallback([](LogModule module, LogLevel level, const String& message) {
            static std::mutex sLogMutex;

            std::lock_guard<std::mutex> lock(sLogMutex);

            std::cout << level.ToString().CStr() << " | " << module.ToString().CStr() << " | " << message.CStr()
                      << std::endl;
        });
    }
};

TEST_F(TimeTest, Add4Years)
{
    Time now             = Time::Now();
    Time fourYearsLater  = now.Add(Years(4));
    Time fourYearsBefore = now.Add(Years(-4));

    LOG_INF() << "Time now: " << now;
    LOG_INF() << "Four years later: " << fourYearsLater;

    EXPECT_EQ(now.UnixNano() + Years(4), fourYearsLater.UnixNano());
    EXPECT_EQ(now.UnixNano() + Years(-4), fourYearsBefore.UnixNano());
}

TEST_F(TimeTest, Compare)
{
    Time now = Time::Now();

    const Duration year       = Years(1);
    const Duration oneNanosec = 1;

    EXPECT_TRUE(now < now.Add(year));
    EXPECT_TRUE(now < now.Add(oneNanosec));

    EXPECT_FALSE(now.Add(oneNanosec) < now);
    EXPECT_FALSE(now < now);
}