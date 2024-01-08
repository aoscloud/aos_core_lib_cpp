/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "aos/common/uuid.hpp"

namespace aos {
namespace uuid {

TEST(CommonTest, CreateUUID)
{
    static constexpr auto cTestUUIDsCount = 1000;

    Error err = ErrorEnum::eNone;

    std::vector<UUID> uuids;
    UUIDManager       manager;

    for (int i = 0; i < cTestUUIDsCount; i++) {
        UUID tmp;

        Tie(tmp, err) = manager.CreateUUID();
        ASSERT_TRUE(err.IsNone());
        ASSERT_EQ(tmp.Size(), tmp.MaxSize());

        uuids.push_back(tmp);
    }

    UUID prevUUID;
    std::sort(uuids.begin(), uuids.end());

    for (const auto& cur : uuids) {
        ASSERT_NE(prevUUID, cur);
        prevUUID = cur;
    }
}

TEST(CommonTest, UUIDToString)
{
    Error                           err = ErrorEnum::eNone;
    StaticString<uuid::cUUIDStrLen> destination;

    uint8_t uuidBlob[uuid::cUUIDLen]
        = {0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x9A, 0xAB, 0xBC, 0xCD, 0xDE, 0xEF, 0xFF};
    UUID source = Array<uint8_t>(uuidBlob, uuid::cUUIDLen);

    Tie(destination, err) = UUIDManager().UUIDToString(source);
    ASSERT_TRUE(err.IsNone());

    static constexpr auto expected = "01122334-4556-6778-899A-ABBCCDDEEFFF";
    EXPECT_EQ(destination, expected);
}

TEST(CommonTest, StringToUUID)
{
    Error   err = ErrorEnum::eNone;
    UUID    destination;
    uint8_t expected[uuid::cUUIDLen]
        = {0x01, 0x12, 0x23, 0x34, 0x45, 0x56, 0x67, 0x78, 0x89, 0x9A, 0xAB, 0xBC, 0xCD, 0xDE, 0xEF, 0xFF};

    Tie(destination, err) = UUIDManager().StringToUUID("01122334-4556-6778-899A-ABBCCDDEEFFF");
    ASSERT_TRUE(err.IsNone());

    EXPECT_EQ(destination, Array<uint8_t>(expected, uuid::cUUIDLen));
}

} // namespace uuid
} // namespace aos
