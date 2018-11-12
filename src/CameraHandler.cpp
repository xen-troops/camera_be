
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
    mCamera.reset(new Camera(uniqueId));
}

void CameraHandler::release()
{
}

void CameraHandler::configToXen(xencamera_config *cfg)
{
    v4l2_format fmt = mCamera->formatGet();

    cfg->pixel_format = fmt.fmt.pix.pixelformat;
    cfg->width = fmt.fmt.pix.width;
    cfg->height = fmt.fmt.pix.height;

    cfg->colorspace = V4L2ToXen::colorspaceToXen(fmt.fmt.pix.colorspace);

    cfg->xfer_func = V4L2ToXen::xferToXen(fmt.fmt.pix.xfer_func);

    cfg->ycbcr_enc = V4L2ToXen::ycbcrToXen(fmt.fmt.pix.ycbcr_enc);

    cfg->quantization = V4L2ToXen::quantizationToXen(fmt.fmt.pix.quantization);

    /* TODO: This needs to be properly handled. */
    cfg->displ_asp_ratio_numer = 1;
    cfg->displ_asp_ratio_denom = 1;

    v4l2_fract frameRate = mCamera->frameRateGet();

    cfg->frame_rate_numer = frameRate.numerator;
    cfg->frame_rate_denom = frameRate.denominator;
}

void CameraHandler::configSet(const xencamera_req& aReq,
                              xencamera_resp& aResp)
{
    const xencamera_config *req = &aReq.req.config;

    DLOG(mLog, DEBUG) << "Handle command [CONFIG SET]";

    v4l2_format fmt {0};

    fmt.fmt.pix.pixelformat = req->pixel_format;
    fmt.fmt.pix.width = req->width;
    fmt.fmt.pix.height = req->height;

    fmt.fmt.pix.colorspace = V4L2ToXen::colorspaceToV4L2(req->colorspace);

    fmt.fmt.pix.xfer_func = V4L2ToXen::xferToV4L2(req->xfer_func);

    fmt.fmt.pix.ycbcr_enc = V4L2ToXen::ycbcrToV4L2(req->ycbcr_enc);

    fmt.fmt.pix.quantization = V4L2ToXen::quantizationToV4L2(req->quantization);

    mCamera->formatSet(fmt);

    mCamera->frameRateSet(req->frame_rate_numer, req->frame_rate_denom);

    configToXen(&aResp.resp.config);
}

void CameraHandler::configGet(const xencamera_req& aReq,
                              xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [CONFIG GET]";

    configToXen(&aResp.resp.config);
}

void CameraHandler::bufGetLayout(const xencamera_req& aReq,
                                 xencamera_resp& aResp)
{
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
    const xencamera_buf_request *req = &aReq.req.buf_request;
    xencamera_buf_request *resp = &aResp.resp.buf_request;

    DLOG(mLog, DEBUG) << "Handle command [BUF REQUEST] domId " <<
        std::to_string(domId);

    resp->num_bufs = mCamera->bufferRequest(req->num_bufs);

    DLOG(mLog, DEBUG) << "Handle command [BUF REQUEST] num_bufs " <<
        std::to_string(resp->num_bufs);
}

void CameraHandler::bufCreate(const xencamera_req& aReq,
                              xencamera_resp& aResp, domid_t domId)
{
    DLOG(mLog, DEBUG) << "Handle command [BUF CREATE] domId " <<
        std::to_string(domId);

    v4l2_format fmt = mCamera->formatGet();

    mBuffers.emplace(domId, new FrontendBuffer(domId, fmt.fmt.pix.sizeimage, aReq));
}

void CameraHandler::bufDestroy(const xencamera_req& aReq,
                               xencamera_resp& aResp, domid_t domId)
{
    const xencamera_index *req = &aReq.req.index;

    DLOG(mLog, DEBUG) << "Handle command [BUF DESTROY] domId " <<
        std::to_string(domId) << " index " << std::to_string(req->index);

    for (auto it = mBuffers.begin(); it != mBuffers.end(); ++it) {
        if (it->first != domId)
            continue;

        if (it->second->getIndex() == req->index) {
            mBuffers.erase(it);
            break;
        }
    }
}

void CameraHandler::bufQueue(const xencamera_req& aReq,
                             xencamera_resp& aResp, domid_t domId)
{
    DLOG(mLog, DEBUG) << "Handle command [BUF QUEUE] domId " <<
        std::to_string(domId);
}

void CameraHandler::bufDequeue(const xencamera_req& aReq,
                               xencamera_resp& aResp, domid_t domId)
{
    DLOG(mLog, DEBUG) << "Handle command [BUF DEQUEUE] domId " <<
        std::to_string(domId);
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
    DLOG(mLog, DEBUG) << "Handle command [SET CTRL]";
}

void CameraHandler::ctrlGet(const xencamera_req& aReq,
                            xencamera_resp& aResp,
                            std::string name)
{
    DLOG(mLog, DEBUG) << "Handle command [GET CTRL]";
}

void CameraHandler::streamStart(const xencamera_req& aReq,
                                xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [STREAM START]";
}

void CameraHandler::streamStop(const xencamera_req& aReq,
                               xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [STREAM STOP]";
}

