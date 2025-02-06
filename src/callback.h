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

#ifndef CALLBACK_H
#define CALLBACK_H

#include "header.h"

#include "tests/ip.h"
#include "tests/ping.h"
#include "tests/download.h"
#include "tests/upload.h"

using namespace std;

class CCallback
{
    friend void startTestCase(int nTestCase);

    friend void measurementStart(std::string measurementParameters);

private:
    Json jMeasurementParameters;

    void callbackToPlatform(string cmd, string msg, int error_code, string error_description);

    void ipCallback(string cmd);

    void rttUdpCallback(string cmd);

    void downloadCallback(string cmd);

    void uploadCallback(string cmd);

    Json::object getMeasurementResults(struct measurement tempMeasurement, struct measurement_data data, string cmd);

    void calculateMeasurementResultsDownloadUpload(string cmd, void *vThreads);

    Json::object getMeasurementResultsAggregate(struct measurement tempMeasurement, struct measurement_data data, string cmd);

public:
    Json::object jMeasurementResultsTime;
    Json::object jMeasurementResultsPeer;
    Json::object jMeasurementResultsIp;
    Json::object jMeasurementResultsRttUdp;
    Json::array jMeasurementResultsDownload;
    Json::array jMeasurementResultsDownloadStreams;
    Json::array jMeasurementResultsUpload;
    Json::array jMeasurementResultsUploadStreams;
    int mTestCase;

    Ip *ipThread;
    Ping *pingThread;
    vector<Download *> vDownloadThreads;
    vector<Upload *> vUploadThreads;

    CCallback(Json measurementParameters);

    virtual ~CCallback();

    void callback(string cmd, string msg, int error_code, string error_description);

#ifdef IAS_IOS
    static std::function<void(Json::object &)> iosCallbackFunc;
#endif
};

#endif
