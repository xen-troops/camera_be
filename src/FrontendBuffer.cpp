// SPDX-License-Identifier: GPL-2.0

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */

#include <xen/be/Exception.hpp>

#include "FrontendBuffer.hpp"

using XenBackend::Exception;

FrontendBuffer::FrontendBuffer(domid_t domId, size_t size,
                               const xencamera_req& req) :
    mLog("FrontendBuffer"),
    mDomId(domId)
{
    LOG(mLog, DEBUG) << "Create camera buffer, domId " << std::to_string(domId);

    try {
        init(req, size);
    } catch (...) {
        release();
        throw;
    }
}

FrontendBuffer::~FrontendBuffer()
{
    release();
}

void FrontendBuffer::init(const xencamera_req& req, size_t size)
{
    const xencamera_buf_create_req& aReq = req.req.buf_create;
    std::vector<grant_ref_t> refs;

    mIndex = aReq.index;
    mOffset = aReq.plane_offset[0];

    /* Real size of the buffer will be bigger if there is offset. */
    size += mOffset;

    getBufferRefs(aReq.gref_directory, size, refs);

    mBuffer.reset(new XenBackend::XenGnttabBuffer(mDomId, refs.data(),
                                                  refs.size(),
                                                  PROT_READ | PROT_WRITE));
}

void FrontendBuffer::release()
{
    DLOG(mLog, DEBUG) << "Release buffer " << mIndex;
}

void FrontendBuffer::getBufferRefs(grant_ref_t startDirectory, uint32_t size,
                                   std::vector<grant_ref_t>& refs)
{
    refs.clear();

    size_t requestedNumGrefs = (size + XC_PAGE_SIZE - 1) / XC_PAGE_SIZE;

    DLOG(mLog, DEBUG) << "Get buffer refs, directory: " << startDirectory
        << ", size: " << size
        << ", in grefs: " << requestedNumGrefs;

    while(startDirectory != 0)
    {

        XenBackend::XenGnttabBuffer pageBuffer(mDomId, startDirectory);

        xencamera_page_directory* pageDirectory =
            static_cast<xencamera_page_directory*>(pageBuffer.get());

        size_t numGrefs = std::min(requestedNumGrefs,
            (XC_PAGE_SIZE - offsetof(xencamera_page_directory, gref)) /
                sizeof(uint32_t));

        DLOG(mLog, DEBUG) << "Gref address: " << pageDirectory->gref
            << ", numGrefs " << numGrefs;

        refs.insert(refs.end(), pageDirectory->gref,
                    pageDirectory->gref + numGrefs);

        requestedNumGrefs -= numGrefs;

        startDirectory = pageDirectory->gref_dir_next_page;
    }

    DLOG(mLog, DEBUG) << "Get buffer refs, num refs: " << refs.size();
}

void FrontendBuffer::copyBuffer(void *data, size_t size)
{
    DLOG(mLog, DEBUG) << "Copy, size: " << size;

    memcpy(static_cast<uint8_t *>(mBuffer->get()) + mOffset, data, size);
}

