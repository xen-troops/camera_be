/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */
#ifndef SRC_CAMERAHANDLER_HPP_
#define SRC_CAMERAHANDLER_HPP_

#include <map>
#include <unordered_map>

#include <xen/be/Log.hpp>
#include <xen/be/Utils.hpp>

#include <xen/io/cameraif.h>

#include "Camera.hpp"
#include "FrontendBuffer.hpp"

class CameraHandler
{
public:
    CameraHandler(std::string uniqueId);
    ~CameraHandler();

    void configToXen(xencamera_config_resp *cfg_resp);
    void configSetTry(const xencamera_req& aReq, xencamera_resp& aResp,
                      bool is_set);

    void configSet(const xencamera_req& aReq, xencamera_resp& aResp);
    void configGet(const xencamera_req& aReq, xencamera_resp& aResp);
    void configValidate(const xencamera_req& aReq, xencamera_resp& aResp);

    void frameRateSet(const xencamera_req& aReq, xencamera_resp& aResp);

    void bufGetLayout(const xencamera_req& aReq, xencamera_resp& aResp);
    void bufRequest(const xencamera_req& aReq, xencamera_resp& aResp,
                    domid_t domId);
    size_t bufGetImageSize(domid_t domId);
    void bufCreate(const xencamera_req& aReq, xencamera_resp& aResp,
                   domid_t domId);
    void bufDestroy(const xencamera_req& aReq, xencamera_resp& aResp,
                    domid_t domId);
    void bufQueue(const xencamera_req& aReq, xencamera_resp& aResp,
                  domid_t domId);
    void bufDequeue(const xencamera_req& aReq, xencamera_resp& aResp,
                    domid_t domId);

    void ctrlEnum(const xencamera_req& aReq, xencamera_resp& aResp,
                  std::string name);
    void ctrlSet(const xencamera_req& aReq, xencamera_resp& aResp,
                 std::string name);
    void ctrlGet(const xencamera_req& aReq, xencamera_resp& aResp,
                 std::string name);

    void streamStart(const xencamera_req& aReq, xencamera_resp& aResp);
    void streamStop(const xencamera_req& aReq, xencamera_resp& aResp);

    typedef std::function<int(int, uint8_t *, size_t)> FrameListener;
    typedef std::function<void(int, int64_t)> ControlListener;

    struct Listeners {
        FrameListener frame;
        ControlListener control;
    };

    void listenerSet(domid_t domId, Listeners listeners);
    void listenerReset(domid_t domId);

private:
    XenBackend::Log mLog;

    CameraPtr mCamera;

    std::unordered_map<domid_t, Listeners> mListeners;

    void init(std::string uniqueId);
    void release();

    int onFrameDoneCallback(int index, int size);
};

typedef std::shared_ptr<CameraHandler> CameraHandlerPtr;
typedef std::weak_ptr<CameraHandler> CameraHandlerWeakPtr;

#endif /* SRC_CAMERAHANDLER_HPP_ */
