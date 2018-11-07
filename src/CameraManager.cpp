// SPDX-License-Identifier: GPL-2.0

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */

#include <xen/be/Exception.hpp>

#include "CameraManager.hpp"

using XenBackend::Exception;

CameraManager::CameraManager():
    mLog("CameraManager")
{
}

CameraManager::~CameraManager()
{
}

CameraPtr CameraManager::getNewCamera(const std::string devName)
{
        return CameraPtr(new Camera(devName));
}

CameraPtr CameraManager::getCamera(std::string uniqueId)
{
    std::lock_guard<std::mutex> lock(mLock);

    auto it = mCameras.find(uniqueId);

    if (it != mCameras.end())
        if (auto camera = it->second.lock())
            return camera;

    /* This camera is not on the list yet - create now. */
    auto camera = getNewCamera(uniqueId);

    mCameras[uniqueId] = camera;

    return camera;
}

