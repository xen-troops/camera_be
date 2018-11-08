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

CameraHandlerPtr CameraManager::getNewCameraHandler(const std::string devName)
{
    return CameraHandlerPtr(new CameraHandler(devName));
}

CameraHandlerPtr CameraManager::getCameraHandler(std::string uniqueId)
{
    std::lock_guard<std::mutex> lock(mLock);

    auto it = mCameraHandlers.find(uniqueId);

    if (it != mCameraHandlers.end())
        if (auto cameraHandler = it->second.lock())
            return cameraHandler;

    /* This camera handler is not on the list yet - create now. */
    auto cameraHandler = getNewCameraHandler(uniqueId);

    mCameraHandlers[uniqueId] = cameraHandler;

    return cameraHandler;
}

