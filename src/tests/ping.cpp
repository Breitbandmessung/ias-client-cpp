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

#include "ping.h"

Ping::Ping()
{
}

Ping::~Ping()
{
}

Ping::Ping(CConfigManager *pXml, CConfigManager *pService, string sProvider)
{
    mServerName = pXml->readString(sProvider, "DNS_HOSTNAME", "example.com");
    mServer = pXml->readString(sProvider, "PING_DESTINATION", "1.1.1.1");

    mClient = "0.0.0.0";

    mPingQuery = pXml->readLong(sProvider, "PING_QUERY", 30);
    mPingQuery++;
    nPingTarget = mPingQuery - 1;
    mPingPort = pXml->readLong(sProvider, "PING_PORT", 80);

    mTimeDiff = 1;
}

int Ping::run()
{
    int mSock = 0;

    try
    {
        bool ipv6validated = false;
        bool bReachable = true;

        CTool::getPid(pid);

        TRC_INFO(("Starting Ping Thread with PID: " + CTool::toString(pid)).c_str());

        std::unique_ptr<CConnection> mSocket = std::make_unique<CConnection>();

        measurementTimeStart = 0;
        measurementTimeEnd = 0;
        measurementTimeDuration = 0;

        measurementTimeStart = CTool::get_timestamp();

        pthread_mutex_lock(&mutex_syncing_threads);
        syncing_threads[pid] = 0;
        pthread_mutex_unlock(&mutex_syncing_threads);

        char sbuffer[ECHO];
        char rbuffer[ECHO];

        nSize = ECHO;
        nErrors = 0;
        nMissing = 0;
        nReplies = 0;

        int i = 1;

        bool ipv6 = false;
        bool ipv4 = false;

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

        ::MEASUREMENT_DURATION = (int)mPingQuery * 1.5 * 1.1;
        if (CTool::validateIp(mServer) == 6)
            ipv6validated = true;

        if (ipv6validated)
        {
            if ((mSock = mSocket->udp6Socket(mClient)) < 0)
            {
                TRC_ERR("Creating socket failed - socket()");

                return -1;
            }

            ipv4 = false;
            ipv6 = true;
        }
        else
        {
            if ((mSock = mSocket->udpSocket(mClient)) < 0)
            {
                TRC_ERR("Creating socket failed - socket()");

                return -1;
            }

            ipv4 = true;
            ipv6 = false;
        }

        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        setsockopt(mSock, SOL_SOCKET, SO_RCVTIMEO, (timeval *)&tv, sizeof(timeval));
        setsockopt(mSock, SOL_SOCKET, SO_SNDTIMEO, (timeval *)&tv, sizeof(timeval));

        int mResponse;
        unsigned long long time1;
        unsigned long long time2;

        bool dataReceived = false;
        bool firstResponse = true;

        while (RUNNING && i <= mPingQuery)
        {
            mResponse = -1;

            if (!dataReceived)
            {
                pthread_mutex_lock(&mutex_syncing_threads);
                syncing_threads[pid] = 1;
                pthread_mutex_unlock(&mutex_syncing_threads);

                dataReceived = true;
            }

            memset(sbuffer, 0, sizeof(sbuffer));
            memset(rbuffer, 0, sizeof(rbuffer));

            CTool::randomData(sbuffer, ECHO);

            time1 = CTool::get_timestamp();

            struct sockaddr_in6 mClientDatav6;
            struct sockaddr_in mClientDatav4;
            socklen_t mClientDataSizev6;
            socklen_t mClientDataSizev4;

            if (ipv4)
            {
                mClientDataSizev4 = sizeof(mClientDatav4);

                memset(&mClientDatav4, 0, sizeof(mClientDatav4));
                mClientDatav4.sin_family = AF_INET;
                mClientDatav4.sin_port = htons(mPingPort);
                mClientDatav4.sin_addr.s_addr = inet_addr(mServer.c_str());

                sendto(mSock, sbuffer, ECHO, 0, (struct sockaddr *)&mClientDatav4, sizeof(mClientDatav4));

                mResponse = recvfrom(mSock, rbuffer, ECHO, 0, (struct sockaddr *)&mClientDatav4, &mClientDataSizev4);
            }
            if (ipv6)
            {
                mClientDataSizev6 = sizeof(mClientDatav6);

                memset(&mClientDatav6, 0, sizeof(mClientDatav6));
                mClientDatav6.sin6_family = AF_INET6;
                mClientDatav6.sin6_port = htons(mPingPort);
                (void)inet_pton(AF_INET6, mServer.c_str(), mClientDatav6.sin6_addr.s6_addr);

                sendto(mSock, sbuffer, ECHO, 0, (struct sockaddr *)&mClientDatav6, sizeof(mClientDatav6));

                mResponse = recvfrom(mSock, rbuffer, ECHO, 0, (struct sockaddr *)&mClientDatav6, &mClientDataSizev6);
            }

            if (firstResponse)
            {
                firstResponse = false;
            }
            else
            {
                if (mResponse != -1)
                {
                    time2 = CTool::get_timestamp();

                    mTimeDiff = time2 - time1;

                    bool mirroredPayloadVerified = true;
                    for (int i = 0; i < mResponse; i++)
                    {
                        if (static_cast<short int>(rbuffer[i]) != static_cast<short int>(sbuffer[i]))
                        {
                            mirroredPayloadVerified = false;
                        }
                    }

                    if (!mirroredPayloadVerified)
                    {
                        TRC_ERR("Ping payload mismatch");
                        nErrors++;
                    }
                    else
                    {
                        nReplies++;

                        if (mPingResult.results.find(i) == mPingResult.results.end())
                        {
                            mPingResult.results[i] = mTimeDiff;
                        }
                        else
                        {
                            mPingResult.results[i] += mTimeDiff;
                        }
                        mPingResult.results_timestamp[i] =
                            (CTool::get_timestamp() * 1000) - ::TIMESTAMP_MEASUREMENT_START;
                    }
                }
                else
                {
                    nMissing++;
                }
            }

            i++;

            usleep(500000);
        }

        ::TIMER_STOPPED = true;
    }
    catch (std::exception &ex)
    {
        ::hasError = true;
        ::RUNNING = false;
        ::recentException = ex;
    }

    close(mSock);

    TRC_DEBUG(("Ending Ping Thread with PID: " + CTool::toString(pid)).c_str());

    return 0;
}
