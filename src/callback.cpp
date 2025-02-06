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

#include "callback.h"

#ifdef __ANDROID__

#include "android_connector.h"

#endif

extern MeasurementPhase currentTestPhase;

CCallback::CCallback(Json measurementParameters)
{
    jMeasurementParameters = measurementParameters;

    ::TIMESTAMP_MEASUREMENT_START = CTool::get_timestamp() * 1000;

    jMeasurementResultsTime["measurement_start"] = to_string(::TIMESTAMP_MEASUREMENT_START);
}

CCallback::~CCallback()
{
}

#ifdef IAS_IOS
std::function<void(Json::object &)> CCallback::iosCallbackFunc = nullptr;
#endif

void CCallback::callback(string cmd, string msg, int error_code, string error_description)
{
    TRC_DEBUG("Callback Received: cmd: " + cmd + ", msg: " + msg);

    if (!jMeasurementResultsPeer.size())
    {
        jMeasurementResultsPeer["url"] = jMeasurementParameters["wsTarget"].string_value();
        jMeasurementResultsPeer["port"] = jMeasurementParameters["wsTargetPort"].string_value();
        jMeasurementResultsPeer["tls"] = jMeasurementParameters["wsWss"].string_value();
    }

    if (cmd.compare("report") == 0 || cmd.compare("finish") == 0)
    {
        if (mTestCase == 1)
        {
            ipCallback(cmd);

            if (cmd.compare("finish") == 0)
            {
                PERFORMED_IP = true;
            }
        }

        if (mTestCase == 2)
        {
            rttUdpCallback(cmd);

            if (cmd.compare("finish") == 0)
            {
                PERFORMED_RTT = true;
            }
        }

        if (mTestCase == 3)
        {
            downloadCallback(cmd);

            if (cmd.compare("finish") == 0)
            {
                PERFORMED_DOWNLOAD = true;
            }
        }

        if (mTestCase == 4)
        {
            uploadCallback(cmd);

            if (cmd.compare("finish") == 0)
            {
                PERFORMED_UPLOAD = true;
            }
        }
    }

    if (::IP == PERFORMED_IP && ::RTT == PERFORMED_RTT && ::DOWNLOAD == PERFORMED_DOWNLOAD && ::UPLOAD == PERFORMED_UPLOAD)
    {
        jMeasurementResultsTime["measurement_end"] = to_string(CTool::get_timestamp() * 1000);
    }

    callbackToPlatform(cmd, msg, error_code, error_description);

    if (cmd.compare("finish") == 0 && ::IP == PERFORMED_IP && ::RTT == PERFORMED_RTT && ::DOWNLOAD == PERFORMED_DOWNLOAD && ::UPLOAD == PERFORMED_UPLOAD)
    {
        currentTestPhase = MeasurementPhase::END;
        callbackToPlatform("completed", msg, error_code, error_description);
    }
}

void CCallback::callbackToPlatform(string cmd, string msg, int error_code, string error_description)
{
    Json::object jMeasurementResults = Json::object{};
    jMeasurementResults["cmd"] = cmd;
    jMeasurementResults["msg"] = msg;
    jMeasurementResults["test_case"] = conf.sTestName;

#ifdef __ANDROID__
    AndroidConnector &connector = AndroidConnector::getInstance();
#endif

    if (error_code != 0)
    {
        jMeasurementResults["error_code"] = error_code;
        jMeasurementResults["error_description"] = error_description;
        TRC_ERR("Error: " + error_description + ", code: " + to_string(error_code));
    }

    if (jMeasurementResultsIp.size() > 0)
    {
        jMeasurementResults["ip_info"] = Json(jMeasurementResultsIp);
    }

    if (jMeasurementResultsRttUdp.size() > 0)
    {
        jMeasurementResults["rtt_udp_info"] = Json(jMeasurementResultsRttUdp);
    }

    if (jMeasurementResultsDownload.size() > 0)
    {
        jMeasurementResults["download_info"] = Json(jMeasurementResultsDownload);
    }

    if (jMeasurementResultsDownloadStreams.size() > 0)
    {
        jMeasurementResults["download_raw_data"] = Json(jMeasurementResultsDownloadStreams);
    }

    if (jMeasurementResultsUpload.size())
    {
        jMeasurementResults["upload_info"] = Json(jMeasurementResultsUpload);
    }

    if (jMeasurementResultsUploadStreams.size())
    {
        jMeasurementResults["upload_raw_data"] = Json(jMeasurementResultsUploadStreams);
    }

    if (jMeasurementResultsTime.size())
    {
        jMeasurementResults["time_info"] = Json(jMeasurementResultsTime);
    }

    if (jMeasurementResultsPeer.size())
    {
        jMeasurementResults["peer_info"] = Json(jMeasurementResultsPeer);
    }

#ifdef __ANDROID__
    connector.callback(jMeasurementResults);

    if (currentTestPhase == MeasurementPhase::END)
    {
        connector.callbackFinished(jMeasurementResults);
    }

    if (cmd == "completed" || cmd == "error")
    {
        connector.detachCurrentThreadFromJvm();
    }
#elif IAS_IOS
    if (CCallback::iosCallbackFunc != nullptr)
    {
        CCallback::iosCallbackFunc(jMeasurementResults);
    }
#else
    string platform = ::PLATFORM;
    string clientos = ::CLIENT_OS;

    if (platform.compare("desktop") == 0 && (clientos.compare("linux") == 0 || clientos.compare("macos") == 0))
    {
        TRC_DEBUG("Callback: " + Json(jMeasurementResults).dump());
    }
#endif
}

void CCallback::ipCallback(string cmd)
{

    pthread_mutex_lock(&mutex1);

    ipThread->measurementTimeDuration = CTool::get_timestamp() - ipThread->measurementTimeStart;

    if (cmd.compare("finish") == 0)
    {
        ipThread->measurementTimeEnd = CTool::get_timestamp();
        jMeasurementResultsTime["ip_end"] = to_string(ipThread->measurementTimeEnd * 1000);
    }

    jMeasurementResultsTime["ip_start"] = to_string(ipThread->measurementTimeStart * 1000);

    if (ipThread->jMeasurementResultsIp.size() > 0)
    {
        Json::object jMeasurementResultsTemp = jMeasurementResultsIp;
        jMeasurementResultsIp = ipThread->jMeasurementResultsIp;

        if (!jMeasurementResultsTemp["client_v4"].string_value().empty())
        {
            jMeasurementResultsIp["client_v4"] = jMeasurementResultsTemp["client_v4"].string_value();
        }

        ::TIMER_STOPPED = true;
    }

    pthread_mutex_unlock(&mutex1);
}

void CCallback::rttUdpCallback(string cmd)
{
    struct measurement tempMeasurement;

    pthread_mutex_lock(&mutex1);

    struct measurement_data mPingResult = pingThread->mPingResult;

    pingThread->measurementTimeDuration = CTool::get_timestamp() - pingThread->measurementTimeStart;

    if (cmd.compare("finish") == 0)
    {
        pingThread->measurementTimeEnd = CTool::get_timestamp();
        jMeasurementResultsTime["rtt_udp_end"] = to_string(pingThread->measurementTimeEnd * 1000);
    }

    for (map<int, unsigned long long>::iterator AI = mPingResult.results.begin();
         AI != mPingResult.results.end(); ++AI)
    {

        tempMeasurement.ping.results[(*AI).first] += (*AI).second;
    }

    CTool::calculateResults(tempMeasurement.ping, 1, 0, 0);

    tempMeasurement.ping.packetsize = pingThread->nSize;
    tempMeasurement.ping.requests = pingThread->nReplies + pingThread->nMissing + pingThread->nErrors;
    tempMeasurement.ping.replies = pingThread->nReplies;
    tempMeasurement.ping.missing = pingThread->nMissing;
    tempMeasurement.ping.errors = pingThread->nErrors;

    tempMeasurement.ping.measurement_phase_progress = (tempMeasurement.ping.replies + tempMeasurement.ping.missing + tempMeasurement.ping.errors) / ((float)pingThread->nPingTarget);

    tempMeasurement.ping.starttime = pingThread->measurementTimeStart;
    tempMeasurement.ping.endtime = pingThread->measurementTimeEnd;
    tempMeasurement.ping.totaltime = pingThread->measurementTimeDuration;

    jMeasurementResultsTime["rtt_udp_start"] = to_string(tempMeasurement.ping.starttime * 1000);

    jMeasurementResultsPeer["ip"] = pingThread->mServer;

    Json::array jRtts;

    int index = 1;
    for (auto &rtt : tempMeasurement.ping.interim_values)
    {
        Json jRtt = Json::object{
            {"rtt_ns", rtt * 1000},
            {"relative_time_ns_measurement_start", std::to_string(mPingResult.results_timestamp[index])},
        };
        jRtts.push_back(jRtt);
        index++;
    }

    pthread_mutex_unlock(&mutex1);

    TRC_INFO("RTT UDP AVG MS: " + CTool::to_string_precision(tempMeasurement.ping.avg / 1000.0, 3));

    Json::object jMeasurementResults;
    jMeasurementResults["duration_ns"] = to_string(tempMeasurement.ping.totaltime * 1000);
    jMeasurementResults["average_ns"] = to_string(tempMeasurement.ping.avg * 1000);
    jMeasurementResults["median_ns"] = to_string(tempMeasurement.ping.median_ns);
    jMeasurementResults["min_ns"] = to_string(tempMeasurement.ping.min * 1000);
    jMeasurementResults["max_ns"] = to_string(tempMeasurement.ping.max * 1000);
    jMeasurementResults["num_sent"] = to_string(tempMeasurement.ping.requests);
    jMeasurementResults["num_received"] = to_string(tempMeasurement.ping.replies);
    jMeasurementResults["num_error"] = to_string(tempMeasurement.ping.errors);
    jMeasurementResults["num_missing"] = to_string(tempMeasurement.ping.missing);
    jMeasurementResults["packet_size"] = to_string(tempMeasurement.ping.packetsize);
    jMeasurementResults["standard_deviation_ns"] = to_string(tempMeasurement.ping.standard_deviation_ns);
    jMeasurementResults["rtts"] = jRtts;
    jMeasurementResults["progress"] = tempMeasurement.ping.measurement_phase_progress;

    jMeasurementResultsRttUdp = jMeasurementResults;
}

void CCallback::downloadCallback(string cmd)
{
    calculateMeasurementResultsDownloadUpload(cmd, &vDownloadThreads);
}

void CCallback::uploadCallback(string cmd)
{
    calculateMeasurementResultsDownloadUpload(cmd, &vUploadThreads);
}

void CCallback::calculateMeasurementResultsDownloadUpload(string cmd, void *vThreads)
{
    vector<void *> &vThreadsVector = *reinterpret_cast<vector<void *> *>(vThreads);

    Json::array jMeasurementResults;
    Json::array jMeasurementResultsStreams;
    Json::array jMeasurementResultsPerStream;

    int max_steps = 0;

    max_steps = (TIMER_DURATION - (TIMER_DURATION % 500000)) / 500000;

    if (conf.sTestName.compare("download") == 0)
    {
        if (max_steps > MEASUREMENT_DURATION * 2)
        {
            max_steps = MEASUREMENT_DURATION * 2;
        }
    }
    else if (conf.sTestName.compare("upload") == 0)
    {
        if (max_steps > (MEASUREMENT_DURATION - UPLOAD_ADDITIONAL_MEASUREMENT_DURATION) * 2)
        {
            max_steps = (MEASUREMENT_DURATION - UPLOAD_ADDITIONAL_MEASUREMENT_DURATION) * 2;
        }
    }

    for (int current_step = 1; current_step <= max_steps; current_step++)
    {
        int streamId = 0;

        struct measurement measurement_aggregate;
        measurement_aggregate.streams = 0;

        unsigned long long bytes_aggregate = 0;
        unsigned long long bytes_distinct_aggregate = 0;
        unsigned long long bytes_slow_start_aggregate = 0;
        unsigned long long bytes_total_aggregate = 0;
        unsigned long long duration_ns_total = 0;
        unsigned long long relative_time_ns_measurement_start = 0;

        unsigned long long current_timestamp = CTool::get_timestamp();

        for (vector<void *>::iterator itThread = vThreadsVector.begin();
             itThread != vThreadsVector.end(); ++itThread)
        {

            pthread_mutex_lock(&mutex1);

            struct measurement_data mResults;
            unsigned long long bytes = 0;
            unsigned long long bytes_slow_start = 0;
            unsigned long long bytes_distinct = 0;

            unsigned long long relative_time_ns_total = 0;

            Json::object jMeasurementResultPerStream;
            jMeasurementResultPerStream["stream_id"] = streamId;
            jMeasurementResultPerStream["bytes"] = "0";
            jMeasurementResultPerStream["bytes_distinct"] = "0";
            jMeasurementResultPerStream["bytes_slow_start"] = "0";
            jMeasurementResultPerStream["bytes_total"] = "0";
            jMeasurementResultPerStream["relative_time_ns"] = "0";
            jMeasurementResultPerStream["relative_time_ns_distinct"] = "0";
            jMeasurementResultPerStream["relative_time_ns_total"] = "0";
            jMeasurementResultPerStream["relative_time_ns_measurement_start"] = "0";

            if (conf.sTestName.compare("download") == 0)
            {
                mResults = ((Download *)*itThread)->mDownload;
                bytes_slow_start = ((Download *)*itThread)->mDownload.datasize_slow_start / 8;

                ((Download *)*itThread)->measurementTimeDuration = current_timestamp - ((Download *)*itThread)->measurementTimeStart;
                relative_time_ns_total = ((Download *)*itThread)->measurementTimeDuration;

                jMeasurementResultsTime["download_start"] = to_string(((Download *)*itThread)->measurementTimeStart * 1000);

                if (cmd.compare("finish") == 0)
                {
                    ((Download *)*itThread)->measurementTimeEnd = CTool::get_timestamp();
                    jMeasurementResultsTime["download_end"] = to_string(((Download *)*itThread)->measurementTimeEnd * 1000);
                }

                if (((Download *)*itThread)->mResponse > 0)
                {
                    measurement_aggregate.streams++;
                }

                jMeasurementResultsPeer["ip"] = ((Download *)*itThread)->mServer;
            }
            else if (conf.sTestName.compare("upload") == 0)
            {
                mResults = ((Upload *)*itThread)->mUpload;
                bytes_slow_start = ((Upload *)*itThread)->mUpload.datasize_slow_start / 8;

                ((Upload *)*itThread)->measurementTimeDuration = current_timestamp - ((Upload *)*itThread)->measurementTimeStart;
                relative_time_ns_total = ((Upload *)*itThread)->measurementTimeDuration;

                jMeasurementResultsTime["upload_start"] = to_string(((Upload *)*itThread)->measurementTimeStart * 1000);

                if (cmd.compare("finish") == 0)
                {
                    ((Upload *)*itThread)->measurementTimeEnd = CTool::get_timestamp();
                    jMeasurementResultsTime["upload_end"] = to_string(((Upload *)*itThread)->measurementTimeEnd * 1000);
                }

                if (((Upload *)*itThread)->mResponse > 0)
                {
                    measurement_aggregate.streams++;
                }

                jMeasurementResultsPeer["ip"] = ((Upload *)*itThread)->mServer;
            }

            int i = 1;
            for (auto &result : mResults.results)
            {

                if ((result.first < ::TIMER_INDEX_UPLOAD_FIRST || ::TIMER_INDEX_UPLOAD_FIRST == 0) && conf.sTestName.compare("upload") == 0)
                {
                    continue;
                }

                if (i <= current_step)
                {
                    if (conf.sTestName.compare("download") == 0)
                    {
                        measurement_aggregate.download.results[i] += result.second;
                    }
                    else if (conf.sTestName.compare("upload") == 0)
                    {
                        measurement_aggregate.upload.results[i] += result.second;
                    }

                    bytes += result.second / 8;
                }

                if (i == current_step)
                {
                    bytes_distinct = result.second / 8;
                }

                if (i == max_steps)
                {
                    break;
                }
                i++;
            }

            jMeasurementResultPerStream["bytes"] = to_string(bytes);
            jMeasurementResultPerStream["bytes_distinct"] = to_string(bytes_distinct);
            jMeasurementResultPerStream["bytes_slow_start"] = to_string(bytes_slow_start);
            jMeasurementResultPerStream["bytes_total"] = to_string(bytes_slow_start + bytes);
            jMeasurementResultPerStream["relative_time_ns"] = to_string(long(current_step) * 500000000);
            jMeasurementResultPerStream["relative_time_ns_distinct"] = to_string(500000000);
            jMeasurementResultPerStream["relative_time_ns_total"] = to_string((relative_time_ns_total * 1000) - (long((max_steps - current_step)) * 500000000));
            jMeasurementResultPerStream["relative_time_ns_measurement_start"] = to_string((current_timestamp * 1000) - ::TIMESTAMP_MEASUREMENT_START - (long((max_steps - current_step)) * 500000000));

            jMeasurementResultsPerStream.push_back(jMeasurementResultPerStream);

            bytes_aggregate += stoll(jMeasurementResultPerStream["bytes"].string_value());
            bytes_distinct_aggregate += stoll(jMeasurementResultPerStream["bytes_distinct"].string_value());
            bytes_slow_start_aggregate += stoll(jMeasurementResultPerStream["bytes_slow_start"].string_value());
            bytes_total_aggregate += stoll(jMeasurementResultPerStream["bytes_total"].string_value());
            duration_ns_total = stoll(jMeasurementResultPerStream["relative_time_ns_total"].string_value());
            relative_time_ns_measurement_start = stoll(jMeasurementResultPerStream["relative_time_ns_measurement_start"].string_value());

            streamId++;

            pthread_mutex_unlock(&mutex1);
        }

        if (measurement_aggregate.streams == 0)
        {
            ::hasError = true;
            ::STREAMS_DROPPED = true;
        }

        if (conf.sTestName.compare("download") == 0)
        {
            measurement_aggregate.download.datasize = bytes_aggregate * 8;
            measurement_aggregate.download.datasize_total = bytes_total_aggregate * 8;
            measurement_aggregate.download.datasize_slow_start = bytes_slow_start_aggregate * 8;
            measurement_aggregate.download.totaltime = duration_ns_total;

            CTool::calculateResults(measurement_aggregate.download, 0.5, 0, 0);

            measurement_aggregate.download.measurement_phase_progress =
                (measurement_aggregate.download.duration_ns -
                 (measurement_aggregate.download.duration_ns % 500000000)) /
                (MEASUREMENT_DURATION * 1e9);

            Json::object jMeasurementResult = getMeasurementResultsAggregate(measurement_aggregate, measurement_aggregate.download, cmd);

            jMeasurementResult["bytes_distinct"] = to_string(bytes_distinct_aggregate);
            jMeasurementResult["relative_time_ns_measurement_start"] = to_string(relative_time_ns_measurement_start);

            jMeasurementResults.push_back(jMeasurementResult);
        }
        else if (conf.sTestName.compare("upload") == 0)
        {
            measurement_aggregate.upload.datasize = bytes_aggregate * 8;
            measurement_aggregate.upload.datasize_total = bytes_total_aggregate * 8;
            measurement_aggregate.upload.datasize_slow_start = bytes_slow_start_aggregate * 8;
            measurement_aggregate.upload.totaltime = duration_ns_total;

            CTool::calculateResults(measurement_aggregate.upload, 0.5, 0, 0);

            measurement_aggregate.upload.measurement_phase_progress = (measurement_aggregate.upload.duration_ns - (measurement_aggregate.upload.duration_ns % 500000000)) / ((MEASUREMENT_DURATION - UPLOAD_ADDITIONAL_MEASUREMENT_DURATION) * 1e9);

            Json::object jMeasurementResult = getMeasurementResultsAggregate(measurement_aggregate, measurement_aggregate.upload, cmd);

            jMeasurementResult["bytes_distinct"] = to_string(bytes_distinct_aggregate);
            jMeasurementResult["relative_time_ns_measurement_start"] = to_string(relative_time_ns_measurement_start);

            jMeasurementResults.push_back(jMeasurementResult);
        }
    }

    Json jMeasurementResultsLast = jMeasurementResults[jMeasurementResults.size() - 1];

    if (conf.sTestName.compare("download") == 0)
    {

        jMeasurementResultsDownload = jMeasurementResults;
        jMeasurementResultsDownloadStreams = jMeasurementResultsPerStream;

        TRC_INFO("DOWNLOAD AVG MBPS: " + CTool::to_string_precision(stoll(jMeasurementResultsLast["throughput_avg_bps"].string_value()) * 1e-6, 3));
    }
    else if (conf.sTestName.compare("upload") == 0)
    {
        jMeasurementResultsUpload = jMeasurementResults;
        jMeasurementResultsUploadStreams = jMeasurementResultsPerStream;

        TRC_INFO("UPLOAD AVG MBPS: " + CTool::to_string_precision(stoll(jMeasurementResultsLast["throughput_avg_bps"].string_value()) * 1e-6, 3));
    }
}

Json::object CCallback::getMeasurementResultsAggregate(struct measurement tempMeasurement, struct measurement_data data, string cmd)
{
    Json::object jMeasurementResults;
    jMeasurementResults["bytes"] = to_string(data.datasize / 8);
    jMeasurementResults["bytes_slow_start"] = to_string(data.datasize_slow_start / 8);
    jMeasurementResults["bytes_total"] = to_string(data.datasize_total / 8);
    jMeasurementResults["duration_ns"] = to_string(data.duration_ns);
    jMeasurementResults["duration_ns_distinct"] = "500000000";
    jMeasurementResults["duration_ns_total"] = to_string(data.totaltime);
    jMeasurementResults["progress"] = data.measurement_phase_progress;
    jMeasurementResults["throughput_avg_bps"] = to_string(data.avg);
    jMeasurementResults["num_streams_start"] = to_string(conf.instances);

    if (cmd.compare("finish") == 0 && data.measurement_phase_progress == 1)
    {
        jMeasurementResults["num_streams_end"] = to_string(tempMeasurement.streams);
    }
    else
    {
        jMeasurementResults["num_streams_end"] = "0";
    }

    return jMeasurementResults;
}
