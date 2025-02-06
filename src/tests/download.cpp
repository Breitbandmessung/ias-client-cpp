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

#include "download.h"

Download::Download()
{
}

Download::~Download()
{
}

Download::Download(
    CConfigManager *pConfig, CConfigManager *pXml, CConfigManager *pService, string sProvider,
    int nTlsOverhead)
{
    mServerName = pXml->readString(sProvider, "DNS_HOSTNAME", "example.com");
    mServer = pXml->readString(sProvider, "IP", "1.1.1.1");

    mClient = "0.0.0.0";

    mPort = pXml->readLong(sProvider, "PORT", 443);
    mTls = pXml->readLong(sProvider, "TLS", 1);

    mTlsOverhead = nTlsOverhead;

    mConfig = pConfig;

    mDownloadString = "GET";
}

int Download::run()
{

    std::unique_ptr<CConnection> mConnection = std::make_unique<CConnection>();

    try
    {
        bool ipv6validated = false;
        bool bReachable = true;

        CTool::getPid(pid);

        TRC_INFO(("Starting Download Thread with PID: " + CTool::toString(pid)).c_str());

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

        std::unique_ptr<char[]> rbufferOwner = std::make_unique<char[]>(MAX_PACKET_SIZE);

        char *rbuffer = rbufferOwner.get();

        nHttpResponseDuration = 0;
        nHttpResponseReportValue = 0;

        if (ipv6validated)
        {
            if ((mConnection->tcp6Socket(mClient, mServer, mPort, mTls, mServerName)) < 0)
            {
                ::UNREACHABLE = true;
                ::hasError = true;
                TRC_ERR("no connection to measurement peer: " + mServer);
                return -1;
            }
        }
        else
        {
            if ((mConnection->tcpSocket(mClient, mServer, mPort, mTls, mServerName)) < 0)
            {
                ::UNREACHABLE = true;
                ::hasError = true;
                TRC_ERR("no connection to measurement peer: " + mServer);
                return -1;
            }
        }

        bzero(rbuffer, MAX_PACKET_SIZE);

        timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        setsockopt(mConnection->mSocket, SOL_SOCKET, SO_SNDTIMEO, (timeval *)&tv, sizeof(timeval));
        setsockopt(mConnection->mSocket, SOL_SOCKET, SO_RCVTIMEO, (timeval *)&tv, sizeof(timeval));

        std::unique_ptr<CHttp> pHttp = std::make_unique<CHttp>(mConfig, mConnection.get(), mDownloadString);

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

            TRC_DEBUG(("Ending Download Thread with PID: " + CTool::toString(pid)).c_str());

            return 0;
        }

        nHttpResponseDuration = pHttp->getHttpResponseDuration();
        std::string mServerHostname = pHttp->getHttpServerHostname();

        mDownload.datasize_total = 0;
        mDownload.datasize_slow_start = 0;

        for (int i = 0; i <= MEASUREMENT_DURATION * 2; i++)
        {
            mDownload.results[i] = 0;
        }

        bool dataReceived = false;

        while (RUNNING && TIMER_ACTIVE && !TIMER_STOPPED && !m_fStop)
        {
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
                TRC_ERR("Received an Error: Download RECV == " + std::to_string(mResponse) + " error num: " + std::to_string(errno));
                break;
            }

            bzero(rbuffer, MAX_PACKET_SIZE);

            mResponse += (mConnection->mTlsRecordsReceived * mTlsOverhead);
            mConnection->mTlsRecordsReceived = 0;

            mResponse = mResponse * 8;
            mDownload.datasize_total += mResponse;

            if (TIMER_RUNNING && !hasError)
            {
                mDownload.results[TIMER_INDEX] += mResponse;

                if (mDownload.datasize_slow_start == 0 && TIMER_INDEX == 0)
                {
                    mDownload.datasize_slow_start = mDownload.datasize_total - mResponse;
                }
            }
        }
    }
    catch (std::exception &ex)
    {
        TRC_ERR("Exception in download");
        TRC_ERR(ex.what());
        ::hasError = true;
        ::RUNNING = false;
        ::recentException = ex;
    }

    mConnection->close();

    TRC_DEBUG(("Ending Download Thread with PID: " + CTool::toString(pid)).c_str());

    return 0;
}
