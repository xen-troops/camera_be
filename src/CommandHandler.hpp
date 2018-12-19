/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */
#ifndef SRC_COMMANDHANDLER_HPP_
#define SRC_COMMANDHANDLER_HPP_

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <xen/be/RingBufferBase.hpp>
#include <xen/be/Log.hpp>

#include <xen/io/cameraif.h>

#include "CameraHandler.hpp"

class EventRingBuffer : public XenBackend::RingBufferOutBase<
                        xencamera_event_page, xencamera_evt>
{
public:
    EventRingBuffer(domid_t domId, evtchn_port_t port,
                    grant_ref_t ref, int offset, size_t size);

private:
    XenBackend::Log mLog;
};

typedef std::shared_ptr<EventRingBuffer> EventRingBufferPtr;

class CommandHandler
{
public:
    CommandHandler(domid_t domId, EventRingBufferPtr eventBuffer,
                   std::string ctrls, CameraHandlerPtr cameraHandler);
    ~CommandHandler();

    int processCommand(const xencamera_req& req, xencamera_resp& resp);

private:
    typedef void(CommandHandler::*CommandFn)(const xencamera_req& aReq,
                                             xencamera_resp& aResp);

    static std::unordered_map<int, CommandFn> sCmdTable;

    domid_t mDomId;

    EventRingBufferPtr mEventBuffer;

    int mEventId;

    CameraHandlerPtr mCameraHandler;

    XenBackend::Log mLog;
    std::mutex mLock;

    std::vector<std::string> mControls;
    std::unordered_map<int, FrontendBufferPtr> mBuffers;

    /*
     * Buffer management
     * 1. Frontend sends queue event: add the buffer to the queued list end
     * 2. onFrame callback:
     * 2.1. If there are buffers in the queued list then fill the first buffer
     * from the queued list
     * 2.2. If there are no buffers in the queued list, then do nothing
     * 3. Frontend sends dequeue event: remove the buffer from the queued list
     */
    std::list<int> mQueuedBuffers;

    uint32_t mSequence;

    void init(std::string ctrls);
    void release();

    void configSet(const xencamera_req& aReq, xencamera_resp& aResp);
    void configGet(const xencamera_req& aReq, xencamera_resp& aResp);
    void configValidate(const xencamera_req& aReq, xencamera_resp& aResp);

    void frameRateSet(const xencamera_req& aReq, xencamera_resp& aResp);

    void bufGetLayout(const xencamera_req& aReq, xencamera_resp& aResp);
    void bufRequest(const xencamera_req& aReq, xencamera_resp& aResp);
    void bufCreate(const xencamera_req& aReq, xencamera_resp& aResp);
    void bufDestroy(const xencamera_req& aReq, xencamera_resp& aResp);
    void bufQueue(const xencamera_req& aReq, xencamera_resp& aResp);
    void bufDequeue(const xencamera_req& aReq, xencamera_resp& aResp);

    void ctrlEnum(const xencamera_req& aReq, xencamera_resp& aResp);
    void ctrlSet(const xencamera_req& aReq, xencamera_resp& aResp);
    void ctrlGet(const xencamera_req& aReq, xencamera_resp& aResp);

    void streamStart(const xencamera_req& aReq, xencamera_resp& aResp);
    void streamStop(const xencamera_req& aReq, xencamera_resp& aResp);

    void onFrameDoneCallback(uint8_t *data, size_t size);
    void onCtrlChangeCallback(const std::string name, int64_t value);
};

/***************************************************************************//**
 * Ring buffer used for the camera control.
 ******************************************************************************/
class CtrlRingBuffer : public XenBackend::RingBufferInBase<xen_cameraif_back_ring,
    xen_cameraif_sring, xencamera_req, xencamera_resp>
{
public:
    CtrlRingBuffer(EventRingBufferPtr eventBuffer, domid_t domId,
                   evtchn_port_t port, grant_ref_t ref,
                   std::string ctrls, CameraHandlerPtr cameraHandler);

private:
    CommandHandler mCommandHandler;

    XenBackend::Log mLog;

    virtual void processRequest(const xencamera_req& req) override;
};

typedef std::shared_ptr<CtrlRingBuffer> CtrlRingBufferPtr;

#endif /* SRC_COMMANDHANDLER_HPP_ */
