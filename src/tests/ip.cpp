

#include "ip.h"

Ip::Ip()
{
}

Ip::~Ip()
{
}

Ip::Ip(CConfigManager *pConfig, CConfigManager *pXml, CConfigManager *pService, string sProvider)
{
    mServerName = pXml->readString(sProvider, "DNS_HOSTNAME", "example.com");
    mServer = pXml->readString(sProvider, "IP", "1.1.1.1");

    mClient = "0.0.0.0";

    mPort = pXml->readLong(sProvider, "PORT", 443);
    mTls = pXml->readLong(sProvider, "TLS", 1);

    mConfig = pConfig;

    mIpString = "GET";
}

int Ip::run()
{
    std::unique_ptr<CConnection> mConnection = std::make_unique<CConnection>();

    try
    {
        bool ipv6validated = false;
        bool bReachable = true;

        CTool::getPid(pid);

        TRC_INFO(("Starting Ip Thread with PID: " + CTool::toString(pid)).c_str());

        TRC_DEBUG(("Resolving Hostname Ip for Measurement: " + mServerName).c_str());

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
            TRC_ERR("no ip connection to measurement peer");
            return -1;
        }

        char host[NI_MAXHOST];

        getnameinfo(ips->ai_addr, ips->ai_addrlen, host, sizeof host, NULL, 0, NI_NUMERICHOST);
        mServer = string(host);

        if (CTool::validateIp(mServer) == 6)
            ipv6validated = true;

        TRC_DEBUG(("Resolved Hostname Ip for Measurement: " + mServer).c_str());

        measurementTimeStart = 0;
        measurementTimeEnd = 0;
        measurementTimeDuration = 0;

        measurementTimeStart = CTool::get_timestamp();

        pthread_mutex_lock(&mutex_syncing_threads);
        syncing_threads[pid] = 0;
        pthread_mutex_unlock(&mutex_syncing_threads);

        std::unique_ptr<char[]> rbufferOwner = std::make_unique<char[]>(MAX_PACKET_SIZE);

        char *rbuffer = rbufferOwner.get();

        if (ipv6validated)
        {
            if ((mConnection->tcp6Socket(mClient, mServer, mPort, mTls, mServerName)) < 0)
            {
                ::UNREACHABLE = true;
                ::hasError = true;
                TRC_ERR("no ip connection to measurement peer: " + mServer);
                return -1;
            }
        }
        else
        {
            if ((mConnection->tcpSocket(mClient, mServer, mPort, mTls, mServerName)) < 0)
            {
                ::UNREACHABLE = true;
                ::hasError = true;
                TRC_ERR("no ip connection to measurement peer: " + mServer);
                return -1;
            }
        }

        bzero(rbuffer, MAX_PACKET_SIZE);

        timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        setsockopt(mConnection->mSocket, SOL_SOCKET, SO_SNDTIMEO, (timeval *)&tv, sizeof(timeval));
        setsockopt(mConnection->mSocket, SOL_SOCKET, SO_RCVTIMEO, (timeval *)&tv, sizeof(timeval));

        std::unique_ptr<CHttp> pHttp = std::make_unique<CHttp>(mConfig, mConnection.get(), mIpString);

        int response = pHttp->requestToReferenceServer("/ip");
        if (response < 0)
        {
            if (response == -1)
            {
                ::UNREACHABLE = true;
                TRC_ERR("no ip connection to measurement peer: " + mServer);
            }

            ::hasError = true;

            mConnection->close();

            TRC_DEBUG(("Ending Ip Thread with PID: " + CTool::toString(pid)).c_str());

            return 0;
        }

        bool dataReceived = false;

        while (RUNNING && TIMER_ACTIVE && !TIMER_STOPPED && !m_fStop)
        {
            bzero(rbuffer, MAX_PACKET_SIZE);

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
                TRC_ERR("Received an Error: Ip RECV == " + std::to_string(mResponse) + " error num: " + std::to_string(errno));

                break;
            }

            string error;
            Json ipParameters = Json::parse(rbuffer, error);
            if (ipParameters["cmd"].string_value().compare("ip_report") == 0)
            {
                TRC_INFO("Received Ip report: " + ipParameters.dump());
                jMeasurementResultsIp["client"] = ipParameters["client"].string_value();

                if (mTls == 1)
                {
                    if (!ipParameters["tls_version"].string_value().empty() &&
                        !ipParameters["tls_cipher_suite"].string_value().empty() &&
                        !ipParameters["tls_overhead"].string_value().empty() &&
                        CTool::toInt(ipParameters["tls_overhead"].string_value()) > 0)
                    {
                        jMeasurementResultsIp["tls_version"] = ipParameters["tls_version"].string_value();
                        jMeasurementResultsIp["tls_cipher_suite"] = ipParameters["tls_cipher_suite"].string_value();
                        jMeasurementResultsIp["tls_overhead"] = ipParameters["tls_overhead"].string_value();
                    }
                    else
                    {
                        ::TLS_PARAMETERS_MISSING = true;
                        ::hasError = true;
                        TRC_ERR("TLS parameters missing");
                    }
                }

                if (ipv6validated)
                {
                    jMeasurementResultsIp["client_ip_version"] = "v6";
                }
                else
                {
                    jMeasurementResultsIp["client_ip_version"] = "v4";
                }

                break;
            }
        }
    }
    catch (std::exception &ex)
    {
        TRC_ERR("Exception in Ip");
        TRC_ERR(ex.what());
        ::hasError = true;
        ::RUNNING = false;
        ::recentException = ex;
    }

    mConnection->close();

    TRC_DEBUG(("Ending Ip Thread with PID: " + CTool::toString(pid)).c_str());

    return 0;
}
