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

#ifndef UPLOAD_H
#define UPLOAD_H

#include "../header.h"
#include "upload_sender.h"

class Upload : public CBasisThread
{
private:
    int mPort;
    int mTls;
    string mUploadString;

    CConfigManager *mConfig;

public:
    int mResponse;

    struct measurement_data mUpload;

    uint64_t pid;

    unsigned long long measurementTimeStart;
    unsigned long long measurementTimeEnd;
    unsigned long long measurementTimeDuration;

    string mServer;
    string mServerName;
    string mClient;

    unsigned long long nDataRecv;
    unsigned long long nTimeRecv;

    unsigned long long nTimeRecvExa;
    unsigned long long nTimeRecvExaFirst;
    unsigned long long nTimeRecvFirst;

    unsigned long long nHttpResponseDuration;
    unsigned long long nHttpResponseReportValue;

    Upload();

    virtual ~Upload();

    Upload(CConfigManager *pConfig, CConfigManager *pXml, CConfigManager *pService, string sProvider);

    int run();
};

#endif
