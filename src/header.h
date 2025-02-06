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

#define VERSION "2.8"

#ifndef HEADER_H
#define HEADER_H

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include <syslog.h>
#include <signal.h>
#include <sched.h>
#include <unistd.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#include <net/if_dl.h>

#ifdef TARGET_OS_IPHONE
#elif TARGET_IPHONE_SIMULATOR
#elif TARGET_OS_MAC
#include <netinet/if_ether.h>
#endif
#else

#include <netinet/ether.h>
#include <netpacket/packet.h>

#endif

#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <exception>
#include <atomic>
#include <chrono>

#include <pthread.h>
#include <fcntl.h>
#include <map>
#include <list>
#include <vector>
#include <getopt.h>
#include <getopt.h>
#include <errno.h>
#include <cxxabi.h>

#ifndef __ANDROID__
#include <execinfo.h>
#include <ifaddrs.h>
#endif

#include "../ias-libtool/json11.hpp"
#include "../ias-libtool/typedef.h"
#include "../ias-libtool/tool.h"
#include "../ias-libtool/connection.h"
#include "../ias-libtool/configmanager.h"
#include "../ias-libtool/basisthread.h"
#include "../ias-libtool/http.h"

#include "type.h"

using namespace std;
using namespace json11;

#define MAX_NUM_THREADS 256
#define ECHO 64
#define UPLOAD_ADDITIONAL_MEASUREMENT_DURATION 2

extern bool _DEBUG_;
extern bool RUNNING;
extern bool UNREACHABLE;
extern bool STREAMS_DROPPED;
extern bool FORBIDDEN;
extern bool TLS_PARAMETERS_MISSING;
extern const char *PLATFORM;
extern const char *CLIENT_OS;

extern unsigned long long TCP_STARTUP;

extern bool IP;
extern bool RTT;
extern bool DOWNLOAD;
extern bool UPLOAD;

extern bool TIMER_ACTIVE;
extern bool TIMER_RUNNING;
extern bool TIMER_STOPPED;

extern std::atomic_bool hasError;
extern std::exception recentException;

extern int TIMER_INDEX;
extern int TIMER_INDEX_UPLOAD_FIRST;
extern int TIMER_DURATION;
extern unsigned long long MEASUREMENT_DURATION;
extern long long TIMESTAMP_MEASUREMENT_START;

extern bool PERFORMED_IP;
extern bool PERFORMED_RTT;
extern bool PERFORMED_DOWNLOAD;
extern bool PERFORMED_UPLOAD;

extern int INSTANCE;

extern vector<char> randomDataValues;

extern struct conf_data conf;
extern struct measurement measurements;

extern pthread_mutex_t mutex1;
extern map<int, int> syncing_threads;
extern pthread_mutex_t mutex_syncing_threads;
extern pthread_mutex_t mutex_timer_index_upload_first;

#endif
