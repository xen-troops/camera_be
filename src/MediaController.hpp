/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2020 EPAM Systems Inc.
 */
#ifndef SRC_MEDIACONTROLLER_HPP_
#define SRC_MEDIACONTROLLER_HPP_

#include <string>
#include <memory>

#include <xen/be/Log.hpp>
#include "Config.hpp"

class MediaController final
{
public:
    MediaController(std::string devName, ConfigPtr config);
    ~MediaController();

    MediaController(MediaController&&) = delete;
    MediaController(const MediaController&) = delete;
    void operator = (const MediaController&) = delete;

private:
    XenBackend::Log mLog;
    const std::string mDevPath;
    ConfigPtr mConfig;

    void init();
    void release();
    void showInfo();

    struct media_device *m_mediaDevice = nullptr;
};

typedef std::unique_ptr<MediaController> MediaControllerPtr;

#endif /* SRC_MEDIACONTROLLER_HPP_ */
