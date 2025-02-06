/*
    Copyright (C) 2016-2025 zafaco GmbH
    Copyright (C) 2019 alladin-IT GmbH

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

#ifndef CPP_CLIENT_ANDROID_INTERFACE_H
#define CPP_CLIENT_ANDROID_INTERFACE_H

#include <string>
#include <vector>
#include <map>
#include <jni.h>
#include <iostream>
#include <atomic>
#include "../ias-libtool/json11.hpp"
#include "type.h"

extern void measurementStart(std::string measurementParameters);

extern void measurementStop();

extern void startTestCase(int nTestCase);

extern void shutdown();

extern void show_usage(char *argv0);

extern bool _DEBUG_;
extern bool RUNNING;

extern const char *PLATFORM;
extern const char *CLIENT_OS;

extern unsigned long long TCP_STARTUP;

extern bool RTT;
extern bool DOWNLOAD;
extern bool UPLOAD;

extern bool TIMER_ACTIVE;
extern bool TIMER_RUNNING;
extern bool TIMER_STOPPED;

extern std::atomic_bool hasError;

extern int TIMER_INDEX;
extern int TIMER_INDEX_UPLOAD_FIRST;
extern int TIMER_DURATION;
extern unsigned long long MEASUREMENT_DURATION;
extern long long TIMESTAMP_MEASUREMENT_START;

extern struct conf_data conf;
extern struct measurement measurements;

extern std::vector<char> randomDataValues;

extern pthread_mutex_t mutex1;
extern std::map<int, int> syncing_threads;
extern pthread_mutex_t mutex_syncing_threads;
extern pthread_mutex_t mutex_timer_index_upload_first;

extern class CConfigManager *pConfig;

extern class CConfigManager *pXml;

extern class CConfigManager *pService;

extern class CCallback *pCallback;

extern class CMeasurement *pMeasurement;

extern MeasurementPhase currentTestPhase;

extern std::function<void(int)> signalFunction;

class AndroidConnector
{

public:
    static AndroidConnector &getInstance()
    {
        static AndroidConnector instance;
        return instance;
    }

    static const char *TAG;

    static void detachCurrentThreadFromJavaVM()
    {
        getInstance().detachCurrentThreadFromJvm();
    }

    void registerSharedObject(
        JNIEnv *env, jobject caller, jobject baseMeasurementState, jobject pingMeasurementState,
        jobject downloadMeasurementState, jobject uploadMeasurementState);

    void setSpeedSettings(JNIEnv *env, jobject speedTaskDesc);

    void unregisterSharedObject();

    void startMeasurement();

    void stopMeasurement();

    jint jniLoad(JavaVM *vm);

    void callback(json11::Json::object &message) const;

    void callbackFinished(json11::Json::object &message);

    void printLog(std::string const &category, std::string const &message) const;

    inline void detachCurrentThreadFromJvm() const
    {
        javaVM->DetachCurrentThread();
    }

    AndroidConnector(AndroidConnector const &) = delete;

    void operator=(AndroidConnector const &) = delete;

    AndroidConnector(AndroidConnector const &&) = delete;

    void operator=(AndroidConnector const &&) = delete;

private:
    struct JavaParseInformation
    {
        jclass longClass;
        jmethodID staticLongValueOf;

        jclass intClass;
        jmethodID staticIntValueOf;

        jclass floatClass;
        jmethodID staticFloatValueOf;
    };

    struct SpeedTaskDesc
    {
        std::vector<std::string> measurementServerUrl;
        int rttCount;
        int downloadStreams;
        int uploadStreams;
        int speedServerPort;
        int speedServerPortRtt;
        bool performDownload;
        bool performUpload;
        bool performRtt;
        bool isEncrypted;
    } speedTaskDesc;

    JavaVM *javaVM;
    jclass jniHelperClass;
    jobject jniCaller;

    jclass jniExceptionClass;

    jmethodID callbackID;
    jmethodID cppCallbackFinishedID;
    jmethodID setMeasurementPhaseByStringValueID;

    jobject baseMeasurementState;
    jobject downloadMeasurementState;
    jobject uploadMeasurementState;
    jobject pingMeasurementState;

    jfieldID fieldProgress;

    jfieldID fieldAvgThroughput;

    jfieldID fieldAverageMs;

    jclass speedMeasurementResultClazz;
    jclass resultUdpClazz;
    jclass resultBandwidthClazz;
    jclass timeClazz;

    std::string measurementServerIp;

    std::vector<std::string> pendingErrorMessages;

    AndroidConnector() {};

    void setBandwidthResult(
        JNIEnv *env, json11::Json const &jsonItems, jobject &result,
        JavaParseInformation &parseInfo);

    inline JNIEnv *getJniEnv() const
    {
        if (javaVM != nullptr)
        {
            JNIEnv *env;
            jint err = javaVM->GetEnv((void **)&env, JNI_VERSION_1_6);
            if (err == JNI_EDETACHED)
            {
                if (javaVM->AttachCurrentThread(&env, NULL) != 0)
                {
                    return nullptr;
                }
            }
            else if (err != JNI_OK)
            {
                return nullptr;
            }
            return env;
        }
        return nullptr;
    }

    void passJniSpeedState(
        JNIEnv *env, const MeasurementPhase &speedStateToSet, const json11::Json &json) const;

    static std::string getStringForMeasurementPhase(const MeasurementPhase &phase)
    {
        switch (phase)
        {
        case MeasurementPhase::INIT:
            return "INIT";
        case MeasurementPhase::IP:
            return "IP";
        case MeasurementPhase::PING:
            return "PING";
        case MeasurementPhase::DOWNLOAD:
            return "DOWNLOAD";
        case MeasurementPhase::UPLOAD:
            return "UPLOAD";
        case MeasurementPhase::END:
            return "END";
        }
    }
};

#endif
