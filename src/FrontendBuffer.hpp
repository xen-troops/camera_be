/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */
#ifndef SRC_FRONTENDBUFFER_HPP_
#define SRC_FRONTENDBUFFER_HPP_

#include <memory>

#include <xen/be/Log.hpp>
#include <xen/be/XenGnttab.hpp>

#include <xen/io/cameraif.h>

class FrontendBuffer
{
public:
    FrontendBuffer(domid_t domId, size_t size, const xencamera_req& req);
    ~FrontendBuffer();

    int getIndex() {
        return mIndex;
    }

    void copyBuffer(void *data, size_t size);

private:
    XenBackend::Log mLog;
    std::mutex mLock;

    domid_t mDomId;
    int mIndex;
    unsigned long mOffset;

    std::unique_ptr<XenBackend::XenGnttabBuffer> mBuffer;

    void init(const xencamera_req& req, size_t size);
    void release();

    void getBufferRefs(grant_ref_t startDirectory, uint32_t size,
                       std::vector<grant_ref_t>& refs);
};

typedef std::unique_ptr<FrontendBuffer> FrontendBufferPtr;

#endif /* SRC_FRONTENDBUFFER_HPP_ */
