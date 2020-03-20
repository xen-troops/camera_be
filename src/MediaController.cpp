// SPDX-License-Identifier: GPL-2.0

/*
 * Xen para-virtualized camera backend
 *
 * Based on:
 * https://git.linuxtv.org/v4l-utils.git/tree/utils/media-ctl
 *
 * Copyright (C) 2020 EPAM Systems Inc.
 */

#include <xen/be/Exception.hpp>
#include "MediaController.hpp"

extern "C" {
#include <mediactl/mediactl.h>
#include <mediactl/v4l2subdev.h>
}

using XenBackend::Exception;

MediaController::MediaController(std::string devName, ConfigPtr config):
    mLog("MediaController"),
    mDevPath("/dev/" + devName),
    mConfig(config)
{
    try {
        init();
    } catch (...) {
        release();
        throw;
    }
}

MediaController::~MediaController()
{
    release();
}

void MediaController::init()
{
    int ret = -ENODEV;

    LOG(mLog, DEBUG) << "Initializing media device " << mDevPath;

    const auto& config = mConfig->getPipelineConfig();

    auto throwIfFalse = [&, this](bool res, std::string&& sMsg, std::string dMsg) {
        if (!res) {
            LOG(mLog, ERROR) << sMsg << dMsg;
            throw Exception("Failed to initialize media device " + mDevPath, -ret);
        }
    };

    m_mediaDevice = media_device_new(mDevPath.c_str());
    throwIfFalse(m_mediaDevice != nullptr, "Failed to open device ", mDevPath);

    ret = media_device_enumerate(m_mediaDevice);
    throwIfFalse(!ret, "Failed to enumerate device ", mDevPath);

    showInfo();

    ret = media_reset_links(m_mediaDevice);
    throwIfFalse(!ret, "Failed to reset links", "");

    ret = media_parse_setup_links(m_mediaDevice, config.link.c_str());
    throwIfFalse(!ret, "Failed to setup link ", config.link);

    /*
     * When the pipeline is configured it's time to propagate the format
     * to the video source/sink.
     */
    ret = v4l2_subdev_parse_setup_formats(m_mediaDevice, config.source_fmt.c_str());
    throwIfFalse(!ret, "Failed to setup source format ", config.source_fmt);

    ret = v4l2_subdev_parse_setup_formats(m_mediaDevice, config.sink_fmt.c_str());
    throwIfFalse(!ret, "Failed to setup sink format ", config.sink_fmt);
}

void MediaController::release()
{
    LOG(mLog, DEBUG) << "Releasing media device " << mDevPath;

    if (m_mediaDevice) {
        media_reset_links(m_mediaDevice);
        media_device_unref(m_mediaDevice);
    }
}

void MediaController::showInfo()
{
    const struct media_device_info *info = media_get_info(m_mediaDevice);
    char version[16];

    LOG(mLog, DEBUG) << "Media device information";

    snprintf(version, sizeof(version), "%u.%u.%u",
        (info->driver_version >> 16) & 0xff,
        (info->driver_version >> 8) & 0xff,
        (info->driver_version >> 0) & 0xff);

    LOG(mLog, DEBUG) << "Driver:         " << info->driver;
    LOG(mLog, DEBUG) << "Model:          " << info->model;
    LOG(mLog, DEBUG) << "Serial:         " << info->serial;
    LOG(mLog, DEBUG) << "Bus info:       " << info->bus_info;
    LOG(mLog, DEBUG) << "HW revision:    " << info->hw_revision;
    LOG(mLog, DEBUG) << "Driver version: " << version;
}
