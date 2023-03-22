/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_OCISPEC_HPP_
#define AOS_OCISPEC_HPP_

#include "aos/common/string.hpp"
#include "aos/common/types.hpp"

namespace aos {
namespace oci {

/**
 * Spec parameter max len.
 */
constexpr auto cParamLen = 64;

/**
 * Spec parameter max count.
 */
constexpr auto cParamCount = 8;

/**
 * Spec version max len.
 */
constexpr auto cSpecVersionLen = 32;

/**
 * OCI image config.
 */
struct ImageConfig {
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mEntryPoint;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mCmd;
};

/**
 * OCI image specification.
 */
struct ImageSpec {
    ImageConfig mConfig;
};

/**
 * Contains information about the hypervisor to use for a virtual machine.
 */
struct VMHypervisor {
    StaticString<cFilePathLen>                              mPath;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mParameters;
};

/**
 * Contains information about the kernel to use for a virtual machine.
 */
struct VMKernel {
    StaticString<cFilePathLen>                              mPath;
    StaticArray<StaticString<cMaxParamLen>, cMaxParamCount> mParameters;
};

/**
 * Contains information for virtual-machine-based containers.
 */
struct VM {
    VMHypervisor mHypervisor;
    VMKernel     mKernel;
};

/**
 * OCI runtime specification.
 */
struct RuntimeSpec {
    StaticString<cSpecVersionLen> mVersion;
    VM*                           mVM;
};

} // namespace oci

/**
 * OCI spec interface.
 */
class OCISpecItf {
public:
    /**
     * Loads OCI image spec.
     *
     * @param path file path.
     * @param imageSpec image spec.
     * @return Error.
     */
    virtual Error LoadImageSpec(const String& path, oci::ImageSpec& imageSpec) = 0;

    /**
     * Saves OCI image spec.
     *
     * @param path file path.
     * @param imageSpec image spec.
     * @return Error.
     */
    virtual Error SaveImageSpec(const String& path, const oci::ImageSpec& imageSpec) = 0;

    /**
     * Loads OCI runtime spec.
     *
     * @param path file path.
     * @param runtimeSpec runtime spec.
     * @return Error.
     */
    virtual Error LoadRuntimeSpec(const String& path, oci::RuntimeSpec& runtimeSpec) = 0;

    /**
     * Saves OCI runtime spec.
     *
     * @param path file path.
     * @param runtimeSpec runtime spec.
     * @return Error.
     */
    virtual Error SaveRuntimeSpec(const String& path, const oci::RuntimeSpec& runtimeSpec) = 0;

    /**
     * Destroys OCI spec interface.
     */
    virtual ~OCISpecItf() = default;
};

} // namespace aos

#endif
