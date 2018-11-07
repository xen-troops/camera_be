// SPDX-License-Identifier: GPL-2.0

/*
 * Xen para-virtualized camera backend
 *
 * Based on:
 *   https://linuxtv.org/downloads/v4l-dvb-apis/uapi/v4l/capture.c.html
 *   https://git.ffmpeg.org/ffmpeg.git
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/stat.h>

#include "Camera.hpp"

#include <xen/be/Exception.hpp>
#include <xen/io/cameraif.h>

using XenBackend::Exception;
using XenBackend::PollFd;

Camera::Camera(const std::string devName):
    mLog("Camera"),
    mUniqueId(devName),
    mDevPath("/dev/" + devName),
    mFd(-1),
    mMemoryType(V4L2_MEMORY_MMAP),
    mStreamStarted(false),
    mFrameDoneCallback(nullptr)
{
    try {
        init();
    } catch (...) {
        release();
        throw;
    }
}

Camera::~Camera()
{
    streamStop();
    release();
}

void Camera::init()
{
    LOG(mLog, DEBUG) << "Initializing camera device " << mDevPath;

    open();
    if (!isCaptureDevice())
        throw Exception(mDevPath + " is not a camera device", ENOTTY);

    formatEnumerate();
    controlEnumerate();

    mPollFd.reset(new PollFd(mFd, POLLIN));
}

void Camera::release()
{
    LOG(mLog, DEBUG) << "Deleting camera device " << mDevPath;

    mPollFd.reset();
    close();
}

bool Camera::isOpen()
{
    return mFd >= 0;
}

void Camera::open()
{
    struct stat st;

    if (isOpen())
        return;

    if (stat(mDevPath.c_str(), &st) < 0)
        throw Exception("Cannot stat " + mDevPath + " video device: " +
                        strerror(errno), errno);

    if (!S_ISCHR(st.st_mode))
        throw Exception(mDevPath + " is not a character device", EINVAL);

    int fd = ::open(mDevPath.c_str(), O_RDWR /* required */ | O_NONBLOCK, 0);

    if (fd < 0)
        throw Exception("Cannot open " + mDevPath + " video device: " +
                        strerror(errno), errno);

    mFd = fd;
}

int Camera::xioctl(int request, void *arg)
{
    int ret;

    if (!isOpen()) {
        errno = EINVAL;
        return -1;
    }

    do {
        ret = ioctl(mFd, request, arg);
    } while (ret == -1 && errno == EINTR);

    return ret;
}

bool Camera::isCaptureDevice()
{
    struct v4l2_capability cap = {0};

    if (xioctl(VIDIOC_QUERYCAP, &cap) < 0) {
        if (EINVAL == errno) {
            LOG(mLog, DEBUG) << mDevPath << " is not a V4L2 device";
            return false;
        } else {
            LOG(mLog, ERROR) <<
                "Failed to call [VIDIOC_QUERYCAP] for device " <<
                mDevPath;
            return false;
        }
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        LOG(mLog, DEBUG) << mDevPath << " is not a video capture device";
        return false;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        LOG(mLog, DEBUG) << mDevPath << " does not support streaming IO";
        return false;
    }

    /*
     * FIXME: skip all devices which report zero width/height.
     * This can, for example, be for capture devices which have no
     * source connected, e.g. disconnected HDMI In though...
     */
    struct v4l2_format fmt {0};

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(VIDIOC_G_FMT, &fmt) < 0) {
        LOG(mLog, ERROR) <<
            "Failed to call [VIDIOC_G_FMT] for device " << mDevPath;
        return false;
    }

    if (!fmt.fmt.pix.width || !fmt.fmt.pix.height) {
        LOG(mLog, DEBUG) << mDevPath << " has zero resolution";
        return false;
    }

    LOG(mLog, DEBUG) << mDevPath << " is a valid capture device";

    LOG(mLog, DEBUG) << "Driver:   " << cap.driver;
    LOG(mLog, DEBUG) << "Card:     " << cap.card;
    LOG(mLog, DEBUG) << "Bus info: " << cap.bus_info;

    return true;
}

void Camera::close()
{
    if (isOpen())
        ::close(mFd);

    mFd = -1;
}

/*
 ********************************************************************
 * Buffer related functionality.
 ********************************************************************
 */
int Camera::bufferGetMin()
{
    signed int min;

    controlGetValue(V4L2_CID_MIN_BUFFERS_FOR_CAPTURE, &min);
    return min;
}

int Camera::bufferRequest(int numBuffers)
{
    v4l2_requestbuffers req {0};

    req.count = numBuffers;
    req.type = cV4L2BufType;
    req.memory = mMemoryType;

    if (xioctl(VIDIOC_REQBUFS, &req) < 0)
        throw Exception("Failed to call [VIDIOC_REQBUFS] for device " +
                        mDevPath, errno);

    LOG(mLog, DEBUG) << "Initialized " << req.count <<
        " buffers for device " << mDevPath;

    return req.count;
}

v4l2_buffer Camera::bufferQuery(int index)
{
    v4l2_buffer buf {0};

    buf.type = cV4L2BufType;
    buf.memory = mMemoryType;
    buf.index = index;

    if (xioctl(VIDIOC_QUERYBUF, &buf) < 0)
        throw Exception("Failed to call [VIDIOC_QUERYBUF] for device " +
                        mDevPath, errno);

    return buf;
}

void Camera::bufferQueue(int index)
{
    v4l2_buffer buf {0};

    buf.type = cV4L2BufType;
    buf.memory = mMemoryType;
    buf.index = index;

    if (xioctl(VIDIOC_QBUF, &buf) < 0)
        throw Exception("Failed to call [VIDIOC_QBUF] for device " +
                        mDevPath, errno);
}

v4l2_buffer Camera::bufferDequeue()
{
    v4l2_buffer buf {0};

    buf.type = cV4L2BufType;
    buf.memory = mMemoryType;

    if (xioctl(VIDIOC_DQBUF, &buf) < 0)
        throw Exception("Failed to call [VIDIOC_DQBUF] for device " +
                        mDevPath, errno);

    return buf;
}

int Camera::bufferExport(int index)
{
    v4l2_exportbuffer expbuf = {
        .type = cV4L2BufType,
        .index = static_cast<uint32_t>(index)
    };

    if (xioctl(VIDIOC_EXPBUF, &expbuf))
        throw Exception("Failed to call [VIDIOC_EXPBUF] for device " +
                        mDevPath, errno);

    return expbuf.fd;
}

void *Camera::bufferGetData(int index)
{
    return nullptr;
}

/*
 ********************************************************************
 * Stream related functionality.
 ********************************************************************
 */
void Camera::streamStart(FrameDoneCallback clb)
{
    std::lock_guard<std::mutex> lock(mLock);

    if (mStreamStarted)
        return;

    mFrameDoneCallback = clb;

    mThread = std::thread(&Camera::eventThread, this);

    v4l2_buf_type type = cV4L2BufType;

    if (xioctl(VIDIOC_STREAMON, &type) < 0)
        LOG(mLog, ERROR) << "Failed to start streaming on device " << mDevPath;

    mStreamStarted = true;

    LOG(mLog, DEBUG) << "Started streaming on device " << mDevPath;
}

void Camera::streamStop()
{
    std::lock_guard<std::mutex> lock(mLock);

    if (!mStreamStarted)
        return;

    if (mPollFd)
        mPollFd->stop();

    if (mThread.joinable())
        mThread.join();

    v4l2_buf_type type = cV4L2BufType;

    if (xioctl(VIDIOC_STREAMOFF, &type) < 0)
        LOG(mLog, ERROR) << "Failed to stop streaming for " << mDevPath;

    mStreamStarted = false;

    LOG(mLog, DEBUG) << "Stopped streaming on device " << mDevPath;
}

void Camera::streamAlloc(int numBuffers, uint32_t width,
                         uint32_t height, uint32_t pixelFormat)
{
}

void Camera::streamRelease()
{
}

void Camera::eventThread()
{
    try {
        while (mPollFd->poll()) {
            v4l2_buffer buf = bufferDequeue();

            int next = buf.index;

            if (mFrameDoneCallback)
                next = mFrameDoneCallback(buf.index, buf.bytesused);
            bufferQueue(next);
        }
    } catch(const std::exception& e) {
        LOG(mLog, ERROR) << e.what();

        kill(getpid(), SIGTERM);
    }
}

/*
 ********************************************************************
 * Format related functionality.
 ********************************************************************
 */
v4l2_format Camera::formatGet()
{
    v4l2_format fmt {0};

    fmt.type = cV4L2BufType;

    if (xioctl(VIDIOC_G_FMT, &fmt) < 0)
        throw Exception("Failed to call [VIDIOC_G_FMT] for device " +
                        mDevPath, errno);
    return fmt;
}

void Camera::formatSet(v4l2_format fmt)
{
    LOG(mLog, DEBUG) << "Set format to " << fmt.fmt.pix.width <<
        "x" << fmt.fmt.pix.height;

    fmt.type = cV4L2BufType;

    if (xioctl(VIDIOC_S_FMT, &fmt) < 0)
        throw Exception("Failed to call [VIDIOC_S_FMT] for device " +
                        mDevPath, errno);
}

void Camera::formatSet(uint32_t width, uint32_t height, uint32_t pixelFormat)
{
    v4l2_format fmt = formatGet();

    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixelFormat;

    formatSet(fmt);
}

void Camera::formatEnumerate()
{
    v4l2_fmtdesc fmt = {0};

    mFormats.clear();

    fmt.type = cV4L2BufType;

    /* TODO:
     * 1. Multi-planar formats are not supported yet.
     * 2. Continuous/step-wise sizes/intervals are not supported.
     */
    while (xioctl(VIDIOC_ENUM_FMT, &fmt) >= 0) {
        Format format = {
            .pixelFormat = fmt.pixelformat,
            .description = std::string(reinterpret_cast<char *>(fmt.description)),
        };

        v4l2_frmsizeenum size;
        int index = 0;

        while (frameSizeGet(index++, fmt.pixelformat, size) >= 0) {
            FormatSize formatSize;

            if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                v4l2_frmivalenum interval;
                int index = 0;

                formatSize.width = size.discrete.width;
                formatSize.height = size.discrete.height;

                while (frameIntervalGet(index++, fmt.pixelformat,
                                        size.discrete.width,
                                        size.discrete.height,
                                        interval) >= 0)
                    formatSize.fps.push_back(interval.discrete);
            } else {
                LOG(mLog, WARNING) <<
                    "Step-wise/continuous intervals are not supported " << mDevPath;
                continue;
            }

            format.size.push_back(formatSize);
        }

        fmt.index++;

        mFormats.push_back(format);
    }
}

/*
 ********************************************************************
 * Frame rate related functionality.
 ********************************************************************
 */
v4l2_fract Camera::frameRateGet()
{
     v4l2_streamparm parm {0};

     parm.type = cV4L2BufType;

     if (xioctl(VIDIOC_G_PARM, &parm) < 0)
         throw Exception("Failed to call [VIDIOC_G_PARM] for device " +
                         mDevPath, errno);

     return parm.parm.capture.timeperframe;
}

void Camera::frameRateSet(int num, int denom)
{
    v4l2_streamparm parm {0};

    parm.type = cV4L2BufType;
    parm.parm.capture.timeperframe.numerator = num;
    parm.parm.capture.timeperframe.denominator = denom;

     if (xioctl(VIDIOC_S_PARM, &parm) < 0)
         throw Exception("Failed to call [VIDIOC_S_PARM] for device " +
                         mDevPath, errno);
}

int Camera::frameSizeGet(int index, uint32_t pixelFormat,
                         v4l2_frmsizeenum &size)
{
    memset(&size, 0, sizeof(size));

    size.index = index;
    size.pixel_format = pixelFormat;

    return xioctl(VIDIOC_ENUM_FRAMESIZES, &size);
}

int Camera::frameIntervalGet(int index, uint32_t pixelFormat,
                             uint32_t width, uint32_t height,
                             v4l2_frmivalenum &interval)
{
    memset(&interval, 0, sizeof(interval));

    interval.index = index;
    interval.pixel_format = pixelFormat;
    interval.width = width;
    interval.height = height;

    return xioctl(VIDIOC_ENUM_FRAMEINTERVALS, &interval);
}

/*
 ********************************************************************
 * Control related functionality.
 ********************************************************************
 */
void Camera::controlEnumerate()
{
    v4l2_queryctrl queryctrl {0};

    queryctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;

    while (xioctl(VIDIOC_QUERYCTRL, &queryctrl) == 0) {
        if (!(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
            LOG(mLog, DEBUG) << "Control " << queryctrl.name;

            if (queryctrl.type != V4L2_CTRL_TYPE_MENU) {
                ControlInfo ctrl {0};

                ctrl.v4l2_cid = queryctrl.id;
                ctrl.flags = queryctrl.flags;
                ctrl.minimum = queryctrl.minimum;
                ctrl.maximum = queryctrl.maximum;
                ctrl.default_value = queryctrl.default_value;
                ctrl.step = queryctrl.step;

                mControls.push_back(ctrl);
            }
        }
        queryctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    /*
     * Querying after the last control must return EINVAL indicating
     * that there are no more controls.
     */
    if (errno != EINVAL)
        throw Exception("Failed to query controls for device " +
                        mDevPath, errno);
}

Camera::ControlInfo Camera::controlGetInfo(std::string name)
{
    int v4l2_cid;

    if (name == XENCAMERA_CTRL_CONTRAST_STR)
        v4l2_cid = V4L2_CID_CONTRAST;
    else if (name == XENCAMERA_CTRL_BRIGHTNESS_STR)
        v4l2_cid = V4L2_CID_BRIGHTNESS;
    else if (name == XENCAMERA_CTRL_HUE_STR)
        v4l2_cid = V4L2_CID_HUE;
    else if (name == XENCAMERA_CTRL_SATURATION_STR)
        v4l2_cid = V4L2_CID_SATURATION;
    else
        throw Exception("Wrong control name " + name + " for device " +
                        mDevPath, EINVAL);

    for (auto const& ctrl: mControls)
        if (ctrl.v4l2_cid == v4l2_cid)
            return ctrl;

    throw Exception("Control " + name + " not found for device " +
                    mDevPath, EINVAL);
}

void Camera::controlSetValue(int v4l2_cid, signed int value)
{
    v4l2_control control {0};

    control.id = v4l2_cid;
    control.value = value;

    if (xioctl(VIDIOC_S_CTRL, &control) < 0)
        throw Exception("Failed to call [VIDIOC_S_CTRL] for device " +
                        mDevPath, errno);
}

void Camera::controlGetValue(int v4l2_cid, signed int *value)
{
    v4l2_control control {0};

    control.id = v4l2_cid;

    if (xioctl(VIDIOC_G_CTRL, &control) < 0)
        throw Exception("Failed to call [VIDIOC_G_CTRL] for device " +
                        mDevPath, errno);
    *value = control.value;
}

