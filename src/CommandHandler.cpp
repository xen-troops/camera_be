
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

using namespace std::placeholders;

std::unordered_map<int, CommandHandler::CommandFn> CommandHandler::sCmdTable =
{
    { XENCAMERA_OP_CONFIG_SET,          &CommandHandler::configSet },
    { XENCAMERA_OP_CONFIG_GET,          &CommandHandler::configGet },
    { XENCAMERA_OP_CONFIG_VALIDATE,     &CommandHandler::configValidate },
    { XENCAMERA_OP_FRAME_RATE_SET,      &CommandHandler::frameRateSet },
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
                               CameraHandlerPtr cameraHandler) :
    RingBufferInBase<xen_cameraif_back_ring, xen_cameraif_sring,
                     xencamera_req, xencamera_resp>(domId, port, ref),
    mCommandHandler(domId, eventBuffer, ctrls, cameraHandler),
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

CommandHandler::CommandHandler(domid_t domId,
                               EventRingBufferPtr eventBuffer,
                               std::string ctrls,
                               CameraHandlerPtr cameraHandler) :
    mDomId(domId),
    mEventBuffer(eventBuffer),
	mEventId(0),
    mCameraHandler(cameraHandler),
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

    release();
}

void CommandHandler::init(std::string ctrls)
{
    std::stringstream ss(ctrls);
    std::string item;

    while (std::getline(ss, item, XENCAMERA_LIST_SEPARATOR[0])) {
        LOG(mLog, DEBUG) << "Assigned control: " << item;
        mControls.push_back(item);
    }

    mCameraHandler->listenerSet(mDomId,
        CameraHandler::Listeners {
            .frame = bind(&CommandHandler::onFrameDoneCallback,
                          this, _1, _2),
            .control = bind(&CommandHandler::onCtrlChangeCallback,
                            this, _1, _2),
        });
}

void CommandHandler::release()
{
    mCameraHandler->listenerReset(mDomId);
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

void CommandHandler::configSet(const xencamera_req& req,
                               xencamera_resp& resp)
{
    mCameraHandler->configSet(mDomId, req, resp);
}

void CommandHandler::configGet(const xencamera_req& req,
                               xencamera_resp& resp)
{
    mCameraHandler->configGet(mDomId, req, resp);
}

void CommandHandler::configValidate(const xencamera_req& req,
                                    xencamera_resp& resp)
{
    mCameraHandler->configValidate(mDomId, req, resp);
}

void CommandHandler::frameRateSet(const xencamera_req& req,
                                  xencamera_resp& resp)
{
    mCameraHandler->frameRateSet(mDomId, req, resp);
}

void CommandHandler::bufGetLayout(const xencamera_req& req,
                                  xencamera_resp& resp)
{
    mCameraHandler->bufGetLayout(mDomId, req, resp);
}

void CommandHandler::bufRequest(const xencamera_req& req,
                                xencamera_resp& resp)
{
    mCameraHandler->bufRequest(mDomId, req, resp);
}

void CommandHandler::bufCreate(const xencamera_req& req,
                               xencamera_resp& resp)
{
    const xencamera_buf_create_req *create = &req.req.buf_create;

    DLOG(mLog, DEBUG) << "Handle command [BUF CREATE] dom " <<
        std::to_string(mDomId) << " index " <<
        std::to_string(create->index) << " offset " <<
        std::to_string(create->plane_offset[0]);

    size_t imageSize = mCameraHandler->bufGetImageSize(mDomId);

    mBuffers[create->index] = FrontendBufferPtr(new FrontendBuffer(mDomId,
                                                                   imageSize,
                                                                   req));
}

void CommandHandler::bufDestroy(const xencamera_req& req,
                                xencamera_resp& resp)
{
    size_t index = static_cast<size_t>(req.req.index.index);

    DLOG(mLog, DEBUG) << "Handle command [BUF DESTROY] dom " <<
        std::to_string(mDomId) << " index " << std::to_string(index);

    mBuffers.erase(index);
    /*
     * If this was the last buffer then tell the CameraHandler it might
     * release the buffers.
     */
    if (!mBuffers.size())
            mCameraHandler->bufRelease(mDomId);
}

void CommandHandler::bufQueue(const xencamera_req& req,
                              xencamera_resp& resp)
{
    std::lock_guard<std::mutex> lock(mLock);
    size_t index = static_cast<size_t>(req.req.index.index);

    DLOG(mLog, DEBUG) << "Handle command [BUF QUEUE] dom " <<
        std::to_string(mDomId) << " index " << std::to_string(index);

    mQueuedBuffers.push_back(index);
}

void CommandHandler::bufDequeue(const xencamera_req& req,
                                xencamera_resp& resp)
{
    std::lock_guard<std::mutex> lock(mLock);
    size_t index = static_cast<size_t>(req.req.index.index);

    DLOG(mLog, DEBUG) << "Handle command [BUF DEQUEUE] dom " <<
        std::to_string(mDomId) << " index " << std::to_string(index);

    mQueuedBuffers.remove(index);
}

void CommandHandler::onFrameDoneCallback(uint8_t *data, size_t size)
{
    std::lock_guard<std::mutex> lock(mLock);
    int index;

    if (mQueuedBuffers.empty())
        return;

    index = mQueuedBuffers.front();

    DLOG(mLog, DEBUG) << "Send event [FRAME] dom " <<
        std::to_string(mDomId) << " index " << std::to_string(index);

    xencamera_evt event {0};

    event.type = XENCAMERA_EVT_FRAME_AVAIL;
    event.evt.frame_avail.index = index;
    event.evt.frame_avail.used_sz = size;
    event.evt.frame_avail.seq_num = mSequence++;
    event.id = mEventId++;

    mBuffers[index]->copyBuffer(data, size);

    mEventBuffer->sendEvent(event);
}

void CommandHandler::ctrlEnum(const xencamera_req& req,
                              xencamera_resp& resp)
{
    size_t index = static_cast<size_t>(req.req.index.index);

    DLOG(mLog, DEBUG) << "Handle command [CTRL ENUM] dom " <<
        std::to_string(mDomId);

    /*
     * The index of the control we have in the request is frontend
     * related, e.g. it is in the range of the assigned controls to
     * this domain. Thus, we have to control control's index to be within
     * the assigned range.
     */
    if (index >= mControls.size())
        throw XenBackend::Exception("No more assigned controls", EINVAL);

    mCameraHandler->ctrlEnum(mDomId, req, resp, mControls[index]);
}

void CommandHandler::ctrlSet(const xencamera_req& req,
                             xencamera_resp& resp)
{
    int type = req.req.ctrl_value.type;

    DLOG(mLog, DEBUG) << "Handle command [SET CTRL] dom " <<
        std::to_string(mDomId);

    auto ctrlName = V4L2ToXen::ctrlGetNameXen(type);

    if (std::find(mControls.begin(), mControls.end(), ctrlName) ==
        mControls.end())
        throw XenBackend::Exception("Wrong control type " +
                                    std::to_string(type), EINVAL);

    mCameraHandler->ctrlSet(mDomId, req, resp, ctrlName);
}

void CommandHandler::ctrlGet(const xencamera_req& req,
                             xencamera_resp& resp)
{
    int type = req.req.get_ctrl.type;

    DLOG(mLog, DEBUG) << "Handle command [GET CTRL] dom " <<
        std::to_string(mDomId);

    auto ctrlName = V4L2ToXen::ctrlGetNameXen(type);

    if (std::find(mControls.begin(), mControls.end(), ctrlName) ==
        mControls.end())
        throw XenBackend::Exception("Wrong control type " +
                                    std::to_string(type), EINVAL);
}

void CommandHandler::streamStart(const xencamera_req& req,
                                 xencamera_resp& resp)
{
    mSequence = 0;
    mCameraHandler->streamStart(mDomId, req, resp);
}

void CommandHandler::streamStop(const xencamera_req& req,
                                xencamera_resp& resp)
{
    mCameraHandler->streamStop(mDomId, req, resp);
}

void CommandHandler::onCtrlChangeCallback(const std::string name, int64_t value)
{
    auto ctrl = std::find(mControls.begin(), mControls.end(), name);

    if (ctrl == mControls.end()) {
        DLOG(mLog, DEBUG) << "Not supported control for change event, skipping";
        return;
    }

    DLOG(mLog, DEBUG) << "Send event [CTRL] dom " <<
        std::to_string(mDomId);

    xencamera_evt event {0};

    event.type = XENCAMERA_EVT_CTRL_CHANGE;
    event.evt.ctrl_value.type = V4L2ToXen::ctrlGetTypeXen(name);
    event.evt.ctrl_value.value = value;
    event.id = mEventId++;

    mEventBuffer->sendEvent(event);
}

