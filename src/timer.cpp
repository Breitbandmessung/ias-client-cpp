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

#include "timer.h"

#ifdef __ANDROID__

#include "android_connector.h"

#endif

CTimer::CTimer()
{
}

CTimer::~CTimer()
{
}

CTimer::CTimer(int nInstances, CCallback *pCallback, unsigned long long nInitialCallbackDelay)
{
    mInstances = nInstances;
    mCallback = pCallback;
    unreachableSignaled = false;
    streamsDroppedSignaled = false;
    forbiddenSignaled = false;
    tlsParametersMissingSignaled = false;

    mInitialCallbackDelay = nInitialCallbackDelay;

    ::TIMER_ACTIVE = true;
    ::TIMER_RUNNING = false;
    ::TIMER_STOPPED = false;

    ::TIMER_INDEX = 0;
    ::TIMER_INDEX_UPLOAD_FIRST = 0;
    ::TIMER_DURATION = 0;
    ::MEASUREMENT_DURATION = 0;
}

int CTimer::run()
{
    CTool::getPid(pid);

    TRC_INFO(("Starting Timer Thread with PID: " + CTool::toString(pid)).c_str());

    unsigned long long time1 = 0;
    unsigned long long time2 = 0;
    unsigned long long time_diff = 0;

    map<int, int>::iterator AI;

    int sync_counter;

    while (RUNNING && !TIMER_STOPPED && !m_fStop)
    {
        if (::UNREACHABLE && !unreachableSignaled)
        {
            unreachableSignaled = true;
            mCallback->callback("error", "no connection to measurement peer", 7, "no connection to measurement peer");

            ::TIMER_STOPPED = true;

            break;
        }
        if (::FORBIDDEN && !forbiddenSignaled)
        {
            forbiddenSignaled = true;
            mCallback->callback("error", "forbidden", 4, "forbidden");

            ::TIMER_STOPPED = true;

            break;
        }
        if (::TLS_PARAMETERS_MISSING && !tlsParametersMissingSignaled)
        {
            tlsParametersMissingSignaled = true;
            mCallback->callback("error", "TLS parameters missing", 8, "TLS parameters missing");

            ::TIMER_STOPPED = true;

            break;
        }

        if (!TIMER_RUNNING)
        {
            sync_counter = 0;

            pthread_mutex_lock(&mutex_syncing_threads);
            if (syncing_threads.size() > 0)
            {
                for (AI = syncing_threads.begin(); AI != syncing_threads.end(); ++AI)
                {
                    if ((*AI).second == 1)
                        sync_counter++;
                }
            }
            pthread_mutex_unlock(&mutex_syncing_threads);

            if (sync_counter == mInstances)
            {
                mCallback->callback("info", "starting measurement", 0, "");

                usleep(mInitialCallbackDelay);
                mCallback->callback("info", "measurement started", 0, "");

                TIMER_RUNNING = true;

                time1 = CTool::get_timestamp();
            }
        }
        else
        {
            int current_index = 0;
            do
            {
                if (::STREAMS_DROPPED && !streamsDroppedSignaled)
                {
                    streamsDroppedSignaled = true;
                    mCallback->callback("error", "measurement streams dropped", 9, "measurement streams dropped");

                    ::TIMER_STOPPED = true;

                    break;
                }

                time2 = CTool::get_timestamp();

                time_diff = time2 - time1;
                TIMER_DURATION = time_diff;

                if (MEASUREMENT_DURATION <= (unsigned long long)(time_diff / 1000000))
                {
                    TIMER_STOPPED = true;
                }
                else
                    TIMER_INDEX = (int)(time_diff / 500000);

                if (TIMER_INDEX != current_index)
                {
                    current_index = TIMER_INDEX;

                    mCallback->callback("report", "measurement report", 0, "");
                }

                usleep(100);

            } while (RUNNING && !TIMER_STOPPED && !m_fStop);
        }

        usleep(1000);

        if (m_fStop)
            break;
    }

    if (::RUNNING && !::hasError)
    {
        mCallback->callback("finish", "measurement completed", 0, "");
    }

    TIMER_ACTIVE = false;

    TRC_INFO(("Ending Timer Thread with PID: " + CTool::toString(pid)).c_str());

    return 0;
}
