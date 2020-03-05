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
#include "MediaController.hpp"
#include "FrontendBuffer.hpp"

class CameraHandler
{
public:
    CameraHandler(std::string uniqueId);
    ~CameraHandler();

    void configToXen(xencamera_config_resp *cfg_resp);
    void configSetTry(const xencamera_req& aReq, xencamera_resp& aResp,
                      bool is_set);

    void configSet(domid_t domId, const xencamera_req& aReq,
                   xencamera_resp& aResp);
    void configGet(domid_t domId, const xencamera_req& aReq,
                   xencamera_resp& aResp);
    void configValidate(domid_t domId, const xencamera_req& aReq,
                        xencamera_resp& aResp);

    void frameRateSet(domid_t domId, const xencamera_req& aReq,
                      xencamera_resp& aResp);

    void bufGetLayout(domid_t domId, const xencamera_req& aReq,
                      xencamera_resp& aResp);
    void bufRequest(domid_t domId, const xencamera_req& aReq,
                    xencamera_resp& aResp);
    void bufRelease(domid_t domId);
    size_t bufGetImageSize(domid_t domId);

    void ctrlEnum(domid_t domId, const xencamera_req& aReq,
                  xencamera_resp& aResp, std::string name);
    void ctrlSet(domid_t domId, const xencamera_req& aReq,
                 xencamera_resp& aResp,std::string name);
    void ctrlGet(domid_t domId, const xencamera_req& aReq,
                 xencamera_resp& aResp, std::string name);

    void streamStart(domid_t domId, const xencamera_req& aReq,
                     xencamera_resp& aResp);
    void streamStop(domid_t domId, const xencamera_req& aReq,
                    xencamera_resp& aResp);

    /* data, size */
    typedef std::function<void(uint8_t *, size_t)> FrameListener;
    /* name, value */
    typedef std::function<void(const std::string, int64_t)> ControlListener;

    struct Listeners {
        FrameListener frame;
        ControlListener control;
    };

    void listenerSet(domid_t domId, Listeners listeners);
    void listenerReset(domid_t domId);

private:
    XenBackend::Log mLog;
    std::mutex mLock;

    CameraPtr mCamera;
    MediaControllerPtr mMediaController;

    /*
     * These help to make a decision if a requst from a frontend
     * needs to directly go to HW camera device or needs to be emulated:
     * for example, if one of the frontends has already set configuration
     * what needs to be done if the other one wants to set some other
     * diffferent configuration etc.
     * FIXME: For now, for simplicity, we expect all the frontends to
     * have the same configuration in order to avoid clashes.
     * On the other hand, according to http://www.mail-archive.com/linux-media@vger.kernel.org/msg56550.html
     * if frontend requests "wrong" configuration, e.g. different from what is
     * already set, then it should be ok to return actual format, not the
     * desired one.
     * FIXME: the assumption above introduces a race condition between the
     * frontends willing to set different configurations/formats.
     * In order to avoid misbehaviour, e.g. when frontend-1 sets format first
     * and then frontend-2 changes it to something different and there is
     * no way to notify frontend-1 and its user-space of such a change, we
     * only accept the very first set format and then emulate it to the rest.
     */
    bool mFormatSet;
    bool mFramerateSet;
    int mNumBuffersAllocated;

    std::unordered_map<domid_t, int> mBuffersAllocated;
    std::unordered_map<domid_t, bool> mStreamingNow;

    /* TODO: This needs to be a configuration option of the backend. */
    static const int BE_CONFIG_NUM_BUFFERS = 4;

    std::unordered_map<domid_t, Listeners> mListeners;

    void init(std::string uniqueId);
    void release();

    void onFrameDoneCallback(int index, int size);

    void parseUniqueId(const std::string& uniqueId, std::string& videoId,
        std::string& mediaId);
};

typedef std::shared_ptr<CameraHandler> CameraHandlerPtr;
typedef std::weak_ptr<CameraHandler> CameraHandlerWeakPtr;

#endif /* SRC_CAMERAHANDLER_HPP_ */
