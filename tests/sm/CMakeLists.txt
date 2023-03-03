#
# Copyright (C) 2023 Renesas Electronics Corporation.
# Copyright (C) 2023 EPAM Systems, Inc.
#
# SPDX-License-Identifier: Apache-2.0
#

set(TARGET aossmcpp_test)

# ######################################################################################################################
# Sources
# ######################################################################################################################

set(SOURCES launcher/launcher_test.cpp)

# ######################################################################################################################
# Target
# ######################################################################################################################

add_executable(${TARGET} ${SOURCES})
target_link_libraries(${TARGET} GTest::gtest_main aossmcpp)

gtest_discover_tests(${TARGET})