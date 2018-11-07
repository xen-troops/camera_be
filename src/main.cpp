// SPDX-License-Identifier: GPL-2.0

/*
 * Xen para-virtualized camera backend
 *
 * Copyright (C) 2018 EPAM Systems Inc.
 */

#include <fstream>

#include <csignal>
#include <execinfo.h>
#include <getopt.h>

#include <xen/be/Log.hpp>
#include <xen/be/Utils.hpp>
#include <xen/io/cameraif.h>

#include "Backend.hpp"
#include "Version.hpp"

using std::cout;
using std::endl;
using std::ofstream;
using std::string;

using XenBackend::Log;
using XenBackend::Utils;

string gLogFileName;

int gRetStatus = EXIT_SUCCESS;

/*******************************************************************************
 *
 ******************************************************************************/

void segmentationHandler(int sig)
{
    void *array[20];
    size_t size;

    LOG("Main", ERROR) << "Segmentation fault!";

    size = backtrace(array, 20);

    backtrace_symbols_fd(array, size, STDERR_FILENO);

    raise(sig);
}

void registerSignals()
{
    struct sigaction act {};

    act.sa_handler = segmentationHandler;
    act.sa_flags = SA_RESETHAND;

    sigaction(SIGSEGV, &act, nullptr);
}

void waitSignals()
{
    sigset_t set;
    int signal;

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    sigprocmask(SIG_BLOCK, &set, nullptr);

    sigwait(&set,&signal);

    if (signal == SIGTERM)
        gRetStatus = EXIT_FAILURE;
}

bool commandLineOptions(int argc, char *argv[])
{
    int opt = -1;

    while((opt = getopt(argc, argv, "v:l:fh?")) != -1) {
        switch(opt) {
        case 'v':
            if (!Log::setLogMask(string(optarg)))
                return false;
            break;

        case 'l':
            gLogFileName = optarg;
            break;

        case 'f':
            Log::setShowFileAndLine(true);
            break;

        default:
            return false;
        }
    }

    return true;
}


int main(int argc, char *argv[])
{
    try {
        registerSignals();

        if (commandLineOptions(argc, argv)) {
            LOG("Main", INFO) << "backend version:  " <<
                VERSION;
            LOG("Main", INFO) << "libxenbe version: " <<
                Utils::getVersion();

            ofstream logFile;

            if (!gLogFileName.empty()) {
                logFile.open(gLogFileName);
                Log::setStreamBuffer(logFile.rdbuf());
            }

            Backend backend(XENCAMERA_DRIVER_NAME);

            backend.start();

            waitSignals();

            logFile.close();
        } else {
            cout << "Usage: " << argv[0]
                << " [-l <file>] [-v <level>]"
                << endl;
            cout << "\t-l -- log file" << endl;
            cout << "\t-v -- verbose level in format: "
                << "<module>:<level>;<module:<level>" << endl;
            cout << "\t      use * for mask selection:"
                << " *:Debug,Mod*:Info" << endl;

            gRetStatus = EXIT_FAILURE;
        }
    }
    catch(const std::exception& e) {
        Log::setStreamBuffer(cout.rdbuf());

        LOG("Main", ERROR) << e.what();

        gRetStatus = EXIT_FAILURE;
    }

    return gRetStatus;
}
