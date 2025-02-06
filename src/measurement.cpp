/*
    Copyright (C) 2016-2025 zafaco GmbH

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License version 3
    as published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "measurement.h"

CMeasurement::CMeasurement()
{
}

CMeasurement::~CMeasurement()
{
}

CMeasurement::CMeasurement(
    CConfigManager *pConfig, CConfigManager *pXml, CConfigManager *pService, string sProvider,
    int nTestCase, CCallback *pCallback)
{
    mConfig = pConfig;
    mXml = pXml;
    mService = pService;
    mProvider = sProvider;
    mTestCase = nTestCase;
    mCallback = pCallback;

    switch (conf.nTestCase)
    {
    case 1:
        conf.instances = 1;
        mInitialCallbackDelay = 0;
        break;
    case 2:
        conf.instances = 1;
        mInitialCallbackDelay = 1000000;
        break;
    case 3:
        conf.instances = pXml->readLong(conf.sProvider, "DL_STREAMS", 4);
        mInitialCallbackDelay = ::TCP_STARTUP;
        break;
    case 4:
        conf.instances = pXml->readLong(conf.sProvider, "UL_STREAMS", 4);
        mInitialCallbackDelay = ::TCP_STARTUP;
        break;
    default:
        conf.instances = 1;
        mInitialCallbackDelay = 1000000;
        break;
    }

    mTimer = std::make_unique<CTimer>(conf.instances, mCallback, mInitialCallbackDelay);

    if (mTimer->createThread() != 0)
    {
        TRC_ERR("Error: Failure while creating the Thread - Timer!");
    }
}

int CMeasurement::startMeasurement()
{
    measurements.streams = 0;

    map<int, unsigned long long> mTmpMap;

    switch (mTestCase)
    {

    case 1:
    {
        MEASUREMENT_DURATION = 60;

        std::unique_ptr<Ip> ip = std::make_unique<Ip>(mConfig, mXml, mService, mProvider);
        ip->createThread();

        mCallback->ipThread = ip.get();

        ip->waitForEnd();
        while (!::PERFORMED_IP && !::hasError && ::RUNNING)
        {
            usleep(100000);
        }

        break;
    }

    case 2:
    {
        std::unique_ptr<Ping> ping = std::make_unique<Ping>(mXml, mService, mProvider);
        ping->createThread();

        mCallback->pingThread = ping.get();

        ping->waitForEnd();
        while (!::PERFORMED_RTT && !::hasError && ::RUNNING)
        {

            usleep(100000);
        }

        break;
    }

    case 3:
    {
        std::vector<Download *> vDownloadThreads;

        MEASUREMENT_DURATION = mXml->readLong(mProvider, "DL_DURATION", 10);

        int tls_overhead = 0;
        int mTls = mXml->readLong(mProvider, "TLS", 1);
        if (mTls == 1 &&
            !mCallback->jMeasurementResultsIp["tls_overhead"].string_value().empty() &&
            CTool::toInt(mCallback->jMeasurementResultsIp["tls_overhead"].string_value()) > 0)
        {
            tls_overhead = CTool::toInt(
                mCallback->jMeasurementResultsIp["tls_overhead"].string_value());
        }

        for (int i = 0; i < conf.instances; i++)
        {
            Download *download = new Download(mConfig, mXml, mService, mProvider, tls_overhead);
            if (download->createThread() != 0)
            {
                TRC_ERR("Error: Failure while creating the Thread - DownloadThread!");
                return -1;
            }

            vDownloadThreads.push_back(download);
        }

        mCallback->vDownloadThreads = vDownloadThreads;

        for (vector<Download *>::iterator itThread = vDownloadThreads.begin();
             itThread != vDownloadThreads.end(); ++itThread)
        {
            (*itThread)->waitForEnd();
        }

        while (!::PERFORMED_DOWNLOAD && !::hasError && ::RUNNING)
        {

            usleep(100000);
        }

        for (vector<Download *>::iterator itThread = vDownloadThreads.begin();
             itThread != vDownloadThreads.end(); ++itThread)
        {
            delete (*itThread);
        }

        break;
    }

    case 4:
    {
        std::vector<Upload *> vUploadThreads;

        MEASUREMENT_DURATION = mXml->readLong(mProvider, "UL_DURATION", 10) + UPLOAD_ADDITIONAL_MEASUREMENT_DURATION;

        measurements.upload.datasize = 0;

        for (int i = 0; i < conf.instances; i++)
        {
            Upload *upload = new Upload(mConfig, mXml, mService, mProvider);
            if (upload->createThread() != 0)
            {
                TRC_ERR("Error: Failure while creating the Thread - UploadThread!");
                return -1;
            }

            vUploadThreads.push_back(upload);
        }

        mCallback->vUploadThreads = vUploadThreads;

        for (vector<Upload *>::iterator itThread = vUploadThreads.begin();
             itThread != vUploadThreads.end(); ++itThread)
        {
            (*itThread)->waitForEnd();
        }

        while (!::PERFORMED_UPLOAD && !::hasError && ::RUNNING)
        {

            usleep(100000);
        }

        for (vector<Upload *>::iterator itThread = vUploadThreads.begin();
             itThread != vUploadThreads.end(); ++itThread)
        {
            delete (*itThread);
        }

        break;
    }
    }

    mTimer->waitForEnd();

    return 0;
}
