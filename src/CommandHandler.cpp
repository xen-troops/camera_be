
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

void CommandHandler::configSet(const xencamera_req& aReq,
                               xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [CONFIG SET]";
}

void CommandHandler::configGet(const xencamera_req& aReq,
                               xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [CONFIG GET]";
}

void CommandHandler::bufGetLayout(const xencamera_req& aReq,
                                  xencamera_resp& aResp)
{
    DLOG(mLog, DEBUG) << "Handle command [BUF GET LAYOUT]";
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

