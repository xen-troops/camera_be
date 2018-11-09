/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */
#ifndef SRC_CAMERAMANAGER_HPP_
#define SRC_CAMERAMANAGER_HPP_

#include <unordered_map>

#include <xen/be/Log.hpp>

#include "CameraHandler.hpp"

class CameraManager
{
public:
    CameraManager();
    ~CameraManager();

    CameraHandlerPtr getCameraHandler(std::string uniqueId);

private:
    XenBackend::Log mLog;
    std::mutex mLock;

    std::unordered_map<std::string, CameraHandlerWeakPtr> mCameraHandlers;

    CameraHandlerPtr getNewCameraHandler(const std::string devName);
};

typedef std::shared_ptr<CameraManager> CameraManagerPtr;

#endif /* SRC_CAMERAMANAGER_HPP_ */
