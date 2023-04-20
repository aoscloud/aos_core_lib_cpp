/*
 * Copyright (C) 2023 Renesas Electronics Corporation.
 * Copyright (C) 2023 EPAM Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef AOS_LAUNCHER_HPP_
#define AOS_LAUNCHER_HPP_

#include <assert.h>

#include "aos/common/ocispec.hpp"
#include "aos/common/tools/array.hpp"
#include "aos/common/tools/memory.hpp"
#include "aos/common/tools/noncopyable.hpp"
#include "aos/common/types.hpp"
#include "aos/sm/config.hpp"
#include "aos/sm/instance.hpp"
#include "aos/sm/runner.hpp"
#include "aos/sm/service.hpp"
#include "aos/sm/servicemanager.hpp"

namespace aos {
namespace sm {
namespace launcher {

/** @addtogroup sm Service Manager
 *  @{
 */

/**
 * Instance launcher interface.
 */
class LauncherItf {
public:
    /**
     * Runs specified instances.
     *
     * @param instances instance info array.
     * @param forceRestart forces restart already started instance.
     * @return Error.
     */
    virtual Error RunInstances(const Array<ServiceInfo>& services, const Array<LayerInfo>& layers,
        const Array<InstanceInfo>& instances, bool forceRestart)
        = 0;

    /**
     * Runs last instances.
     *
     * @return Error.
     */
    virtual Error RunLastInstances() = 0;
};

/**
 * Interface to send instances run status.
 */
class InstanceStatusReceiverItf {
public:
    /**
     * Sends instances run status.
     *
     * @param instances instances status array.
     * @return Error.
     */
    virtual Error InstancesRunStatus(const Array<InstanceStatus>& instances) = 0;

    /**
     * Sends instances update status.
     * @param instances instances status array.
     *
     * @return Error.
     */
    virtual Error InstancesUpdateStatus(const Array<InstanceStatus>& instances) = 0;
};

/**
 * Launcher storage interface.
 */
class StorageItf {
public:
    /**
     * Adds new instance to storage.
     *
     * @param instance instance to add.
     * @return Error.
     */
    virtual Error AddInstance(const InstanceInfo& instance) = 0;

    /**
     * Updates previously stored instance.
     *
     * @param instance instance to update.
     * @return Error.
     */
    virtual Error UpdateInstance(const InstanceInfo& instance) = 0;

    /**
     * Removes previously stored instance.
     *
     * @param instanceID instance ID to remove.
     * @return Error.
     */
    virtual Error RemoveInstance(const InstanceIdent& instanceIdent) = 0;

    /**
     * Returns all stored instances.
     *
     * @param instances array to return stored instances.
     * @return Error.
     */
    virtual Error GetAllInstances(Array<InstanceInfo>& instances) = 0;

    /**
     * Destroys storage interface.
     */
    virtual ~StorageItf() = default;
};

/**
 * Launches service instances.
 */
class Launcher : public LauncherItf, public runner::RunStatusReceiverItf, private NonCopyable {
public:
    /**
     * Creates launcher instance.
     */
    Launcher() = default;

    /**
     * Destroys launcher instance.
     */
    ~Launcher() { mThread.Join(); }

    /**
     * Initializes launcher.
     *
     * @param statusReceiver status receiver instance.
     * @param runner runner instance.
     * @param storage storage instance.
     * @return Error.
     */
    Error Init(servicemanager::ServiceManagerItf& serviceManager, runner::RunnerItf& runner, OCISpecItf& ociManager,
        InstanceStatusReceiverItf& statusReceiver, StorageItf& storage);

    /**
     * Runs specified instances.
     *
     * @param services services info array.
     * @param layers layers info array.
     * @param instances instances info array.
     * @param forceRestart forces restart already started instance.
     * @return Error.
     */
    Error RunInstances(const Array<ServiceInfo>& services, const Array<LayerInfo>& layers,
        const Array<InstanceInfo>& instances, bool forceRestart = false) override;

    /**
     * Runs previously  instances.
     *
     * @return Error.
     */
    Error RunLastInstances() override;

    /**
     * Updates run instances status.
     *
     * @param instances instances state.
     * @return Error.
     */
    Error UpdateRunStatus(const Array<runner::RunStatus>& instances) override;

private:
    static constexpr auto cNumLaunchThreads = AOS_CONFIG_LAUNCHER_NUM_COOPERATE_LAUNCHES;
    static constexpr auto cThreadTaskSize = 256;

    void ProcessInstances(
        const Array<InstanceInfo>& instances, const Array<ServiceInfo>& services, bool forceRestart = false);
    void ProcessServices(const Array<ServiceInfo>& services);
    void ProcessLayers(const Array<LayerInfo>& layers);
    void SendRunStatus();
    void StopInstances(const Array<InstanceInfo>& instances, const Array<ServiceInfo>& services, bool forceRestart);
    void StartInstances(const Array<InstanceInfo>& instances);
    void CacheServices(const Array<InstanceInfo>& instances);
    void UpdateInstanceServices();

    RetWithError<const Service*> GetService(const String& serviceID) const
    {
        auto findService = mCurrentServices.Find(
            [&serviceID](const Service& service) { return service.Data().mServiceID == serviceID; });

        if (!findService.mError.IsNone()) {
            return {nullptr, findService.mError};
        }

        return findService.mValue;
    }

    Error StartInstance(const InstanceInfo& info);
    Error StopInstance(const InstanceIdent& ident);

    servicemanager::ServiceManagerItf* mServiceManager {};
    runner::RunnerItf*                 mRunner {};
    InstanceStatusReceiverItf*         mStatusReceiver {};
    StorageItf*                        mStorage {};
    OCISpecItf*                        mOCIManager {};

    StaticAllocator<sizeof(InstanceInfoStaticArray) + sizeof(ServiceInfoStaticArray) + sizeof(LayerInfoStaticArray)>
        mAllocator;

    bool                    mLaunchInProgress = false;
    Mutex                   mMutex;
    Thread<cThreadTaskSize> mThread;
    ThreadPool<cNumLaunchThreads, Max(cMaxNumInstances, cMaxNumServices, cMaxNumLayers), cThreadTaskSize> mLaunchPool;

    StaticArray<Service, cMaxNumServices>   mCurrentServices;
    StaticArray<Instance, cMaxNumInstances> mCurrentInstances;
};

/** @}*/

} // namespace launcher
} // namespace sm
} // namespace aos

#endif
