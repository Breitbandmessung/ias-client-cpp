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
#include "callback.h"
#include "timer.h"
#include "measurement.h"

bool _DEBUG_;
bool RUNNING;
bool UNREACHABLE;
bool STREAMS_DROPPED;
bool FORBIDDEN;
bool TLS_PARAMETERS_MISSING;
const char *PLATFORM;
const char *CLIENT_OS;

unsigned long long TCP_STARTUP;

bool IP;
bool RTT;
bool DOWNLOAD;
bool UPLOAD;

std::atomic_bool hasError;
std::exception recentException;

bool TIMER_ACTIVE;
bool TIMER_RUNNING;
bool TIMER_STOPPED;

int TIMER_INDEX;
int TIMER_INDEX_UPLOAD_FIRST;
int TIMER_DURATION;
unsigned long long MEASUREMENT_DURATION;
long long TIMESTAMP_MEASUREMENT_START;

bool PERFORMED_IP;
bool PERFORMED_RTT;
bool PERFORMED_DOWNLOAD;
bool PERFORMED_UPLOAD;

struct conf_data conf;
struct measurement measurements;

vector<char> randomDataValues;

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;

map<int, int> syncing_threads;
pthread_mutex_t mutex_syncing_threads = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_timer_index_upload_first = PTHREAD_MUTEX_INITIALIZER;

std::unique_ptr<CConfigManager> pConfig;
std::unique_ptr<CConfigManager> pXml;
std::unique_ptr<CConfigManager> pService;

std::unique_ptr<CCallback> pCallback;

MeasurementPhase currentTestPhase = MeasurementPhase::INIT;

std::function<void(int)> signalFunction = nullptr;

void show_usage(char *argv0);

void measurementStart(string measurementParameters);

void measurementStop();

void startTestCase(int nTestCase);

void shutdown();

static void signal_handler(int signal);

#ifndef IAS_IOS

int main(int argc, char **argv)
{
    ::_DEBUG_ = false;
    ::RUNNING = true;

    ::RTT = false;
    ::DOWNLOAD = false;
    ::UPLOAD = false;

    long int opt;
    int tls = 0;
    string target_port = "80";

    while ((opt = getopt(argc, argv, "rdutp:nhva:m:")) != -1)
    {
        switch (opt)
        {
        case 'r':
            ::RTT = true;
            break;
        case 'd':
            ::DOWNLOAD = true;
            break;
        case 'u':
            ::UPLOAD = true;
            break;
        case 't':
            tls = 1;
            break;
        case 'p':
            if (optarg == NULL)
            {
                show_usage(argv[0]);
                return EXIT_SUCCESS;
            }
            target_port = optarg;
            break;
        case 'n':
            ::_DEBUG_ = true;
            break;
        case 'h':
            show_usage(argv[0]);
            return EXIT_SUCCESS;
        case 'v':
            cout << "ias-client" << endl;
            cout << "Version: " << VERSION << endl;
            return 0;
        case '?':
        default:
            printf("Error: Unknown Argument -%c\n", optopt);
            show_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (!::RTT && !::DOWNLOAD && !::UPLOAD)
    {
        printf("Error: At least one test case is required");
        show_usage(argv[0]);
        return EXIT_FAILURE;
    }

    Json::object jRttParameters;
    Json::object jDownloadParameters;
    Json::object jUploadParameters;

    Json::object jMeasurementParameters;

    jRttParameters["performMeasurement"] = ::RTT;
    jDownloadParameters["performMeasurement"] = ::DOWNLOAD;
    jUploadParameters["performMeasurement"] = ::UPLOAD;

    jDownloadParameters["streams"] = "4";
    jUploadParameters["streams"] = "4";
    jMeasurementParameters["rtt"] = Json(jRttParameters);
    jMeasurementParameters["download"] = Json(jDownloadParameters);
    jMeasurementParameters["upload"] = Json(jUploadParameters);

#if defined(__APPLE__) && defined(TARGET_OS_MAC)
    jMeasurementParameters["clientos"] = "macos";
#else
    jMeasurementParameters["clientos"] = "linux";
#endif

    jMeasurementParameters["platform"] = "desktop";
    jMeasurementParameters["wsTargetPort"] = target_port;
    jMeasurementParameters["wsTargetPortRtt"] = "80";
    jMeasurementParameters["wsWss"] = to_string(tls);

    Json::array jTargets;
    jTargets.push_back("peer.example.com");
    jMeasurementParameters["wsTargets"] = Json(jTargets);

    Json jMeasurementParametersJson = jMeasurementParameters;

#ifdef IAS_CLIENT

    CTrace::setLogFunction([](std::string const &cat, std::string const &s)
                           { std::cout << "[" + CTool::get_timestamp_string() + "] " + cat + ": " + s + "\n"; });
#endif

    measurementStart(jMeasurementParametersJson.dump());
}

#endif

void measurementStart(string measurementParameters)
{
    signal(SIGFPE, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGSEGV, signal_handler);
    signal(SIGCHLD, signal_handler);
    signal(SIGPIPE, signal_handler);

    ::PERFORMED_IP = false;
    ::PERFORMED_RTT = false;
    ::PERFORMED_DOWNLOAD = false;
    ::PERFORMED_UPLOAD = false;
    ::hasError = false;

    ::UNREACHABLE = false;
    ::STREAMS_DROPPED = false;
    ::FORBIDDEN = false;
    ::TLS_PARAMETERS_MISSING = false;

    ::TCP_STARTUP = 3000000;
    conf.sProvider = "ias";

    Json jMeasurementParameters = Json::object{};
    string error = "";
    jMeasurementParameters = Json::parse(measurementParameters, error);
    Json::object jMeasurementParametersMutable;
    jMeasurementParametersMutable = jMeasurementParameters.object_items();

    if (error.compare("") != 0)
    {
        TRC_ERR("JSON parameter parse failed");
        shutdown();
    }

    ::PLATFORM = jMeasurementParametersMutable["platform"].string_value().c_str();
    ::CLIENT_OS = jMeasurementParametersMutable["clientos"].string_value().c_str();

    TRC_INFO("Status: ias-client started");

    srand(CTool::get_timestamp());

    pConfig = std::make_unique<CConfigManager>();
    pXml = std::make_unique<CConfigManager>();
    pService = std::make_unique<CConfigManager>();

    Json::array jTargets = jMeasurementParametersMutable["wsTargets"].array_items();
    pXml->writeString(conf.sProvider, "DNS_HOSTNAME", jTargets[rand() % jTargets.size()].string_value());
    jMeasurementParametersMutable["wsTarget"] = pXml->readString(conf.sProvider, "DNS_HOSTNAME", "example.com");

    char sbuffer[64];
    memset(sbuffer, 0, sizeof(sbuffer));
    CTool::randomData(sbuffer, 64);

    pXml->writeString(conf.sProvider, "PORT", jMeasurementParametersMutable["wsTargetPort"].string_value());
    pXml->writeString(conf.sProvider, "PING_PORT", jMeasurementParametersMutable["wsTargetPortRtt"].string_value());

    pXml->writeString(conf.sProvider, "TLS", jMeasurementParametersMutable["wsWss"].string_value());

    if (jMeasurementParametersMutable["wsWss"].string_value() == "1")
    {
        TRC_INFO("Note: All Values account for Overhead added by TLS Framing");
    }

    Json::object jRtt = jMeasurementParametersMutable["rtt"].object_items();
    ::RTT = jRtt["performMeasurement"].bool_value();

    Json::object jDownload = jMeasurementParametersMutable["download"].object_items();
    ::DOWNLOAD = jDownload["performMeasurement"].bool_value();
    pXml->writeString(conf.sProvider, "DL_STREAMS", jDownload["streams"].string_value());

    Json::object jUpload = jMeasurementParametersMutable["upload"].object_items();
    ::UPLOAD = jUpload["performMeasurement"].bool_value();
    pXml->writeString(conf.sProvider, "UL_STREAMS", jUpload["streams"].string_value());

#if defined(__ANDROID__)
    pXml->writeString(conf.sProvider, "PING_QUERY", jRtt["ping_query"].string_value());
#else
    pXml->writeString(conf.sProvider, "PING_QUERY", "10");
#endif

    ::IP = true;

    pCallback = std::make_unique<CCallback>(jMeasurementParametersMutable);

    if (!::RTT && !::DOWNLOAD && !::UPLOAD)
    {
        pCallback->callback("error", "no test case enabled", 1, "no test case enabled");

        shutdown();
    }

    if (::IP && ::RUNNING)
    {
        usleep(500000);

        conf.nTestCase = 1;
        conf.sTestName = "ip";
        TRC_INFO(("Taking Testcase IP (" + CTool::toString(conf.nTestCase) + ")").c_str());
        currentTestPhase = MeasurementPhase::IP;
        startTestCase(conf.nTestCase);
    }

    if (::RTT && ::RUNNING && !hasError)
    {
        usleep(500000);

        conf.nTestCase = 2;
        conf.sTestName = "rtt_udp";
        TRC_INFO(("Taking Testcase RTT UDP (" + CTool::toString(conf.nTestCase) + ")").c_str());
        currentTestPhase = MeasurementPhase::PING;
        startTestCase(conf.nTestCase);
    }

    if (::DOWNLOAD && ::RUNNING && !hasError)
    {
        usleep(500000);

        conf.nTestCase = 3;
        conf.sTestName = "download";
        TRC_INFO(("Taking Testcase DOWNLOAD (" + CTool::toString(conf.nTestCase) + ")").c_str());
        currentTestPhase = MeasurementPhase::DOWNLOAD;
        startTestCase(conf.nTestCase);
    }

    if (::UPLOAD && ::RUNNING && !hasError)
    {
        usleep(500000);

        CTool::randomData(randomDataValues, 1123457 * 10);
        conf.nTestCase = 4;
        conf.sTestName = "upload";
        TRC_INFO(("Taking Testcase UPLOAD (" + CTool::toString(conf.nTestCase) + ")").c_str());
        currentTestPhase = MeasurementPhase::UPLOAD;
        startTestCase(conf.nTestCase);
    }

    currentTestPhase = MeasurementPhase::END;

    shutdown();
}

void measurementStop()
{
    shutdown();
}

void startTestCase(int nTestCase)
{
    pthread_mutex_lock(&mutex_syncing_threads);
    syncing_threads.clear();
    pthread_mutex_unlock(&mutex_syncing_threads);
    pCallback->mTestCase = nTestCase;
#if defined(__ANDROID__) || defined(IAS_IOS)

    pCallback->callbackToPlatform("started", "", 0, "");
#endif
    std::unique_ptr<CMeasurement> pMeasurement = std::make_unique<CMeasurement>(pConfig.get(), pXml.get(), pService.get(), conf.sProvider, nTestCase, pCallback.get());
    pMeasurement->startMeasurement();
}

void shutdown()
{
    usleep(1000000);

    ::RUNNING = false;

    TRC_INFO("Status: ias-client stopped");

#if !defined(__ANDROID__) && !defined(IAS_IOS)
    exit(EXIT_SUCCESS);
#endif
}

void show_usage(char *argv0)
{
    cout << "                                                 				" << endl;
    cout << "Usage: " << argv0 << " [ options ... ]           				" << endl;
    cout << "                                                 				" << endl;
    cout << "  -r             - Perform RTT measurement       				" << endl;
    cout << "  -d             - Perform Download measurement  				" << endl;
    cout << "  -u             - Perform Upload measurement    				" << endl;
    cout << "  -p port        - Target Port to use for TCP Connections		" << endl;
    cout << "  -t             - Enable TLS for TCP Connections               " << endl;
#if !defined(__APPLE__)
    cout << "  -n             - Show debugging output	 	 				" << endl;
#endif
    cout << "  -h             - Show Help                     				" << endl;
    cout << "  -v             - Show Version                  				" << endl;
    cout << "                                                 				" << endl;

    exit(EXIT_FAILURE);
}

static void signal_handler(int signal)
{
    TRC_ERR("Error signal received: " + std::to_string(signal));

    if (signalFunction != nullptr)
    {
        signalFunction(signal);
    }

#if !defined(__APPLE__) && !defined(__ANDROID__)
    hasError = true;
    ::RUNNING = false;

    CTool::print_stacktrace();

    sleep(1);
    exit(signal);
#endif
}
