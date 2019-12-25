/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */

#ifndef SRC_BACKEND_HPP_
#define SRC_BACKEND_HPP_

#include <list>
#include <string>

#include <xen/be/BackendBase.hpp>
#include <xen/be/FrontendHandlerBase.hpp>
#include <xen/be/RingBufferBase.hpp>
#include <xen/be/Log.hpp>

#include <xen/io/cameraif.h>

#include "CameraManager.hpp"

class CameraFrontendHandler : public XenBackend::FrontendHandlerBase
{
public:
    CameraFrontendHandler(CameraManagerPtr cameraManager,
                          const std::string& devName,
                          domid_t domId, uint16_t devId) :
        FrontendHandlerBase("CameraFrontend", devName,
                            domId, devId),
        mLog("CameraFrontend"),
        mCameraManager(cameraManager) {}

protected:
    void onBind() override;
    void onStateClosed() override;

private:
    XenBackend::Log mLog;

    CameraManagerPtr mCameraManager;
    CameraHandlerPtr mCameraHandler;
};

class Backend : public XenBackend::BackendBase
{
public:
    Backend(const std::string& deviceName);
    ~Backend();

private:
    XenBackend::Log mLog;

    CameraManagerPtr mCameraManager;

    void init();
    void release();

    void onNewFrontend(domid_t domId, uint16_t devId);
};

#endif /* SRC_BACKEND_HPP_ */
