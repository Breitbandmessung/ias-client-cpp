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

#include "android_connector.h"
#include "../ias-libtool/json11.hpp"
#include <vector>
#include <android/log.h>
#include <android/fdsan.h>
#include <dlfcn.h>

const char *AndroidConnector::TAG = "cpp";

extern "C" JNIEXPORT
    jint JNICALL
    JNI_OnLoad(JavaVM *vm, void *)
{
    __android_log_write(ANDROID_LOG_DEBUG, AndroidConnector::TAG, "jni on load executed");
    return AndroidConnector::getInstance().jniLoad(vm);
}

extern "C" JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved)
{
    __android_log_write(ANDROID_LOG_DEBUG, AndroidConnector::TAG, "jni on unload executed");
    return AndroidConnector::getInstance().unregisterSharedObject();
}

extern "C" JNIEXPORT void JNICALL Java_com_zafaco_moduleSpeed_jni_JniSpeedMeasurementClient_startMeasurement(
    JNIEnv *env, jobject thiz)
{
    AndroidConnector::getInstance().startMeasurement();
}

extern "C" JNIEXPORT void JNICALL
Java_com_zafaco_moduleSpeed_jni_JniSpeedMeasurementClient_stopMeasurement(JNIEnv *env, jobject thiz)
{
    AndroidConnector::getInstance().stopMeasurement();
}

extern "C" JNIEXPORT void JNICALL Java_com_zafaco_moduleSpeed_jni_JniSpeedMeasurementClient_shareMeasurementState(
    JNIEnv *env, jobject caller, jobject speedTaskDesc,
    jobject baseMeasurementState, jobject pingMeasurementState,
    jobject downloadMeasurementState, jobject uploadMeasurementState)
{
    AndroidConnector &connector = AndroidConnector::getInstance();
    connector.registerSharedObject(env, caller, baseMeasurementState, pingMeasurementState,
                                   downloadMeasurementState, uploadMeasurementState);
    connector.setSpeedSettings(env, speedTaskDesc);
}

extern "C" JNIEXPORT void JNICALL
Java_com_zafaco_speed_jni_JniSpeedMeasurementClient_cleanUp(JNIEnv *env, jobject caller)
{
    AndroidConnector::getInstance().unregisterSharedObject();
}

jint AndroidConnector::jniLoad(JavaVM *vm)
{
    JNIEnv *env;
    if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK)
    {
        return JNI_ERR;
    }

    jclass clazz = env->FindClass("com/zafaco/moduleSpeed/jni/JniSpeedMeasurementClient");
    jniHelperClass = (jclass)env->NewGlobalRef(clazz);
    callbackID = env->GetMethodID(jniHelperClass, "cppCallback",
                                  "(Ljava/lang/String;)V");
    cppCallbackFinishedID = env->GetMethodID(jniHelperClass, "cppCallbackFinished",
                                             "(Ljava/lang/String;Lcom/zafaco/moduleSpeed/models/speed/JniSpeedMeasurementResult;)V");

    clazz = env->FindClass(
        "com/zafaco/moduleSpeed/jni/exception/AndroidJniCppException");
    jniExceptionClass = (jclass)env->NewGlobalRef(clazz);

    clazz = env->FindClass(
        "com/zafaco/moduleSpeed/models/speed/SpeedMeasurementState$SpeedPhaseState");
    fieldAvgThroughput = env->GetFieldID(clazz, "throughputAvgBps", "J");

    clazz = env->FindClass(
        "com/zafaco/moduleSpeed/models/speed/SpeedMeasurementState$PingPhaseState");
    fieldAverageMs = env->GetFieldID(clazz, "averageMs", "J");

    clazz = env->FindClass("com/zafaco/moduleSpeed/models/speed/SpeedMeasurementState");
    fieldProgress = env->GetFieldID(clazz, "progress", "F");

    setMeasurementPhaseByStringValueID = env->GetMethodID(clazz, "setMeasurementPhaseByStringValue",
                                                          "(Ljava/lang/String;)V");

    clazz = env->FindClass(
        "com/zafaco/moduleSpeed/models/speed/JniSpeedMeasurementResult");
    speedMeasurementResultClazz = (jclass)env->NewGlobalRef(clazz);
    clazz = env->FindClass("com/zafaco/moduleSpeed/models/rtt/RttUdpResult");
    resultUdpClazz = (jclass)env->NewGlobalRef(clazz);
    clazz = env->FindClass(
        "com/zafaco/moduleSpeed/models/speed/BandwidthResult");
    resultBandwidthClazz = (jclass)env->NewGlobalRef(clazz);
    clazz = env->FindClass("com/zafaco/moduleSpeed/models/time/TimeInfo");
    timeClazz = (jclass)env->NewGlobalRef(clazz);

    javaVM = vm;

    return JNI_VERSION_1_6;
}

void AndroidConnector::registerSharedObject(
    JNIEnv *env, jobject caller, jobject baseMeasurementState, jobject pingMeasurementState,
    jobject downloadMeasurementState, jobject uploadMeasurementState)
{
    jniCaller = env->NewGlobalRef(caller);

    this->baseMeasurementState = env->NewGlobalRef(baseMeasurementState);
    this->downloadMeasurementState = env->NewGlobalRef(downloadMeasurementState);
    this->uploadMeasurementState = env->NewGlobalRef(uploadMeasurementState);
    this->pingMeasurementState = env->NewGlobalRef(pingMeasurementState);
}

void AndroidConnector::setSpeedSettings(JNIEnv *env, jobject speedTaskDesc)
{
    const jclass clazz = env->FindClass("com/zafaco/moduleSpeed/models/speed/SpeedTaskDesc");

    jfieldID toParseId = env->GetFieldID(clazz, "speedServerAddrs", "[Ljava/lang/String;");
    jobjectArray serverUrls = (jobjectArray)env->GetObjectField(speedTaskDesc, toParseId);

    if (serverUrls != nullptr)
    {
        int i;
        jsize len = env->GetArrayLength(serverUrls);

        for (i = 0; i < len; i++)
        {
            this->speedTaskDesc.measurementServerUrl.emplace_back(
                env->GetStringUTFChars((jstring)env->GetObjectArrayElement(serverUrls, i),
                                       NULL));
        }
    }

    toParseId = env->GetFieldID(clazz, "rttCount", "I");
    this->speedTaskDesc.rttCount = (int)env->GetIntField(speedTaskDesc, toParseId);

    toParseId = env->GetFieldID(clazz, "downloadStreams", "I");
    this->speedTaskDesc.downloadStreams = (int)env->GetIntField(speedTaskDesc, toParseId);

    toParseId = env->GetFieldID(clazz, "uploadStreams", "I");
    this->speedTaskDesc.uploadStreams = (int)env->GetIntField(speedTaskDesc, toParseId);

    toParseId = env->GetFieldID(clazz, "speedServerPort", "I");
    this->speedTaskDesc.speedServerPort = (int)env->GetIntField(speedTaskDesc, toParseId);

    toParseId = env->GetFieldID(clazz, "speedServerPortRtt", "I");
    this->speedTaskDesc.speedServerPortRtt = (int)env->GetIntField(speedTaskDesc, toParseId);

    toParseId = env->GetFieldID(clazz, "performDownload", "Z");
    this->speedTaskDesc.performDownload = env->GetBooleanField(speedTaskDesc, toParseId);

    toParseId = env->GetFieldID(clazz, "performUpload", "Z");
    this->speedTaskDesc.performUpload = env->GetBooleanField(speedTaskDesc, toParseId);

    toParseId = env->GetFieldID(clazz, "performRtt", "Z");
    this->speedTaskDesc.performRtt = env->GetBooleanField(speedTaskDesc, toParseId);

    toParseId = env->GetFieldID(clazz, "useEncryption", "Z");
    this->speedTaskDesc.isEncrypted = env->GetBooleanField(speedTaskDesc, toParseId);
}

void AndroidConnector::unregisterSharedObject()
{
    JNIEnv *env = getJniEnv();
    if (env == nullptr)
    {
        return;
    }

    if (jniExceptionClass != nullptr)
    {
        env->DeleteGlobalRef(jniExceptionClass);
        jniExceptionClass = nullptr;
    }
    if (jniHelperClass != nullptr)
    {
        env->DeleteGlobalRef(jniHelperClass);
        jniHelperClass = nullptr;
    }
    if (speedMeasurementResultClazz != nullptr)
    {
        env->DeleteGlobalRef(speedMeasurementResultClazz);
        speedMeasurementResultClazz = nullptr;
    }
    if (resultUdpClazz != nullptr)
    {
        env->DeleteGlobalRef(resultUdpClazz);
        resultUdpClazz = nullptr;
    }
    if (resultBandwidthClazz != nullptr)
    {
        env->DeleteGlobalRef(resultBandwidthClazz);
        resultBandwidthClazz = nullptr;
    }
    if (timeClazz != nullptr)
    {
        env->DeleteGlobalRef(timeClazz);
        timeClazz = nullptr;
    }
    if (baseMeasurementState != nullptr)
    {
        env->DeleteGlobalRef(baseMeasurementState);
        baseMeasurementState = nullptr;
    }
    if (downloadMeasurementState != nullptr)
    {
        env->DeleteGlobalRef(downloadMeasurementState);
        downloadMeasurementState = nullptr;
    }
    if (uploadMeasurementState != nullptr)
    {
        env->DeleteGlobalRef(uploadMeasurementState);
        uploadMeasurementState = nullptr;
    }
    if (pingMeasurementState != nullptr)
    {
        env->DeleteGlobalRef(pingMeasurementState);
        pingMeasurementState = nullptr;
    }
    if (jniCaller != nullptr)
    {
        env->DeleteGlobalRef(jniCaller);
        jniCaller = nullptr;
    }
}

void AndroidConnector::callback(json11::Json::object &message) const
{
    JNIEnv *env = getJniEnv();
    if (env == nullptr)
    {
        return;
    }

    jmethodID initId = env->GetMethodID(speedMeasurementResultClazz, "<init>", "()V");

    jobject speedMeasurementResult = env->NewObject(speedMeasurementResultClazz, initId);

    env->CallVoidMethod(baseMeasurementState, setMeasurementPhaseByStringValueID,
                        env->NewStringUTF(getStringForMeasurementPhase(currentTestPhase).c_str()));

    if (currentTestPhase == MeasurementPhase::PING)
    {
        if (message["rtt_udp_info"].is_object())
        {
            json11::Json const recentResult = message["rtt_udp_info"];
            passJniSpeedState(env, MeasurementPhase::PING, recentResult);
        }
        else
        {
            env->SetFloatField(baseMeasurementState, fieldProgress, 0.0f);
        }
    }

    if (currentTestPhase == MeasurementPhase::DOWNLOAD)
    {

        if (message["download_info"].is_array())
        {
            const json11::Json::array dlInfo = message["download_info"].array_items();
            const json11::Json recentResult = dlInfo.at(dlInfo.size() - 1);
            passJniSpeedState(env, MeasurementPhase::DOWNLOAD, recentResult);
        }
        else
        {
            env->SetFloatField(baseMeasurementState, fieldProgress, 0.0f);
        }
    }

    if (currentTestPhase == MeasurementPhase::UPLOAD)
    {
        if (message["upload_info"].is_array())
        {
            const json11::Json::array ulInfo = message["upload_info"].array_items();
            const json11::Json recentResult = ulInfo.at(ulInfo.size() - 1);
            passJniSpeedState(env, MeasurementPhase::UPLOAD, recentResult);
        }
        else
        {
            env->SetFloatField(baseMeasurementState, fieldProgress, 0.0f);
        }
    }

    const jstring javaMsg = env->NewStringUTF(json11::Json(message).dump().c_str());
    env->CallVoidMethod(jniCaller, callbackID, javaMsg, speedMeasurementResult);

    env->CallVoidMethod(jniCaller, callbackID, env->NewStringUTF(""));
}

void AndroidConnector::callbackFinished(json11::Json::object &message)
{

    JNIEnv *env = getJniEnv();
    if (env == nullptr)
    {
        return;
    }

    jmethodID initId = env->GetMethodID(speedMeasurementResultClazz, "<init>", "()V");

    jobject speedMeasurementResult = env->NewObject(speedMeasurementResultClazz, initId);

    JavaParseInformation parse;

    parse.longClass = env->FindClass("java/lang/Long");
    parse.staticLongValueOf = env->GetStaticMethodID(parse.longClass, "valueOf",
                                                     "(J)Ljava/lang/Long;");

    parse.intClass = env->FindClass("java/lang/Integer");
    parse.staticIntValueOf = env->GetStaticMethodID(parse.intClass, "valueOf",
                                                    "(I)Ljava/lang/Integer;");

    parse.floatClass = env->FindClass("java/lang/Float");
    parse.staticFloatValueOf = env->GetStaticMethodID(parse.floatClass, "valueOf",
                                                      "(F)Ljava/lang/Float;");

    if (message["rtt_udp_info"].is_object())
    {
        jmethodID setMethod = env->GetMethodID(speedMeasurementResultClazz, "setRttUdpResult",
                                               "(Lcom/zafaco/moduleSpeed/models/rtt/RttUdpResult;)V");
        initId = env->GetMethodID(resultUdpClazz, "<init>", "()V");

        Json const &rttEntry = message["rtt_udp_info"];
        jobject singleResult = env->NewObject(resultUdpClazz, initId);

        jmethodID setId = env->GetMethodID(resultUdpClazz, "setAverageNs", "(J)V");
        jobject obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf,
                                                  std::stoll(
                                                      rttEntry["average_ns"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setDurationNs", "(J)V");
        obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf,
                                          std::stoll(rttEntry["duration_ns"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setMaxNs", "(J)V");
        obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf,
                                          std::stoll(rttEntry["max_ns"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setMedianNs", "(J)V");
        obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf,
                                          std::stoll(rttEntry["median_ns"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setMinNs", "(J)V");
        obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf,
                                          std::stoll(rttEntry["min_ns"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setNumError", "(I)V");
        obj = env->CallStaticObjectMethod(parse.intClass, parse.staticIntValueOf,
                                          std::stol(rttEntry["num_error"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setNumMissing", "(I)V");
        obj = env->CallStaticObjectMethod(parse.intClass, parse.staticIntValueOf,
                                          std::stol(rttEntry["num_missing"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setNumReceived", "(I)V");
        obj = env->CallStaticObjectMethod(parse.intClass, parse.staticIntValueOf,
                                          std::stol(rttEntry["num_received"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setNumSent", "(I)V");
        obj = env->CallStaticObjectMethod(parse.intClass, parse.staticIntValueOf,
                                          std::stol(rttEntry["num_sent"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setPacketSize", "(I)V");
        obj = env->CallStaticObjectMethod(parse.intClass, parse.staticIntValueOf,
                                          std::stol(rttEntry["packet_size"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setPeer", "(Ljava/lang/String;)V");
        obj = env->NewStringUTF(rttEntry["peer"].string_value().c_str());
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setProgress", "(F)V");
        obj = env->CallStaticObjectMethod(parse.floatClass, parse.staticFloatValueOf,
                                          rttEntry["progress"].number_value());
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        setId = env->GetMethodID(resultUdpClazz, "setStandardDeviationNs", "(J)V");
        obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf, std::stoll(rttEntry["standard_deviation_ns"].string_value()));
        env->CallVoidMethod(singleResult, setId, obj);
        env->DeleteLocalRef(obj);

        jmethodID addMethod = env->GetMethodID(resultUdpClazz, "addSingleRtt", "(JJ)V");
        if (rttEntry["rtts"].is_array())
        {
            jobject timeStampObj;
            for (json11::Json const &singleRtt : rttEntry["rtts"].array_items())
            {
                long long rttValue = -1;
                if (singleRtt["rtt_ns"].is_number())
                {
                    rttValue = (long long)singleRtt["rtt_ns"].int_value();
                }
                obj = rttValue != -1 ? env->CallStaticObjectMethod(parse.longClass,
                                                                   parse.staticLongValueOf,
                                                                   rttValue)
                                     : nullptr;
                long long timeStamp = -1;
                if (singleRtt["relative_time_ns_measurement_start"].is_string())
                {
                    timeStamp = std::stoll(
                        singleRtt["relative_time_ns_measurement_start"].string_value());
                }
                timeStampObj = timeStamp != -1 ? env->CallStaticObjectMethod(parse.longClass,
                                                                             parse.staticLongValueOf,
                                                                             timeStamp)
                                               : nullptr;

                env->CallVoidMethod(singleResult, addMethod, obj, timeStampObj);
                env->DeleteLocalRef(obj);
                env->DeleteLocalRef(timeStampObj);
            }
        }
        env->CallVoidMethod(speedMeasurementResult, setMethod, singleResult);
    }

    if (message["download_info"].is_array())
    {
        jmethodID addMethod = env->GetMethodID(speedMeasurementResultClazz, "addDownloadInfo",
                                               "(Lcom/zafaco/moduleSpeed/models/speed/BandwidthResult;)V");
        initId = env->GetMethodID(resultBandwidthClazz, "<init>", "()V");

        for (Json const &downloadEntry : message["download_info"].array_items())
        {
            jobject singleResult = env->NewObject(resultBandwidthClazz, initId);
            setBandwidthResult(env, downloadEntry, singleResult, parse);
            env->CallVoidMethod(speedMeasurementResult, addMethod, singleResult);
        }
    }

    if (message["upload_info"].is_array())
    {
        jmethodID addMethod = env->GetMethodID(speedMeasurementResultClazz, "addUploadInfo",
                                               "(Lcom/zafaco/moduleSpeed/models/speed/BandwidthResult;)V");
        initId = env->GetMethodID(resultBandwidthClazz, "<init>", "()V");

        for (Json const &downloadEntry : message["upload_info"].array_items())
        {
            jobject singleResult = env->NewObject(resultBandwidthClazz, initId);
            setBandwidthResult(env, downloadEntry, singleResult, parse);
            env->CallVoidMethod(speedMeasurementResult, addMethod, singleResult);
        }
    }

    if (message["time_info"].is_object())
    {
        Json const &timeEntry = message["time_info"];
        jmethodID addMethod = env->GetMethodID(speedMeasurementResultClazz, "setTimeInfo",
                                               "(Lcom/zafaco/moduleSpeed/models/time/TimeInfo;)V");

        initId = env->GetMethodID(timeClazz, "<init>", "()V");
        jobject timeObj = env->NewObject(timeClazz, initId);
        jmethodID setId;

        jobject obj;

        if (timeEntry["download_start"].is_string())
        {
            setId = env->GetMethodID(timeClazz, "setDownloadStart", "(J)V");
            env->CallVoidMethod(timeObj, setId,
                                std::stoll(timeEntry["download_start"].string_value()));
        }

        if (timeEntry["download_end"].is_string())
        {
            setId = env->GetMethodID(timeClazz, "setDownloadEnd", "(J)V");
            obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf,
                                              std::stoll(
                                                  timeEntry["download_end"].string_value()));
            env->CallVoidMethod(timeObj, setId, obj);
            env->DeleteLocalRef(obj);
        }

        if (timeEntry["rtt_udp_start"].is_string())
        {
            setId = env->GetMethodID(timeClazz, "setRttUdpStart", "(J)V");
            obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf,
                                              std::stoll(
                                                  timeEntry["rtt_udp_start"].string_value()));
            env->CallVoidMethod(timeObj, setId, obj);
            env->DeleteLocalRef(obj);
        }

        if (timeEntry["rtt_udp_end"].is_string())
        {
            setId = env->GetMethodID(timeClazz, "setRttUdpEnd", "(J)V");
            obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf,
                                              std::stoll(
                                                  timeEntry["rtt_udp_end"].string_value()));
            env->CallVoidMethod(timeObj, setId, obj);
            env->DeleteLocalRef(obj);
        }

        if (timeEntry["upload_start"].is_string())
        {
            setId = env->GetMethodID(timeClazz, "setUploadStart", "(J)V");
            obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf,
                                              std::stoll(
                                                  timeEntry["upload_start"].string_value()));
            env->CallVoidMethod(timeObj, setId, obj);
            env->DeleteLocalRef(obj);
        }

        if (timeEntry["upload_end"].is_string())
        {
            setId = env->GetMethodID(timeClazz, "setUploadEnd", "(J)V");
            obj = env->CallStaticObjectMethod(parse.longClass, parse.staticLongValueOf,
                                              std::stoll(timeEntry["upload_end"].string_value()));
            env->CallVoidMethod(timeObj, setId, obj);
            env->DeleteLocalRef(obj);
        }

        env->CallVoidMethod(speedMeasurementResult, addMethod, timeObj);
    }

    if (message["peer_info"].is_object())
    {
        Json const &peerEntry = message["peer_info"];

        if (peerEntry["ip"].is_string())
        {
            jmethodID setId = env->GetMethodID(speedMeasurementResultClazz,
                                               "setMeasurementServerIp", "(Ljava/lang/String;)V");
            jstring str = env->NewStringUTF(peerEntry["ip"].string_value().c_str());
            env->CallVoidMethod(speedMeasurementResult, setId, str);
            env->DeleteLocalRef(str);
        }
    }

    const jstring javaMsg = env->NewStringUTF(json11::Json(message).dump().c_str());
    env->CallVoidMethod(jniCaller, cppCallbackFinishedID, javaMsg, speedMeasurementResult);

    if (baseMeasurementState != nullptr)
    {
        env->SetFloatField(baseMeasurementState, fieldProgress, 1.0F);
        env->CallVoidMethod(baseMeasurementState, setMeasurementPhaseByStringValueID,
                            env->NewStringUTF(
                                getStringForMeasurementPhase(MeasurementPhase::END).c_str()));
    }
}

void AndroidConnector::passJniSpeedState(
    JNIEnv *env, const MeasurementPhase &speedStateToSet, const json11::Json &json) const
{
    jobject toFill = nullptr;
    json11::Json obj;

    switch (speedStateToSet)
    {
    case MeasurementPhase::PING:
        toFill = pingMeasurementState;
        obj = json["average_ns"];
        if (obj.is_string())
        {
            env->SetLongField(toFill, fieldAverageMs, std::stoll(obj.string_value()) / 1e6);
        }
        break;
    case MeasurementPhase::DOWNLOAD:
        toFill = downloadMeasurementState;
    case MeasurementPhase::UPLOAD:
        if (toFill == nullptr)
        {
            toFill = uploadMeasurementState;
        }

        obj = json["throughput_avg_bps"];
        if (obj.is_string())
        {
            env->SetLongField(toFill, fieldAvgThroughput, std::stoll(obj.string_value()));
        }

        break;
    }

    obj = json["progress"];
    if (obj.is_number())
    {
        env->SetFloatField(baseMeasurementState, fieldProgress, obj.number_value());
    }
}

void AndroidConnector::setBandwidthResult(
    JNIEnv *env, json11::Json const &jsonItems, jobject &result,
    JavaParseInformation &parseInfo)
{
    jmethodID setId = env->GetMethodID(resultBandwidthClazz, "setBytes", "(J)V");
    jobject obj = env->CallStaticObjectMethod(parseInfo.longClass, parseInfo.staticLongValueOf,
                                              std::stoll(jsonItems["bytes"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setBytesDistinct", "(J)V");
    obj = env->CallStaticObjectMethod(parseInfo.longClass, parseInfo.staticLongValueOf,
                                      std::stoll(jsonItems["bytes_distinct"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setBytesTotal", "(J)V");
    obj = env->CallStaticObjectMethod(parseInfo.longClass, parseInfo.staticLongValueOf,
                                      std::stoll(jsonItems["bytes_total"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setBytesSlowStart", "(J)V");
    obj = env->CallStaticObjectMethod(parseInfo.longClass, parseInfo.staticLongValueOf,
                                      std::stoll(jsonItems["bytes_slow_start"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setDurationNs", "(J)V");
    obj = env->CallStaticObjectMethod(parseInfo.longClass, parseInfo.staticLongValueOf,
                                      std::stoll(jsonItems["duration_ns"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setDurationNsDistinct", "(J)V");
    obj = env->CallStaticObjectMethod(parseInfo.longClass, parseInfo.staticLongValueOf,
                                      std::stoll(
                                          jsonItems["duration_ns_distinct"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setDurationNsTotal", "(J)V");
    obj = env->CallStaticObjectMethod(parseInfo.longClass, parseInfo.staticLongValueOf,
                                      std::stoll(jsonItems["duration_ns_total"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setRelativeTimeNsMeasurementStart", "(J)V");
    obj = env->CallStaticObjectMethod(parseInfo.longClass, parseInfo.staticLongValueOf,
                                      std::stoll(
                                          jsonItems["relative_time_ns_measurement_start"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setThroughputAvgBps", "(J)V");
    obj = env->CallStaticObjectMethod(parseInfo.longClass, parseInfo.staticLongValueOf,
                                      std::stoll(jsonItems["throughput_avg_bps"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setNumStreamsEnd", "(I)V");
    obj = env->CallStaticObjectMethod(parseInfo.intClass, parseInfo.staticIntValueOf,
                                      std::stol(jsonItems["num_streams_end"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setNumStreamsStart", "(I)V");
    obj = env->CallStaticObjectMethod(parseInfo.intClass, parseInfo.staticIntValueOf,
                                      std::stol(jsonItems["num_streams_start"].string_value()));
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);

    setId = env->GetMethodID(resultBandwidthClazz, "setProgress", "(F)V");
    obj = env->CallStaticObjectMethod(parseInfo.floatClass, parseInfo.staticFloatValueOf,
                                      jsonItems["progress"].number_value());
    env->CallVoidMethod(result, setId, obj);
    env->DeleteLocalRef(obj);
}

void AndroidConnector::printLog(std::string const &category, std::string const &message) const
{
    android_LogPriority logPriority = ANDROID_LOG_INFO;

    if (category == "DEBUG")
    {
        logPriority = ANDROID_LOG_DEBUG;
    }
    else if (category == "INFO")
    {
        logPriority = ANDROID_LOG_INFO;
    }
    else if (category == "WARN")
    {
        logPriority = ANDROID_LOG_WARN;
    }
    else if (category == "CRITICAL")
    {
        logPriority = ANDROID_LOG_ERROR;
    }
    else if (category == "ERROR")
    {
        logPriority = ANDROID_LOG_FATAL;
    }
    __android_log_write(logPriority, TAG, message.c_str());
}

void AndroidConnector::startMeasurement()
{
    try
    {
        CTrace::setLogFunction(std::bind(&AndroidConnector::printLog, this, std::placeholders::_1,
                                         std::placeholders::_2));

        void *lib_handle = dlopen("libc.so", RTLD_LAZY);
        if (lib_handle)
        {

            void (*set_fdsan_error_level)(enum android_fdsan_error_level newlevel);

            set_fdsan_error_level = (void (*)(enum android_fdsan_error_level))dlsym(lib_handle,
                                                                                    "android_fdsan_set_error_level");
            if (set_fdsan_error_level)
            {
                set_fdsan_error_level(ANDROID_FDSAN_ERROR_LEVEL_DISABLED);
                TRC_INFO("FDSAN disabled");
            }
            dlclose(lib_handle);
        }

        ::_DEBUG_ = false;
        ::RUNNING = true;

        ::RTT = speedTaskDesc.performRtt;
        ::DOWNLOAD = speedTaskDesc.performDownload;
        ::UPLOAD = speedTaskDesc.performUpload;

        json11::Json::object jRttParameters;
        json11::Json::object jDownloadParameters;
        json11::Json::object jUploadParameters;

        json11::Json::object jMeasurementParameters;

        jRttParameters["performMeasurement"] = ::RTT;
        jDownloadParameters["performMeasurement"] = ::DOWNLOAD;
        jUploadParameters["performMeasurement"] = ::UPLOAD;

        jDownloadParameters["streams"] = std::to_string(speedTaskDesc.downloadStreams);
        jUploadParameters["streams"] = std::to_string(speedTaskDesc.uploadStreams);
        jRttParameters["ping_query"] = std::to_string(speedTaskDesc.rttCount);
        jMeasurementParameters["rtt"] = json11::Json(jRttParameters);
        jMeasurementParameters["download"] = json11::Json(jDownloadParameters);
        jMeasurementParameters["upload"] = json11::Json(jUploadParameters);

        jMeasurementParameters["platform"] = "mobile";
        jMeasurementParameters["clientos"] = "android";
        jMeasurementParameters["wsTargetPort"] = std::to_string(speedTaskDesc.speedServerPort);
        jMeasurementParameters["wsTargetPortRtt"] = std::to_string(
            speedTaskDesc.speedServerPortRtt);
        jMeasurementParameters["wsWss"] = speedTaskDesc.isEncrypted ? "1" : "0";

        jMeasurementParameters["wsTargets"] = this->speedTaskDesc.measurementServerUrl;

        json11::Json jMeasurementParametersJson = jMeasurementParameters;

        measurementStart(jMeasurementParametersJson.dump());
    }
    catch (std::exception &ex)
    {
        shutdown();
    }
}

void AndroidConnector::stopMeasurement()
{
    measurementStop();
}
