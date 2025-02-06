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

#import "ios_connector.h"
#include "../ias-libtool/json11.hpp"
#include "type.h"
#include "callback.h"

extern void measurementStart(std::string measurementParameters);

extern void measurementStop();

extern void startTestCase(int nTestCase);

extern void shutdown();

extern void show_usage(char* argv0);

extern bool _DEBUG_;
extern bool RUNNING;

extern const char* PLATFORM;
extern const char* CLIENT_OS;

extern unsigned long long TCP_STARTUP;

extern bool TIMER_ACTIVE;
extern bool TIMER_RUNNING;
extern bool TIMER_STOPPED;

extern std::atomic_bool hasError;

extern int TIMER_INDEX;
extern int TIMER_INDEX_UPLOAD_FIRST;
extern int TIMER_DURATION;
extern unsigned long long MEASUREMENT_DURATION;

extern struct conf_data conf;
extern struct measurement measurements;

extern std::vector<char> randomDataValues;

extern std::map<int,int> syncing_threads;

extern class CTrace* pTrace;

extern class CConfigManager* pConfig;
extern class CConfigManager* pXml;
extern class CConfigManager* pService;

extern class CCallback *pCallback;
extern class CMeasurement* pMeasurement;

extern MeasurementPhase currentTestPhase;

extern std::function<void(int)> signalFunction;

@implementation IosConnector

+(void)callback:(NSString *)str {
    NSLog(@"%@", str);
}

-(void)start:(NSDictionary *)measurementParameters callback:(void (^)(NSDictionary<NSString *, id> * __nonnull json))callback {
    try {
        CTrace::setLogFunction(
            [] (std::string str, std::string str2) {
                NSString *nsstr = [NSString stringWithUTF8String:str.c_str()];
                NSString *nsstr2 = [NSString stringWithUTF8String:str2.c_str()];
                NSLog(@"%@: %@", nsstr, nsstr2);
            }
        );

        CCallback::iosCallbackFunc = [callback] (Json::object& jsonObj) {
            json11::Json json = jsonObj;
            NSString *jsonStr = [NSString stringWithUTF8String:json.dump().c_str()];
            NSData *jsonData = [jsonStr dataUsingEncoding:NSUTF8StringEncoding];

            NSError *jsonError;
            NSDictionary *jsonDict = [NSJSONSerialization JSONObjectWithData:jsonData
                                                                 options:NSJSONReadingMutableContainers
                                                                       error:&jsonError];
            if (jsonError != nil) {
                jsonDict = @{@"error": @true};
            }

            if (callback != nil) {
                callback(jsonDict);
            }
        };

        #ifdef DEBUG
            ::_DEBUG_ = true;
        #else
            ::_DEBUG_ = false;
        #endif

        ::RUNNING = true;

        Json::object jRttParameters;
        Json::object jDownloadParameters;
        Json::object jUploadParameters;

        Json::object jMeasurementParameters;

        //set requested test cases
        jRttParameters["performMeasurement"] = [[[measurementParameters objectForKey:@"rtt"] objectForKey:@"performMeasurement"] boolValue];
        jDownloadParameters["performMeasurement"] = [[[measurementParameters objectForKey:@"download"] objectForKey:@"performMeasurement"] boolValue];
        jUploadParameters["performMeasurement"] = [[[measurementParameters objectForKey:@"upload"] objectForKey:@"performMeasurement"] boolValue];

        //set default measurement parameters
        jDownloadParameters["streams"] = "4";
        jUploadParameters["streams"] = "4";

        if ([measurementParameters objectForKey:@"download"] && [[measurementParameters objectForKey:@"download"] objectForKey:@"streams"])
        {
            jDownloadParameters["streams"] = std::string([[[[measurementParameters objectForKey:@"download"] objectForKey:@"streams"] stringValue] UTF8String]);
        }
        if ([measurementParameters objectForKey:@"upload"] && [[measurementParameters objectForKey:@"upload"] objectForKey:@"streams"])
        {
            jUploadParameters["streams"] = std::string([[[[measurementParameters objectForKey:@"upload"] objectForKey:@"streams"] stringValue] UTF8String]);
        }

        jMeasurementParameters["rtt"] = Json(jRttParameters);
        jMeasurementParameters["download"] = Json(jDownloadParameters);
        jMeasurementParameters["upload"] = Json(jUploadParameters);

        jMeasurementParameters["platform"] = "mobile";
        jMeasurementParameters["clientos"] = "ios";

        jMeasurementParameters["wsTargetPort"] = "443";
        if ([measurementParameters objectForKey:@"wsTargetPort"])
        {
            jMeasurementParameters["wsTargetPort"] = std::string([[measurementParameters objectForKey:@"wsTargetPort"] UTF8String]);
        }

        jMeasurementParameters["wsTargetPortRtt"] = "80";
        if ([measurementParameters objectForKey:@"wsTargetPortRtt"])
        {
            jMeasurementParameters["wsTargetPortRtt"] = std::string([[measurementParameters objectForKey:@"wsTargetPortRtt"] UTF8String]);
        }

        jMeasurementParameters["wsWss"] = "1";
        if ([measurementParameters objectForKey:@"wsWss"])
        {
            jMeasurementParameters["wsWss"] = std::string([[[measurementParameters objectForKey:@"wsWss"] stringValue] UTF8String]);
        }

        Json::array jTargets;
        if ([measurementParameters objectForKey:@"wsTargets"])
        {
            if ([[measurementParameters objectForKey:@"wsTargets"] isKindOfClass:[NSArray class]])
            {
                for (NSString *target in [measurementParameters objectForKey:@"wsTargets"])
                {
                    jTargets.push_back(std::string([target UTF8String]));
                }
            }
        }
        jMeasurementParameters["wsTargets"] = Json(jTargets);

        json11::Json jMeasurementParametersJson = jMeasurementParameters;
        measurementStart(jMeasurementParametersJson.dump());
    } catch (std::exception & ex) {
        NSLog(@"%@", [NSString stringWithUTF8String:ex.what()]);
    }
}

-(void)stop
{
    measurementStop();
}

@end
