
// SPDX-License-Identifier: GPL-2.0

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
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
        release();
        throw;
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
    mBuffersAllocated = false;
    mStreamingNow.clear();
    mCamera.reset(new Camera(uniqueId));
}

void CameraHandler::release()
{
    mCamera->streamRelease();
}

void CameraHandler::configToXen(xencamera_config_resp *cfg_resp)
{
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
    const xencamera_config_req *cfg_req = &aReq.req.config;

    v4l2_format fmt {0};

    fmt.fmt.pix.pixelformat = cfg_req->pixel_format;
    fmt.fmt.pix.width = cfg_req->width;
    fmt.fmt.pix.height = cfg_req->height;

    fmt.fmt.pix.colorspace = V4L2ToXen::colorspaceToV4L2(cfg_req->colorspace);

    fmt.fmt.pix.xfer_func = V4L2ToXen::xferToV4L2(cfg_req->xfer_func);

    fmt.fmt.pix.ycbcr_enc = V4L2ToXen::ycbcrToV4L2(cfg_req->ycbcr_enc);

    fmt.fmt.pix.quantization = V4L2ToXen::quantizationToV4L2(cfg_req->quantization);

    if (is_set)
        mCamera->formatSet(fmt);
    else
        mCamera->formatTry(fmt);

    configToXen(&aResp.resp.config);
}

void CameraHandler::configSet(const xencamera_req& aReq,
                              xencamera_resp& aResp)
{
    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [CONFIG SET]";

    if (mFormatSet) {
        configToXen(&aResp.resp.config);
    } else {
        configSetTry(aReq, aResp, true);
        mFormatSet = true;
    }
}

void CameraHandler::configValidate(const xencamera_req& aReq,
                                   xencamera_resp& aResp)
{
    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [CONFIG VALIDATE]";

    if (mFormatSet)
        configToXen(&aResp.resp.config);
    else
        configSetTry(aReq, aResp, false);
}

void CameraHandler::configGet(const xencamera_req& aReq,
                              xencamera_resp& aResp)
{
    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [CONFIG GET]";

    configToXen(&aResp.resp.config);
}

void CameraHandler::frameRateSet(const xencamera_req& aReq,
                                 xencamera_resp& aResp)
{
    std::lock_guard<std::mutex> lock(mLock);
    const xencamera_frame_rate_req *req = &aReq.req.frame_rate;

    DLOG(mLog, DEBUG) << "Handle command [FRAME RATE SET]";

    if (mFramerateSet) {
    } else {
        mCamera->frameRateSet(req->frame_rate_numer, req->frame_rate_denom);
        mFramerateSet = true;
    }
}

void CameraHandler::bufGetLayout(const xencamera_req& aReq,
                                 xencamera_resp& aResp)
{
    std::lock_guard<std::mutex> lock(mLock);
    xencamera_buf_get_layout_resp *resp = &aResp.resp.buf_layout;

    DLOG(mLog, DEBUG) << "Handle command [BUF GET LAYOUT]";

    v4l2_format fmt = mCamera->formatGet();

    DLOG(mLog, DEBUG) << "Handle command [BUF GET LAYOUT] size " <<
        fmt.fmt.pix.sizeimage;

    /* XXX: Single plane only. */
    resp->num_planes = 1;
    resp->size = fmt.fmt.pix.sizeimage;
    resp->plane_size[0] = fmt.fmt.pix.sizeimage;
    resp->plane_stride[0] = fmt.fmt.pix.bytesperline;
}

void CameraHandler::bufRequest(const xencamera_req& aReq,
                               xencamera_resp& aResp, domid_t domId)
{
    std::lock_guard<std::mutex> lock(mLock);
    const xencamera_buf_request *req = &aReq.req.buf_request;
    xencamera_buf_request *resp = &aResp.resp.buf_request;

    DLOG(mLog, DEBUG) << "Handle command [BUF REQUEST] domId " <<
        std::to_string(domId);

    if (!mBuffersAllocated) {
        mCamera->streamRelease();
        mNumBuffersAllocated = mCamera->streamAlloc(BE_CONFIG_NUM_BUFFERS);
        mBuffersAllocated = true;
    }

    if (req->num_bufs > mNumBuffersAllocated)
        resp->num_bufs = mNumBuffersAllocated;
    else
        resp->num_bufs = req->num_bufs;

    DLOG(mLog, DEBUG) << "Handle command [BUF REQUEST] num_bufs " <<
        std::to_string(resp->num_bufs);
}

size_t CameraHandler::bufGetImageSize(domid_t domId)
{
    std::lock_guard<std::mutex> lock(mLock);

    v4l2_format fmt = mCamera->formatGet();

    return fmt.fmt.pix.sizeimage;
}

void CameraHandler::ctrlEnum(const xencamera_req& aReq,
                             xencamera_resp& aResp,
                             std::string name)
{
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

void CameraHandler::ctrlSet(const xencamera_req& aReq,
                            xencamera_resp& aResp,
                            std::string name)
{
    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [SET CTRL]";

    /* TODO: send ctrl change event to the rest of frontends. */
}

void CameraHandler::ctrlGet(const xencamera_req& aReq,
                            xencamera_resp& aResp,
                            std::string name)
{
    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [GET CTRL]";
}

void CameraHandler::streamStart(domid_t domId, const xencamera_req& aReq,
                                xencamera_resp& aResp)
{
    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [STREAM START]";

    if (!mStreamingNow.size())
        mCamera->streamStart(bind(&CameraHandler::onFrameDoneCallback,
                                  this, _1, _2));
    mStreamingNow.emplace(domId, true);
}

void CameraHandler::streamStop(domid_t domId, const xencamera_req& aReq,
                               xencamera_resp& aResp)
{
    std::lock_guard<std::mutex> lock(mLock);

    DLOG(mLog, DEBUG) << "Handle command [STREAM STOP]";

    mStreamingNow.erase(domId);
    if (!mStreamingNow.size())
        mCamera->streamStop();
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

int CameraHandler::onFrameDoneCallback(int index, int size)
{
    DLOG(mLog, DEBUG) << "Frame " << std::to_string(index);

    auto data = mCamera->bufferGetData(index);

    for (auto &listener : mListeners)
        listener.second.frame(index, static_cast<uint8_t *>(data), size);

    return index;
}

