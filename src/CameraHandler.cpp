
// SPDX-License-Identifier: GPL-2.0

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018-2019 EPAM Systems Inc.
 */

#include <algorithm>
#include <iomanip>

#include <xen/be/Exception.hpp>

#include "CameraHandler.hpp"
#include "V4L2ToXen.hpp"

using namespace std::placeholders;

CameraHandler::CameraHandler(std::string uniqueId) :
    mLog("CameraHandler")
{
    LOG(mLog, DEBUG) << "Create camera handler";

    try {
        init(uniqueId);
    } catch (...) {
        /*
          If initialization failed, we assume that camera HW does not work properly
          or is not connected.
          In such case we will:
          - not create instance of Camera class
          - handle all requests but instead of interaction with HW will return 'empty' responses
        */
        LOG(mLog, ERROR) << "Camera initialization failed, so we will run without hardware.";
        mCamera.reset();
    }
}

CameraHandler::~CameraHandler()
{
    LOG(mLog, DEBUG) << "Delete camera handler";

    release();
}

void CameraHandler::init(std::string uniqueId)
{
    mFormatSet = false;
    mFramerateSet = false;
    mBuffersAllocated.clear();
    mStreamingNow.clear();
    mCamera.reset(new Camera(uniqueId));
}

void CameraHandler::listenerSet(domid_t domId, Listeners listeners)
{
    std::lock_guard<std::mutex> lock(mLock);

    mListeners.emplace(domId, listeners);
}

void CameraHandler::listenerReset(domid_t domId)
{
    std::lock_guard<std::mutex> lock(mLock);

    mListeners.erase(domId);
}

void CameraHandler::configToXen(xencamera_config_resp *cfg_resp)
{
    if (!mCamera) {
        memset(cfg_resp, 0, sizeof(xencamera_config_resp));
        return;
    }

    v4l2_format fmt = mCamera->formatGet();

    cfg_resp->pixel_format = fmt.fmt.pix.pixelformat;
    cfg_resp->width = fmt.fmt.pix.width;
    cfg_resp->height = fmt.fmt.pix.height;

    cfg_resp->colorspace = V4L2ToXen::colorspaceToXen(fmt.fmt.pix.colorspace);

    cfg_resp->xfer_func = V4L2ToXen::xferToXen(fmt.fmt.pix.xfer_func);

    cfg_resp->ycbcr_enc = V4L2ToXen::ycbcrToXen(fmt.fmt.pix.ycbcr_enc);

    cfg_resp->quantization = V4L2ToXen::quantizationToXen(fmt.fmt.pix.quantization);

    /* TODO: This needs to be properly handled. */
    cfg_resp->displ_asp_ratio_numer = 1;
    cfg_resp->displ_asp_ratio_denom = 1;

    v4l2_fract frameRate = mCamera->frameRateGet();

    cfg_resp->frame_rate_numer = frameRate.numerator;
    cfg_resp->frame_rate_denom = frameRate.denominator;
}

void CameraHandler::configSetTry(const xencamera_req& aReq,
                                 xencamera_resp& aResp, bool is_set)
{
    if (!mCamera) {
        memset(&aResp, 0, sizeof aResp);
        return;
    }

    const xencamera_config_req *cfg_req = &aReq.req.config;

    v4l2_format fmt {0};

    fmt.fmt.pix.pixelformat = cfg_req->pixel_format;
    fmt.fmt.pix.width = cfg_req->width;
    fmt.fmt.pix.height = cfg_req->height;

    if (is_set)
        mCamera->formatSet(fmt);
    else
        mCamera->formatTry(fmt);

    configToXen(&aResp.resp.config);
}

void CameraHandler::configSet(domid_t domId, const xencamera_req& aReq,
                              xencamera_resp& aResp)
{
    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [CONFIG SET] dom " <<
        std::to_string(domId);

    if (mFormatSet) {
        configToXen(&aResp.resp.config);
    } else {
        configSetTry(aReq, aResp, true);
        mFormatSet = true;
    }
}

void CameraHandler::configValidate(domid_t domId, const xencamera_req& aReq,
                                   xencamera_resp& aResp)
{
    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [CONFIG VALIDATE] dom " <<
        std::to_string(domId);

    if (mFormatSet)
        configToXen(&aResp.resp.config);
    else
        configSetTry(aReq, aResp, false);
}

void CameraHandler::configGet(domid_t domId, const xencamera_req& aReq,
                              xencamera_resp& aResp)
{
    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [CONFIG GET] dom " <<
        std::to_string(domId);

    configToXen(&aResp.resp.config);
}

void CameraHandler::frameRateSet(domid_t domId, const xencamera_req& aReq,
                                 xencamera_resp& aResp)
{
    if (!mCamera) {
        memset(&aResp, 0, sizeof aResp);
        return;
    }

    std::lock_guard<std::mutex> lock(mLock);
    const xencamera_frame_rate_req *req = &aReq.req.frame_rate;

    DLOG(mLog, DEBUG) << "Handle command [FRAME RATE SET] dom " <<
        std::to_string(domId);

    if (mFramerateSet) {
    } else {
        mCamera->frameRateSet(req->frame_rate_numer, req->frame_rate_denom);
        mFramerateSet = true;
    }
}

void CameraHandler::bufGetLayout(domid_t domId, const xencamera_req& aReq,
                                 xencamera_resp& aResp)
{
    if (!mCamera) {
        memset(&aResp, 0, sizeof aResp);
        return;
    }

    std::lock_guard<std::mutex> lock(mLock);
    xencamera_buf_get_layout_resp *resp = &aResp.resp.buf_layout;

    DLOG(mLog, DEBUG) << "Handle command [BUF GET LAYOUT] dom " <<
        std::to_string(domId);

    v4l2_format fmt = mCamera->formatGet();

    DLOG(mLog, DEBUG) << "Handle command [BUF GET LAYOUT] size " <<
        fmt.fmt.pix.sizeimage;

    /* XXX: Single plane only. */
    resp->num_planes = 1;
    resp->size = fmt.fmt.pix.sizeimage;
    resp->plane_size[0] = fmt.fmt.pix.sizeimage;
    resp->plane_stride[0] = fmt.fmt.pix.bytesperline;
}

size_t CameraHandler::bufGetImageSize(domid_t domId)
{
    if (!mCamera) {
        return 0;
    }

    std::lock_guard<std::mutex> lock(mLock);

    v4l2_format fmt = mCamera->formatGet();

    return fmt.fmt.pix.sizeimage;
}

void CameraHandler::ctrlEnum(domid_t domId, const xencamera_req& aReq,
                             xencamera_resp& aResp,std::string name)
{
    if (!mCamera) {
        memset(&aResp, 0, sizeof aResp);
        return;
    }

    const xencamera_index *req = &aReq.req.index;
    xencamera_ctrl_enum_resp *resp = &aResp.resp.ctrl_enum;

    auto info = mCamera->controlEnum(name);

    resp->index = req->index;
    resp->type = V4L2ToXen::ctrlToXen(info.v4l2_cid);
    resp->flags = V4L2ToXen::ctrlFlagsToXen(info.flags);
    resp->min = info.minimum;
    resp->max = info.maximum;
    resp->step = info.step;
    resp->def_val = info.default_value;
}

void CameraHandler::ctrlSet(domid_t domId, const xencamera_req& aReq,
                            xencamera_resp& aResp, std::string name)
{
    if (!mCamera) {
        memset(&aResp, 0, sizeof aResp);
        return;
    }

    std::lock_guard<std::mutex> lock(mLock);

    /*
     * FIXME: for V4L2 frontends there could be a circular depependecy
     * here: when a frontend recievs "control changed" event it will
     * inject it into the V4L2 framework with v4l2_ctrl_s_ctrl,
     * which internally calls driver's s_ctrl callback. This callback
     * is implemented in a way that it sends "set control" request to
     * the backend, which is expected to send events to the rest
     * of the frontends.
     * Work this around by checking if this "set control" request
     * has control's value different from the current and only send
     * events if so.
     */
    auto curVal = mCamera->controlGetValue(name);

    DLOG(mLog, DEBUG) << "Handle command [SET CTRL] dom " <<
        std::to_string(domId) << " control " << name << " current: " <<
        std::to_string(curVal) << " requested: " <<
        std::to_string(aReq.req.ctrl_value.value);

    if (curVal == aReq.req.ctrl_value.value) {
        DLOG(mLog, DEBUG) << "Skip command [SET CTRL] dom " <<
            std::to_string(domId) << " control " << name << " current: " <<
            std::to_string(curVal) << " requested: " <<
            std::to_string(aReq.req.ctrl_value.value);
        return;
    }

    mCamera->controlSetValue(name, aReq.req.ctrl_value.value);

    /* Send ctrl change event to the rest of frontends, but current. */
    for (auto &listener : mListeners) {
        if (listener.first != domId)
            listener.second.control(name, aReq.req.ctrl_value.value);
    }
}

void CameraHandler::onFrameDoneCallback(int index, int size)
{
    if (!mCamera) {
        return;
    }

    auto data = mCamera->bufferGetData(index);

    DLOG(mLog, DEBUG) << "Frame " << std::to_string(index) <<
        " backend index " << std::to_string(index);

    for (auto &listener : mListeners)
        listener.second.frame(static_cast<uint8_t *>(data), size);
}

void CameraHandler::bufRequest(domid_t domId, const xencamera_req& aReq,
                               xencamera_resp& aResp)
{
    if (!mCamera) {
        memset(&aResp, 0, sizeof aResp);
        return;
    }

    std::lock_guard<std::mutex> lock(mLock);
    const xencamera_buf_request *req = &aReq.req.buf_request;
    xencamera_buf_request *resp = &aResp.resp.buf_request;

    DLOG(mLog, DEBUG) << "Handle command [BUF REQUEST] dom " <<
        std::to_string(domId) << " requested num_bufs " <<
        std::to_string(req->num_bufs);

    /*
     * If no buffers are allocated yet in the HW device (backend buffers)
     * then request buffers now.
     * This must not be less than max(frontend[i].max_buffers).
     */
    if (!mBuffersAllocated.size())
        /* TODO: use config for BE_CONFIG_NUM_BUFFERS. */
        mNumBuffersAllocated = mCamera->streamAlloc(BE_CONFIG_NUM_BUFFERS);

    if (req->num_bufs > mNumBuffersAllocated)
        resp->num_bufs = mNumBuffersAllocated;
    else
        resp->num_bufs = req->num_bufs;

    mBuffersAllocated.emplace(domId, resp->num_bufs);

    DLOG(mLog, DEBUG) << "Handle command [BUF REQUEST] allowed num_bufs " <<
        std::to_string(resp->num_bufs);
}

void CameraHandler::bufRelease(domid_t domId)
{
    if (!mCamera) {
        return;
    }

    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Frontend dom " << std::to_string(domId) <<
        " has released all buffers";

    mBuffersAllocated.erase(domId);
    if (!mBuffersAllocated.size())
        mCamera->streamRelease();
}

void CameraHandler::streamStart(domid_t domId, const xencamera_req& aReq,
                                xencamera_resp& aResp)
{
    if (!mCamera) {
        memset(&aResp, 0, sizeof(aResp));
        return;
    }

    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [STREAM START] dom " <<
        std::to_string(domId);

    if (!mStreamingNow.size())
        mCamera->streamStart(bind(&CameraHandler::onFrameDoneCallback,
                                  this, _1, _2));
    mStreamingNow.emplace(domId, true);
}

void CameraHandler::streamStop(domid_t domId, const xencamera_req& aReq,
                               xencamera_resp& aResp)
{
    if (!mCamera) {
        memset(&aResp, 0, sizeof(aResp));
        return;
    }

    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [STREAM STOP] dom " <<
        std::to_string(domId);

    mStreamingNow.erase(domId);
    if (!mStreamingNow.size())
        mCamera->streamStop();
}

void CameraHandler::release()
{
    if (mCamera) {
        mCamera->streamStop();
        mCamera->streamRelease();
    }
}

