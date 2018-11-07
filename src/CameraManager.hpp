/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */
#ifndef SRC_CAMERAMANGER_HPP_
#define SRC_CAMERAMANGER_HPP_

#include <unordered_map>

#include <xen/be/Log.hpp>

#include "Camera.hpp"

class CameraManager
{
public:
    CameraManager();
    ~CameraManager();

    CameraPtr getCamera(std::string uniqueId);

private:
    XenBackend::Log mLog;
    std::mutex mLock;

    std::unordered_map<std::string, CameraWeakPtr> mCameras;

    CameraPtr getNewCamera(const std::string devName);
};

typedef std::shared_ptr<CameraManager> CameraManagerPtr;

#endif /* SRC_CAMERAMANGER_HPP_ */
