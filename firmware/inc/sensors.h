/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _SENSORS_H_
#define _SENSORS_H_

#ifdef __cplusplus
extern "C" {
#endif
#include <plat/inc/taggedPtr.h>
#include <eventnums.h>
#include <sensType.h>
#include <stdbool.h>
#include <stdint.h>


#define MAX_REGISTERED_SENSORS  32 /* this may need to be revisted later */
#define MAX_MIN_SAMPLES         3000

enum NumAxis {
    NUM_AXIS_EMBEDDED = 0,   // data = (uint32_t)evtData
    NUM_AXIS_ONE      = 1,   // data is in struct SingleAxisDataEvent format
    NUM_AXIS_THREE    = 3,   // data is in struct TripleAxisDataEvent format
    NUM_AXIS_WIFI     = 253, // data is in struct WifiScanEvent format
};

struct SensorFirstSample
{
    uint8_t numSamples;
    uint8_t numFlushes;
    uint8_t biasCurrent : 1;
    uint8_t biasPresent : 1;
    uint8_t biasSample : 6;
    uint8_t interrupt;
};

// NUM_AXIS_EMBEDDED data format
union EmbeddedDataPoint {
    uint32_t idata;
    float fdata;
    void *vptr;
};

// NUM_AXIS_ONE data format
struct SingleAxisDataPoint {
    union {
        uint32_t deltaTime; //delta since last sample, for 0th sample this is firstSample
        struct SensorFirstSample firstSample;
    };
    union {
        float fdata;
        int32_t idata;
    };
} __attribute__((packed));

struct SingleAxisDataEvent {
    uint64_t referenceTime;
    struct SingleAxisDataPoint samples[];
};

// NUM_AXIS_THREE data format
struct TripleAxisDataPoint {
    union {
        uint32_t deltaTime; //delta since last sample, for 0th sample this is firstSample
        struct SensorFirstSample firstSample;
    };
    union {
        float x;
        int32_t ix;
    };
    union {
        float y;
        int32_t iy;
    };
    union {
        float z;
        int32_t iz;
    };
} __attribute__((packed));

struct TripleAxisDataEvent {
    uint64_t referenceTime;
    struct TripleAxisDataPoint samples[];
};

struct RawTripleAxisDataPoint {
    union {
        uint32_t deltaTime; //delta since last sample, for 0th sample this is firstSample
        struct SensorFirstSample firstSample;
    };
    int16_t ix;
    int16_t iy;
    int16_t iz;
} __attribute__((packed));

struct RawTripleAxisDataEvent {
    uint64_t referenceTime;
    struct RawTripleAxisDataPoint samples[];
};

// For WiFi Scan Events
#define WIFI_MAX_SSID_LEN (32+1)
#define WIFI_BSSID_LEN 6

enum WifiScanResultFlags {
    WIFI_SCAN_RESULT_UNKNOWN                           = 0,
    WIFI_SCAN_RESULT_HT_OPS_PRESENT                    = (1<<0), // EID 61 present, HT 8.4.2.59, HT capabilities present
    WIFI_SCAN_RESULT_VHT_OPS_PRESENT                   = (1<<1), // EID 192 present, VHT 8.4.2.161, VHT capabilities present
    WIFI_SCAN_RESULT_IS_80211MC_RTT_RESPONDER          = (1<<2), // EID 127, bit 70
    WIFI_SCAN_RESULT_HAS_SECONDARY_CHANNEL_OFFSET      = (1<<3), // HT 8.4.2.59
    WIFI_SCAN_RESULT_SECONDARY_CHANNEL_OFFSET_IS_BELOW = (1<<4), // HT 8.4.2.59, secondary channel is below primary channel
};

struct WifiScanResult {
    union {                                 // this union must *always* be first item in the struct
        uint32_t deltaTime;                 // delta since last sample in nanoseconds
        struct SensorFirstSample firstSample;
    };
    uint8_t channelWidth;                   // VHT 8.4.2.161
    uint8_t centerFreqIndex0;               // VHT 8.4.2.161
    uint8_t centerFreqIndex1;               // VHT 8.4.2.161
    uint8_t flags;                          // a set of fields from WifiScanResultFlags enumeration
    uint64_t rtt;                           // in picoseconds
    uint64_t rttStd;                        // standard deviation in rtt
    char ssid[WIFI_MAX_SSID_LEN];           // null-terminated
    uint8_t bssid[WIFI_BSSID_LEN];
    int8_t rssi;                            // RSSI in dBm
    uint8_t band;                           // 0 = 2.4 GHz, 1 = 5 GHz
    uint8_t channelIndex;
};

struct WifiScanEvent {
    uint64_t referenceTime;                 // in nanoseconds
    struct WifiScanResult results[];
};

struct UserSensorEventHdr {  //all user sensor events start with this struct
    TaggedPtr marshallCbk;
};

#define SENSOR_DATA_EVENT_FLUSH (void *)0xFFFFFFFF // flush for all data

struct SensorPowerEvent {
    void *callData;
    bool on;
};

struct SensorSetRateEvent {
    void *callData;
    uint32_t rate;
    uint64_t latency;
};

struct SensorCfgDataEvent {
    void *callData;
    void *data;
};

struct SensorSendDirectEventEvent {
    void *callData;
    uint32_t tid;
};

struct SensorMarshallUserEventEvent {
    void *callData;
    uint32_t origEvtType;
    void *origEvtData;
    TaggedPtr evtFreeingInfo;
};




struct SensorOps {
    bool (*sensorPower)(bool on, void *);          /* -> SENSOR_INTERNAL_EVT_POWER_STATE_CHG (success)         */
    bool (*sensorFirmwareUpload)(void *);    /* -> SENSOR_INTERNAL_EVT_FW_STATE_CHG (rate or 0 if fail)  */
    bool (*sensorSetRate)(uint32_t rate, uint64_t latency, void *);
                                           /* -> SENSOR_INTERNAL_EVT_RATE_CHG (rate)                   */
    bool (*sensorFlush)(void *); //trigger a measurement for ondemand sensors (if supported)
    bool (*sensorTriggerOndemand)(void *);
    bool (*sensorCalibrate)(void *);
    bool (*sensorCfgData)(void *cfgData, void *);

    bool (*sensorSendOneDirectEvt)(void *, uint32_t tid); //resend last state (if known), only for onchange-supporting sensors, to bring on a new client

    bool (*sensorMarshallData)(uint32_t yourEvtType, const void *yourEvtData, TaggedPtr *evtFreeingInfoP, void *); //marshall yourEvt for sending to host. Send a EVT_MARSHALLED_SENSOR_DATA event with marshalled data. Always send event, even on error, free the passed-in event using osFreeRetainedEvent
};

enum SensorInfoFlags1 {
    SENSOR_INFO_FLAGS1_BIAS = (1 << 0),
    SENSOR_INFO_FLAGS1_RAW  = (1 << 1),
};

struct SensorInfo {
    const char *sensorName; /* sensors.c code does not use this */

    /* Specify a list of rates supported in sensorSetRate, using a 0 to mark the
       end of the list.

       If SENSOR_RATE_ONCHANGE is included in this list, then sensor events
       should only be sent on data changes, regardless of any underlying
       sampling rate. In this case, the sensorSendOneDirectEvt callback will be
       invoked on each call to sensorRequest() to send new clients initial data.

       If SENSOR_RATE_ONDEMAND is included in this list, then the
       sensorTriggerOndemand callback must be implemented.

       If this list contains only explicit rates in Hz, then sensorRequests with
       SENSOR_RATE_ONCHANGE or ONDEMAND will be rejected.

       If NULL, the expectation is that rate is not applicable/configurable, and
       only SENSOR_RATE_ONCHANGE or SENSOR_RATE_ONDEMAND will be accepted, but
       neither on-change semantics or on-demand support is implied. */
    const uint32_t *supportedRates;

    uint8_t sensorType;
    uint8_t numAxis; /* enum NumAxis */
    uint8_t interrupt; /* interrupt to generate to AP */
    uint8_t flags1; /* enum SensorInfoFlags1 */
    uint16_t minSamples; /* minimum host fifo size (in # of samples) */
    uint8_t biasType;
    uint8_t rawType;
    uint16_t pad;
    float rawScale;
};


/*
 * Sensor rate is encoded as a 32-bit integer as number of samples it can
 * provide per 1024 seconds, allowing representations of all useful values
 * well. This define is to be used for static values only please, as GCC
 * will convert it into a const int at compile time. Do not use this at
 * runtime please. A few Magic values exist at both ends of the range
 * 0 is used as a list sentinel and high numbers for special abilities.
 */
#define SENSOR_RATE_ONDEMAND    0xFFFFFF00UL
#define SENSOR_RATE_ONCHANGE    0xFFFFFF01UL
#define SENSOR_RATE_ONESHOT     0xFFFFFF02UL
#define SENSOR_HZ(_hz)          ((uint32_t)((_hz) * 1024.0f))

/*
 * Sensor latency is a 64-bit integer specifying the allowable delay in ns
 * that data can be buffered.
 */
#define SENSOR_LATENCY_NODATA   0xFFFFFFFFFFFFFF00ULL

/*
 * sensors module api
 */
bool sensorsInit(void);

/*
 * Api for sensor drivers
 */
#define SENSOR_INTERNAL_EVT_POWER_STATE_CHG  0
#define SENSOR_INTERNAL_EVT_FW_STATE_CHG     1
#define SENSOR_INTERNAL_EVT_RATE_CHG         2

uint32_t sensorRegister(const struct SensorInfo *si, const struct SensorOps *ops, void *callData, bool initComplete); /* returns handle, copy is not made */
uint32_t sensorRegisterAsApp(const struct SensorInfo *si, uint32_t tid, void *callData, bool initComplete); /* returns handle, copy is not made */
bool sensorRegisterInitComplete(uint32_t handle);
bool sensorUnregister(uint32_t handle); /* your job to be sure it is off already */
bool sensorSignalInternalEvt(uint32_t handle, uint32_t intEvtNum, uint32_t value1, uint64_t value2);

#define sensorGetMyEventType(_sensorType) (EVT_NO_FIRST_SENSOR_EVENT + (_sensorType))


/*
 * api for using sensors (enum is not synced with sensor sub/unsub, this is ok since we do not expect a lot of dynamic sub/unsub)
 */
const struct SensorInfo* sensorFind(uint32_t sensorType, uint32_t idx, uint32_t *handleP); //enumerate all sensors of a type
bool sensorRequest(uint32_t clientTid, uint32_t sensorHandle, uint32_t rate, uint64_t latency);
bool sensorRequestRateChange(uint32_t clientTid, uint32_t sensorHandle, uint32_t newRate, uint64_t newLatency);
bool sensorRelease(uint32_t clientTid, uint32_t sensorHandle);
bool sensorTriggerOndemand(uint32_t clientTid, uint32_t sensorHandle);
bool sensorFlush(uint32_t sensorHandle);
bool sensorCalibrate(uint32_t sensorHandle);
bool sensorCfgData(uint32_t sensorHandle, void* cfgData);
uint32_t sensorGetCurRate(uint32_t sensorHandle);
uint64_t sensorGetCurLatency(uint32_t sensorHandle);
bool sensorGetInitComplete(uint32_t sensorHandle); // DO NOT poll on this value
bool sensorMarshallEvent(uint32_t sensorHandle, uint32_t evtType, void *evtData, TaggedPtr *evtFreeingInfoP);


/*
 * convenience funcs
 */
static inline uint64_t sensorTimerLookupCommon(const uint32_t *supportedRates, const uint64_t *timerVals, uint32_t wantedRate)
{
    uint32_t rate;

    while ((rate = *supportedRates++) != 0) {
        if (rate == wantedRate)
            return *timerVals;
        timerVals++;
    }

    return 0;
}


#ifdef __cplusplus
}
#endif

#endif
