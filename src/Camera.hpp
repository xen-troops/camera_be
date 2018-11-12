/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */
#ifndef SRC_CAMERA_HPP_
#define SRC_CAMERA_HPP_

#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <linux/videodev2.h>

#include <xen/be/Log.hpp>
#include <xen/be/Utils.hpp>

class Camera
{
public:
    Camera(const std::string devName);
    ~Camera();

    const std::string getDevPath() const {
        return mDevPath;
    }

    const std::string getUniqueId() const {
        return mUniqueId;
    }

    /* Buffer related functionlity. */
    int bufferRequest(int numBuffers);
    v4l2_buffer bufferQuery(int index);
    void bufferQueue(int index);
    v4l2_buffer bufferDequeue();
    int bufferGetMin();
    int bufferExport(int index);
    virtual void *bufferGetData(int index);

    /* Stream related functionlity. */
    typedef std::function<int(int, int)> FrameDoneCallback;

    virtual void streamAlloc(int numBuffers, uint32_t width,
                             uint32_t height, uint32_t pixelFormat);
    virtual void streamRelease();
    virtual void streamStart(FrameDoneCallback clb);
    virtual void streamStop();

    /* Format related functionality. */
    void formatSet(uint32_t width, uint32_t height, uint32_t pixelFormat);
    void formatSet(v4l2_format fmt);
    v4l2_format formatGet();

    /* Frame rate related functionality. */
    void frameRateSet(int num, int denom);
    v4l2_fract frameRateGet();

    /* Control related functionality. */
    struct ControlInfo {
        int v4l2_cid;
        int flags;
        signed int minimum;
        signed int maximum;
        signed int default_value;
        signed int step;
    };

    ControlInfo controlEnum(std::string name);
    void controlSetValue(std::string name, signed int value);
    void controlGetValue(std::string name, signed int *value);

protected:
    XenBackend::Log mLog;
    std::mutex mLock;

    const std::string mUniqueId;
    const std::string mDevPath;
    int mFd;

    static const v4l2_buf_type cV4L2BufType = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_memory cMemoryType = V4L2_MEMORY_MMAP;

    bool mStreamStarted;

    std::vector<std::string> mVideoNodes;

    std::thread mThread;

    std::unique_ptr<XenBackend::PollFd> mPollFd;

    FrameDoneCallback mFrameDoneCallback;

    void init();
    void release();

    int xioctl(int request, void *arg);

    bool isOpen();
    void open();
    void close();
    bool isCaptureDevice();

    /* Format related functionality. */
    struct FormatSize {
        int width;
        int height;
        std::vector<v4l2_fract> fps;
    };

    struct Format {
        uint32_t pixelFormat;
        std::string description;

        std::vector<FormatSize> size;
    };

    std::vector<Format> mFormats;

    void formatEnumerate();

    /* Frame size related functionality. */
    int frameSizeGet(int index, uint32_t pixelFormat,
                     v4l2_frmsizeenum &size);

    int frameIntervalGet(int index, uint32_t pixelFormat,
                         uint32_t width, uint32_t height,
                         v4l2_frmivalenum &interval);

    static float toFps(const v4l2_fract &fract) {
        return static_cast<float>(fract.denominator) / fract.numerator;
    }

    std::vector<ControlInfo> mControls;

    void controlEnumerate();
    void controlGetValue(int v4l2_cid, signed int *value);

    void eventThread();
};

typedef std::shared_ptr<Camera> CameraPtr;

#endif /* SRC_CAMERA_HPP_ */
