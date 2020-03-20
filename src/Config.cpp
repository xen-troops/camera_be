// SPDX-License-Identifier: GPL-2.0

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2020 EPAM Systems Inc.
 */

#include "Config.hpp"

using std::string;
using std::to_string;

using libconfig::Setting;
using libconfig::FileIOException;
using libconfig::ParseException;
using libconfig::SettingException;
using libconfig::SettingNotFoundException;

Config::Config(string fileName):
    mLog("Config")
{
    const char* cfgName = cDefaultCfgName;

    try
    {
        if (!fileName.empty())
            cfgName = fileName.c_str();

        LOG(mLog, DEBUG) << "Open file: " << cfgName;

        mConfig.readFile(cfgName);

        readPipelineConfig(mPipelineConfig);
    }
    catch(const FileIOException& e)
    {
        throw ConfigException("Config: can't open file: " + string(cfgName));
    }
    catch(const ParseException& e)
    {
        throw ConfigException("Config: " + string(e.getError()) +
                              ", file: " + string(e.getFile()) +
                              ", line: " + to_string(e.getLine()));
    }
}

void Config::readPipelineConfig(PipelineConfig& config)
{
    string sectionName = "mediactl";

    try
    {
        config.link.clear();
        config.source_fmt.clear();
        config.sink_fmt.clear();

        Setting& setting = mConfig.lookup(sectionName);

        config.link = static_cast<const char*>(setting.lookup("link"));
        config.source_fmt = static_cast<const char*>(setting.lookup("source_fmt"));
        config.sink_fmt = static_cast<const char*>(setting.lookup("sink_fmt"));

        LOG(mLog, DEBUG) << "Media pipeline configuration";
        LOG(mLog, DEBUG) << "link:       " << config.link;
        LOG(mLog, DEBUG) << "source_fmt: " << config.source_fmt;
        LOG(mLog, DEBUG) << "sink_fmt:   " << config.sink_fmt;
    }
    catch(const SettingNotFoundException& e)
    {
        throw ConfigException(string("Config: error reading ") + sectionName);
    }
}
