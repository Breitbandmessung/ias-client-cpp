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

#include "header.h"

#ifndef TYPE_H_
#define TYPE_H_

using namespace std;

struct measurement
{
public:
    unsigned long long time;
    unsigned long long timezone;
    string date;

    unsigned long long measurementDuration;

    int streams;
    string client;
    string server;
    string servername;

    string os;
    string osversion;
    string softwareversion;
    string softwareversiontool;

    unsigned long long starttime;
    unsigned long long endtime;
    unsigned long long totaltime;

    struct measurement_data ping;
    struct measurement_data download;
    struct measurement_data upload;

    map<string, string> parameter;
    map<string, map<string, string>> settings;

    map<string, string> metric;
    map<string, string> metatag;
    map<string, string> units;
    map<string, string> message;
};

enum class MeasurementPhase
{
    INIT,
    IP,
    PING,
    DOWNLOAD,
    UPLOAD,
    END
};

#endif
