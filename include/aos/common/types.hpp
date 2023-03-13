/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_TYPES_HPP_
#define AOS_TYPES_HPP_

#include <cstdint>

#include "aos/common/config/types.hpp"
#include "aos/common/error.hpp"
#include "aos/common/stringer.hpp"

namespace aos {

/*
 * Provider ID len.
 */
constexpr auto cProviderIDLen = AOS_CONFIG_TYPES_PROVIDER_ID_LEN;

/*
 * Service ID len.
 */
constexpr auto cServiceIDLen = AOS_CONFIG_TYPES_SERVICE_ID_LEN;

/*
 * Subject ID len.
 */
constexpr auto cSubjectIDLen = AOS_CONFIG_TYPES_SUBJECT_ID_LEN;

/*
 * Layer ID len.
 */
constexpr auto cLayerIDLen = AOS_CONFIG_TYPES_LAYER_ID_LEN;

/*
 * Layer digest len.
 */
constexpr auto cLayerDigestLen = AOS_CONFIG_TYPES_LAYER_DIGEST_LEN;

/*
 * Instance ID len.
 */
constexpr auto cInstanceIDLen = AOS_CONFIG_TYPES_INSTANCE_ID_LEN;

/*
 * File path len.
 */
constexpr auto cFilePathLen = AOS_CONFIG_TYPES_FILE_PATH_LEN;

/*
 * URL len.
 */
constexpr auto cURLLen = AOS_CONFIG_TYPES_URL_LEN;

/**
 * Vendor version len.
 */
constexpr auto cVendorVersionLen = AOS_CONFIG_TYPES_VENDOR_VERSION_LEN;

/**
 * Service/layer description len.
 */

constexpr auto cDescriptionLen = AOS_CONFIG_TYPES_DESCRIPTION_LEN;

/**
 * SHA256 size.
 */
constexpr auto cSHA256Size = 32;

/**
 * SHA512 size.
 */
constexpr auto cSHA512Size = 64;

/**
 * Instance identification.
 */
struct InstanceIdent {
    char     mServiceID[cServiceIDLen + 1];
    char     mSubjectID[cSubjectIDLen + 1];
    uint64_t mInstance;
};

/**
 * Instance info.
 */
struct InstanceInfo {
    InstanceIdent mInstanceIdent;
    uint32_t      mUID;
    uint64_t      mPriority;
    char          mStoragePath[cFilePathLen + 1];
    char          mStatePath[cFilePathLen + 1];
};

/**
 * Instance run state.
 */
class InstanceRunStateType {
public:
    enum class Enum { eActive, eFailed, eNumStates };

    static Pair<const char* const*, size_t> GetStrings()
    {
        static const char* const cInstanceRunStateStrings[static_cast<size_t>(Enum::eNumStates)] = {"active", "failed"};

        return Pair<const char* const*, size_t>(cInstanceRunStateStrings, static_cast<size_t>(Enum::eNumStates));
    };
};

using InstanceRunStateEnum = InstanceRunStateType::Enum;
using InstanceRunState = EnumStringer<InstanceRunStateType>;

/**
 * Instance status.
 */
struct InstanceStatus {
    InstanceIdent    mInstanceIdent;
    uint64_t         mAosVersion;
    InstanceRunState mRunState;
    Error            mError;
};

/**
 * Version info.
 */
struct VersionInfo {
    uint64_t mAosVersion;
    char     mVendorVersion[cVendorVersionLen + 1];
    char     mDescription[cDescriptionLen + 1];
};

/**
 * Service info.
 */

struct ServiceInfo {
    VersionInfo mVersionInfo;
    char        mServiceID[cServiceIDLen + 1];
    char        mProviderID[cProviderIDLen + 1];
    uint32_t    mGID;
    char        mURL[cURLLen + 1];
    uint8_t     mSHA256[cSHA256Size];
    uint8_t     mSHA512[cSHA512Size];
    size_t      mSize;
};

/**
 * Layer info.
 */

// LayerInfo layer info.
struct LayerInfo {
    VersionInfo mVersionInfo;
    char        mLayerID[cLayerIDLen + 1];
    char        mLayerDigest[cLayerDigestLen + 1];
    char        mURL[cURLLen + 1];
    uint8_t     mSHA256[cSHA256Size];
    uint8_t     mSHA512[cSHA512Size];
    size_t      mSize;
};

} // namespace aos

#endif
