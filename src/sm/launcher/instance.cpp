/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "aos/sm/instance.hpp"
#include "aos/common/tools/fs.hpp"
#include "aos/common/tools/memory.hpp"
#include "log.hpp"

namespace aos {
namespace sm {
namespace launcher {

/***********************************************************************************************************************
 * Static
 **********************************************************************************************************************/

size_t                                        Instance::sInstanceID = 0;
Mutex                                         Instance::sMutex {};
StaticAllocator<Instance::cSpecAllocatorSize> Instance::sAllocator {};

/***********************************************************************************************************************
 * Public
 **********************************************************************************************************************/

Instance::Instance(const InstanceInfo& info, OCISpecItf& ociManager, runner::RunnerItf& runner)
    : mInstanceID("instance-")
    , mInfo(info)
    , mOCIManager(ociManager)
    , mRunner(runner)
{
    StaticString<cInstanceIDLen> tmp;

    mInstanceID.Append(tmp.Convert(sInstanceID++));

    LOG_INF() << "Create instance: " << mInfo.mInstanceIdent << ", ID: " << *this;
}

void Instance::SetService(const Service* service, const Error& err)
{
    mService = service;
    mRunError = err;

    if (mService) {
        mAosVersion = mService->Data().mVersionInfo.mAosVersion;

        LOG_DBG() << "Set service " << *service << " for instance " << *this << ", Aos version: " << mAosVersion;
    }
}

Error Instance::Start()
{
    LOG_DBG() << "Start instance: " << *this;

    StaticString<cFilePathLen> instanceDir = FS::JoinPath(cRuntimeDir, mInstanceID);

    auto err = CreateRuntimeSpec(instanceDir);
    if (!err.IsNone()) {
        mRunError = err;

        return err;
    }

    auto runStatus = mRunner.StartInstance(mInstanceID, instanceDir);

    mRunState = runStatus.mState;
    mRunError = runStatus.mError;

    if (!runStatus.mError.IsNone()) {
        return runStatus.mError;
    }

    return ErrorEnum::eNone;
}

Error Instance::Stop()
{
    LOG_DBG() << "Stop instance: " << *this;

    StaticString<cFilePathLen> instanceDir = FS::JoinPath(cRuntimeDir, mInstanceID);
    Error                      stopErr;

    auto err = mRunner.StopInstance(mInstanceID);
    if (!err.IsNone() && stopErr.IsNone()) {
        stopErr = err;
    }

    err = FS::RemoveDir(instanceDir, true);
    if (!err.IsNone() && stopErr.IsNone()) {
        stopErr = AOS_ERROR_WRAP(err);
    }

    return stopErr;
}

/***********************************************************************************************************************
 * Private
 **********************************************************************************************************************/

Error Instance::CreateRuntimeSpec(const String& path)
{
    LockGuard lock(sMutex);

    LOG_DBG() << "Create runtime spec: " << path;

    auto err = FS::ClearDir(path, true);
    if (!err.IsNone()) {
        return AOS_ERROR_WRAP(err);
    }

    if (!mService) {
        return AOS_ERROR_WRAP(ErrorEnum::eNotFound);
    }

    auto imageSpec = mService->ImageSpec();
    if (!imageSpec.mError.IsNone()) {
        return imageSpec.mError;
    }

    auto serviceFS = mService->ServiceFSPath();
    if (!serviceFS.mError.IsNone()) {
        return serviceFS.mError;
    }

    auto runtimeSpec = MakeUnique<oci::RuntimeSpec>(&sAllocator);
    auto vm = MakeUnique<oci::VM>(&sAllocator);
    runtimeSpec->mVM = vm.Get();

    if (imageSpec.mValue.mConfig.mCmd.Size() == 0) {
        return AOS_ERROR_WRAP(ErrorEnum::eInvalidArgument);
    }

    runtimeSpec->mVM->mKernel.mPath = FS::JoinPath(serviceFS.mValue, imageSpec.mValue.mConfig.mCmd[0]);

    LOG_DBG() << "Unikernel path: " << runtimeSpec->mVM->mKernel.mPath;

    err = mOCIManager.SaveRuntimeSpec(FS::JoinPath(path, cRuntimeSpecFile), *runtimeSpec);
    if (!err.IsNone()) {
        return err;
    }

    return ErrorEnum::eNone;
}

} // namespace launcher
} // namespace sm
} // namespace aos
