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

#include "upload.h"

Upload::Upload()
{
}

Upload::~Upload()
{
}

Upload::Upload(
    CConfigManager *pConfig, CConfigManager *pXml, CConfigManager *pService, string sProvider)
{
    mServerName = pXml->readString(sProvider, "DNS_HOSTNAME", "example.com");
    mServer = pXml->readString(sProvider, "IP", "1.1.1.1");

    mClient = "0.0.0.0";

    mPort = pXml->readLong(sProvider, "PORT", 443);
    mTls = pXml->readLong(sProvider, "TLS", 1);

    mConfig = pConfig;

    mUploadString = "POST";
}

int Upload::run()
{
    std::unique_ptr<CConnection> mConnection = std::make_unique<CConnection>();
    try
    {
        bool ipv6validated = false;
        bool bReachable = true;

        CTool::getPid(pid);

        TRC_INFO(("Starting Upload Thread with PID: " + CTool::toString(pid)).c_str());

        TRC_DEBUG(("Resolving Hostname for Measurement: " + mServerName).c_str());

#if defined(__ANDROID__)
        bReachable = false;
#endif

        struct addrinfo *ips;
        memset(&ips, 0, sizeof ips);

        ips = CTool::getIpsFromHostname(mServerName, bReachable);

        if (ips == nullptr || (ips->ai_socktype != 1 && ips->ai_socktype != 2))
        {

            ::UNREACHABLE = true;
            ::hasError = true;
            TRC_ERR("no connection to measurement peer");
            return -1;
        }

        char host[NI_MAXHOST];

        getnameinfo(ips->ai_addr, ips->ai_addrlen, host, sizeof host, NULL, 0, NI_NUMERICHOST);
        mServer = string(host);

        if (CTool::validateIp(mServer) == 6)
            ipv6validated = true;

        TRC_DEBUG(("Resolved Hostname for Measurement: " + mServer).c_str());

        measurementTimeStart = 0;
        measurementTimeEnd = 0;
        measurementTimeDuration = 0;

        measurementTimeStart = CTool::get_timestamp();

        pthread_mutex_lock(&mutex_syncing_threads);
        syncing_threads[pid] = 0;
        pthread_mutex_unlock(&mutex_syncing_threads);

        vector<string> vResponse;
        string vDelimiter = ";";
        vector<string> vResponseAtomic;
        string vDelimiterAtomic = ",";

        std::unique_ptr<char[]> rbufferOwner = std::make_unique<char[]>(MAX_PACKET_SIZE);

        char *rbuffer = rbufferOwner.get();

        nDataRecv = 0;
        nTimeRecv = 0;

        nTimeRecvExa = 0;
        nTimeRecvFirst = 0;

        nHttpResponseDuration = 0;
        nHttpResponseReportValue = 0;

        if (ipv6validated)
        {

            if (mConnection->tcp6Socket(mClient, mServer, mPort, mTls, mServerName) < 0)
            {

                ::UNREACHABLE = true;
                ::hasError = true;
                TRC_ERR("no connection to measurement peer: " + mServer);
                return -1;
            }
        }
        else
        {

            if (mConnection->tcpSocket(mClient, mServer, mPort, mTls, mServerName) < 0)
            {

                ::UNREACHABLE = true;
                ::hasError = true;
                TRC_ERR("no connection to measurement peer: " + mServer);
                return -1;
            }
        }

        timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        setsockopt(mConnection->mSocket, SOL_SOCKET, SO_SNDTIMEO, (timeval *)&tv, sizeof(timeval));
        setsockopt(mConnection->mSocket, SOL_SOCKET, SO_RCVTIMEO, (timeval *)&tv, sizeof(timeval));

        std::unique_ptr<CHttp> pHttp = std::make_unique<CHttp>(mConfig, mConnection.get(), mUploadString);

        int response = pHttp->requestToReferenceServer("/data.img");
        if (response < 0)
        {
            if (response == -1)
            {
                ::UNREACHABLE = true;
                TRC_ERR("no connection to measurement peer: " + mServer);
            }
            if (response == -2)
            {
                ::FORBIDDEN = true;
                TRC_ERR("forbidden on peer: " + mServer);
            }

            ::hasError = true;
            mConnection->close();

            TRC_DEBUG(("Ending Upload Thread with PID: " + CTool::toString(pid)).c_str());

            return 0;
        }

        nHttpResponseDuration = pHttp->getHttpResponseDuration();
        std::string mServerHostname = pHttp->getHttpServerHostname();

        std::unique_ptr<CUploadSender> pUploadSender = std::make_unique<CUploadSender>(
            mConnection.get());
        pUploadSender->createThread();

        mUpload.datasize_total = 0;
        mUpload.datasize_slow_start = 0;

        bool dataReceived = false;

        while (RUNNING && TIMER_ACTIVE && !TIMER_STOPPED)
        {

            bzero(rbuffer, MAX_PACKET_SIZE);

            vResponse.clear();

            mResponse = mConnection->receive(rbuffer, MAX_PACKET_SIZE, 0);

            if (!dataReceived)
            {

                pthread_mutex_lock(&mutex_syncing_threads);
                syncing_threads[pid] = 1;
                pthread_mutex_unlock(&mutex_syncing_threads);

                dataReceived = true;
            }

            if (mResponse == -1 || mResponse == 0)
            {

                TRC_ERR("Received an Error: Upload RECV == " + std::to_string(errno) + " error num: " + std::to_string(errno));

                break;
            }

            string sResponse(rbuffer);

            CTool::tokenize(sResponse, vResponse, vDelimiter);

            for (auto &response : vResponse)
            {
                vResponseAtomic.clear();

                CTool::tokenize(response, vResponseAtomic, vDelimiterAtomic);

                if (vResponseAtomic.size() > 2)
                {

                    nDataRecv = CTool::toULL(vResponseAtomic[0]);
                    nTimeRecv = CTool::toULL(vResponseAtomic[2]);
                    nTimeRecvExa = CTool::toULL(vResponseAtomic[3].substr(vResponseAtomic[3].length() - 6));
                    nDataRecv = nDataRecv * 8;
                    mUpload.datasize_total += nDataRecv;
                }

                if (TIMER_RUNNING && ::TIMER_INDEX_UPLOAD_FIRST == 0)
                {
                    pthread_mutex_lock(&mutex_timer_index_upload_first);

                    if (::TIMER_INDEX_UPLOAD_FIRST == 0)
                    {
                        ::TIMER_INDEX_UPLOAD_FIRST = nTimeRecvExa;
                    }
                    pthread_mutex_unlock(&mutex_timer_index_upload_first);
                }

                if (::TIMER_INDEX_UPLOAD_FIRST != 0 && ::TIMER_INDEX_UPLOAD_FIRST == nTimeRecvExa)
                {
                    for (int i = ::TIMER_INDEX_UPLOAD_FIRST; i <= ::TIMER_INDEX_UPLOAD_FIRST + (MEASUREMENT_DURATION * 2 * 5); i = i + 5)
                    {
                        mUpload.results[i] = 0;
                    }
                }

                if (!::hasError)
                {
                    if (mUpload.results.find(nTimeRecvExa) == mUpload.results.end())
                    {
                        mUpload.results[nTimeRecvExa] = nDataRecv;
                    }
                    else
                    {
                        mUpload.results[nTimeRecvExa] += nDataRecv;

                        if (mUpload.datasize_slow_start == 0 && ::TIMER_INDEX_UPLOAD_FIRST == nTimeRecvExa)
                        {
                            mUpload.datasize_slow_start = mUpload.datasize_total - nDataRecv;
                        }
                    }
                }
            }
        }

        pUploadSender->stopThread();

        pUploadSender->waitForEnd();
    }
    catch (std::exception &ex)
    {
        TRC_ERR("Exception in upload: ");
        TRC_ERR(ex.what());
        ::hasError = true;
        ::RUNNING = false;
        ::recentException = ex;
    }

    mConnection->close();

    TRC_DEBUG(("Ending Upload Thread with PID: " + CTool::toString(pid)).c_str());

    return 0;
}
