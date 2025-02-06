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

#ifndef PING_H
#define PING_H

#include "../header.h"

class Ping : public CBasisThread
{
private:
    int mPingPort;
    int mPingQuery;
    unsigned long long mTimeDiff;

public:
    struct measurement_data mPingResult;

    uint64_t pid;

    unsigned long long measurementTimeStart;
    unsigned long long measurementTimeEnd;
    unsigned long long measurementTimeDuration;

    string mServer;
    string mServerName;
    string mClient;

    int nPingTarget;

    int nSize;
    int nErrors;
    int nMissing;
    int nReplies;

    Ping();

    virtual ~Ping();

    Ping(CConfigManager *pXml, CConfigManager *pService, string sProvider);

    int run();
};

#endif
