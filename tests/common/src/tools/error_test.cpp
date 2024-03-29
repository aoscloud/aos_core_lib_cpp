/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gtest/gtest.h>

#include "aos/common/tools/error.hpp"

using namespace aos;

Error successFunction()
{
    return ErrorEnum::eNone;
}

Error failedFunction()
{
    return ErrorEnum::eFailed;
}

TEST(ErrorTest, Basic)
{
    // Compare errors

    EXPECT_TRUE(Error(ErrorEnum::eFailed).Is(ErrorEnum::eFailed));
    EXPECT_TRUE(Error(ErrorEnum::eFailed).Is(Error(ErrorEnum::eFailed)));
    EXPECT_TRUE(Error(ErrorEnum::eFailed) == ErrorEnum::eFailed);
    EXPECT_TRUE(Error(ErrorEnum::eFailed) != ErrorEnum::eNone);
    EXPECT_TRUE(ErrorEnum::eFailed == Error(ErrorEnum::eFailed));
    EXPECT_TRUE(ErrorEnum::eNone != Error(ErrorEnum::eFailed));

    // Function handling

    EXPECT_TRUE(successFunction().IsNone());
    auto successErr = successFunction();
    EXPECT_FALSE(!successErr.IsNone());
    EXPECT_FALSE(failedFunction().IsNone());
    auto failedErr = failedFunction();
    EXPECT_TRUE(!failedErr.IsNone());

    // Error string

    EXPECT_EQ(strcmp(Error(ErrorEnum::eNone).Message(), "none"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eFailed).Message(), "failed"), 0);

    // Errno handling

    EXPECT_TRUE(Error(0).IsNone());
    EXPECT_FALSE(Error(EINVAL).IsNone());
    EXPECT_TRUE(Error(ENODEV).Is(ENODEV));
    EXPECT_EQ(strcmp(Error(EAGAIN).Message(), "Resource temporarily unavailable"), 0);

    // Error wrap

    auto funcErr = AOS_ERROR_WRAP(failedFunction());
    EXPECT_EQ(funcErr.LineNumber(), __LINE__ - 1);
    EXPECT_EQ(strcmp(funcErr.Message(), "failed"), 0);

    auto enumErr = AOS_ERROR_WRAP(ErrorEnum::eNoMemory);
    EXPECT_EQ(enumErr.LineNumber(), __LINE__ - 1);
    EXPECT_EQ(strcmp(enumErr.Message(), "not enough memory"), 0);

    auto copyErr(enumErr);
    EXPECT_EQ(copyErr.LineNumber(), enumErr.LineNumber());
    EXPECT_EQ(strcmp(copyErr.Message(), enumErr.Message()), 0);

    Error assignErr;

    assignErr = copyErr;
    EXPECT_EQ(assignErr.LineNumber(), enumErr.LineNumber());
    EXPECT_EQ(strcmp(assignErr.Message(), enumErr.Message()), 0);

    auto errnoErr = AOS_ERROR_WRAP(EAGAIN);
    EXPECT_EQ(errnoErr.LineNumber(), __LINE__ - 1);
    EXPECT_EQ(strcmp(errnoErr.Message(), "Resource temporarily unavailable"), 0);
}

TEST(ErrorTest, Messages)
{
    EXPECT_EQ(strcmp(Error(ErrorEnum::eNone).Message(), "none"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eFailed).Message(), "failed"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eRuntime).Message(), "runtime error"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eNoMemory).Message(), "not enough memory"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eOutOfRange).Message(), "out of range"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eInvalidArgument).Message(), "invalid argument"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eNotFound).Message(), "not found"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eAlreadyExist).Message(), "already exist"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eWrongState).Message(), "wrong state"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eInvalidChecksum).Message(), "invalid checksum"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eAlreadyLoggedIn).Message(), "already logged in"), 0);
    EXPECT_EQ(strcmp(Error(ErrorEnum::eNotSupported).Message(), "not supported"), 0);
}

TEST(ErrorTest, Tie)
{
    Error err = ErrorEnum::eNone;
    bool  val = false;

    Tie(val, err) = RetWithError<bool>(true, ErrorEnum::eFailed);

    EXPECT_EQ(val, true);
    EXPECT_EQ(err, ErrorEnum::eFailed);
}
