
// SPDX-License-Identifier: GPL-2.0

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */

#include <algorithm>
#include <iomanip>

#include <xen/be/Exception.hpp>

#include "CommandHandler.hpp"
#include "V4L2ToXen.hpp"

std::unordered_map<int, CommandHandler::CommandFn> CommandHandler::sCmdTable =
{
    { XENCAMERA_OP_CONFIG_SET,          &CommandHandler::configSet },
    { XENCAMERA_OP_CONFIG_GET,          &CommandHandler::configGet },
    { XENCAMERA_OP_BUF_GET_LAYOUT,      &CommandHandler::bufGetLayout },
    { XENCAMERA_OP_BUF_REQUEST,         &CommandHandler::bufRequest },
    { XENCAMERA_OP_BUF_CREATE,          &CommandHandler::bufCreate },
    { XENCAMERA_OP_BUF_DESTROY,         &CommandHandler::bufDestroy },
    { XENCAMERA_OP_BUF_QUEUE,           &CommandHandler::bufQueue },
    { XENCAMERA_OP_BUF_DEQUEUE,         &CommandHandler::bufDequeue },
    { XENCAMERA_OP_CTRL_ENUM,           &CommandHandler::ctrlEnum },
    { XENCAMERA_OP_CTRL_SET,            &CommandHandler::ctrlSet },
    { XENCAMERA_OP_CTRL_GET,            &CommandHandler::ctrlGet },
    { XENCAMERA_OP_STREAM_START,        &CommandHandler::streamStart },
    { XENCAMERA_OP_STREAM_STOP,         &CommandHandler::streamStop },
};

CtrlRingBuffer::CtrlRingBuffer(EventRingBufferPtr eventBuffer,
                               domid_t domId, evtchn_port_t port,
                               grant_ref_t ref,
                               std::string ctrls,
                               CameraPtr camera) :
    RingBufferInBase<xen_cameraif_back_ring, xen_cameraif_sring,
                     xencamera_req, xencamera_resp>(domId, port, ref),
    mCommandHandler(eventBuffer, ctrls, camera),
    mLog("CamCtrlRing")
{
    LOG(mLog, DEBUG) << "Create ctrl ring buffer";
}

void CtrlRingBuffer::processRequest(const xencamera_req& req)
{
    DLOG(mLog, DEBUG) << "Request received, cmd:"
        << static_cast<int>(req.operation);

    xencamera_resp rsp {0};

    rsp.id = req.id;
    rsp.operation = req.operation;

    rsp.status = mCommandHandler.processCommand(req, rsp);

    sendResponse(rsp);
}

EventRingBuffer::EventRingBuffer(domid_t domId, evtchn_port_t port,
                                 grant_ref_t ref, int offset, size_t size) :
    RingBufferOutBase<xencamera_event_page, xencamera_evt>(domId, port, ref,
                                                           offset, size),
    mLog("CamEventRing")
{
    LOG(mLog, DEBUG) << "Create event ring buffer";
}

CommandHandler::CommandHandler(EventRingBufferPtr eventBuffer,
                               std::string ctrls,
                               CameraPtr camera) :
    mEventBuffer(eventBuffer),
    mCamera(camera),
    mLog("CommandHandler")
{
    LOG(mLog, DEBUG) << "Create command handler";

    try {
        init(ctrls);
    } catch (...) {
        release();
        throw;
    }
}

CommandHandler::~CommandHandler()
{
    LOG(mLog, DEBUG) << "Delete command handler";
}

void CommandHandler::init(std::string ctrls)
{
    std::stringstream ss(ctrls);
    std::string item;

    while (std::getline(ss, item, XENCAMERA_LIST_SEPARATOR[0]))
        mCameraControls.push_back(CameraControl {
                                  .name = item,
                                  .v4l2_cid = -1});
}

void CommandHandler::release()
{
}

int CommandHandler::processCommand(const xencamera_req& req,
                                   xencamera_resp& resp)
{
    int status = 0;

    try
    {
        (this->*sCmdTable.at(req.operation))(req, resp);
    }
    catch(const XenBackend::Exception& e)
    {
        LOG(mLog, ERROR) << e.what();

        status = -e.getErrno();

        if (status >= 0)
        {
            DLOG(mLog, WARNING) << "Positive error code: "
                << static_cast<signed int>(status);

            status = -EINVAL;
        }
    }
    catch(const std::out_of_range& e)
    {
        LOG(mLog, ERROR) << e.what();

        status = -ENOTSUP;
    }
    catch(const std::exception& e)
    {
        LOG(mLog, ERROR) << e.what();

        status = -EIO;
    }

    DLOG(mLog, DEBUG) << "Return status: ["
        << static_cast<signed int>(status) << "]";

    return status;
}

void CommandHandler::configToXen(xencamera_config *cfg)
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

void CommandHandler::configSet(const xencamera_req& aReq,
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

void CommandHandler::configGet(const xencamera_req& aReq,
                               xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [CONFIG GET]";

    configToXen(&aResp.resp.config);
}

void CommandHandler::bufGetLayout(const xencamera_req& aReq,
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

void CommandHandler::bufRequest(const xencamera_req& aReq,
                                xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [BUF aReqUEST]";
}

void CommandHandler::bufCreate(const xencamera_req& aReq,
                               xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [BUF CREATE]";
}

void CommandHandler::bufDestroy(const xencamera_req& aReq,
                                xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [BUF DESTROY]";
}

void CommandHandler::bufQueue(const xencamera_req& aReq,
                              xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [BUF QUEUE]";
}

void CommandHandler::bufDequeue(const xencamera_req& aReq,
                                xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [BUF DEQUEUE] front";
}

void CommandHandler::ctrlEnum(const xencamera_req& aReq,
                              xencamera_resp& aResp)
{
    const xencamera_index *req = &aReq.req.index;
    xencamera_ctrl_enum_resp *resp = &aResp.resp.ctrl_enum;

    DLOG(mLog, DEBUG) << "Handle command [CTRL ENUM]";

    if (req->index >= mCameraControls.size())
        throw XenBackend::Exception("No more assigned controls", EINVAL);

    auto info = mCamera->controlGetInfo(mCameraControls[req->index].name);

    mCameraControls[req->index].v4l2_cid = info.v4l2_cid;
    resp->index = req->index;
    resp->type = V4L2ToXen::ctrlToXen(info.v4l2_cid);
    resp->flags = V4L2ToXen::ctrlFlagsToXen(info.flags);
    resp->min = info.minimum;
    resp->max = info.maximum;
    resp->step = info.step;
    resp->def_val = info.default_value;
}

void CommandHandler::ctrlSet(const xencamera_req& aReq,
                             xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [SET CTRL]";
}

void CommandHandler::ctrlGet(const xencamera_req& aReq,
                             xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [GET CTRL]";
}

void CommandHandler::streamStart(const xencamera_req& aReq,
                                 xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [STREAM START]";
}

void CommandHandler::streamStop(const xencamera_req& aReq,
                                xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [STREAM STOP]";
}

