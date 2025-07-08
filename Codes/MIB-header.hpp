/*
 * File: MIB-header.hpp
 * Author: Amneh Alissa
 * Description:
 *   Defines the L-SNMPvS Management Information Base (MIB) class and related functions.
 *   Manages device and sensor data, supports querying and updating MIB objects.
 * 
 * Main Classes/Functions:
 *   - class LSNMP_MIB: Stores and manages all device and sensor objects.
 *   - LSNMP_Timestamp getAgentUptime(): Returns agent uptime as a timestamp.
 * 
 * Data Types/Variables:
 *   - agentRebootTime: Global variable tracking agent start time.
 *   - deviceData, sensorsData: Maps storing device and sensor values.
 *   - LSNMP_IID, LSNMP_Value, LSNMP_Timestamp: Data structures for MIB objects.
 *   - LSNMP_ValueType: Enum for value types (INTEGER, STRING, TIMESTAMP).
 */

#ifndef LSNMP_MIB_HPP
#define LSNMP_MIB_HPP

#pragma once  // Ensure this header is included only once

#include "PDU-header2.hpp"

// Declare some global functions and variables here but defined in agent.cpp
LSNMP_Timestamp getAgentUptime();
extern std::chrono::steady_clock::time_point agentRebootTime;

// Main MIB class to manage device and sensor data
class LSNMP_MIB {
public:
    // ===== Define the MIB structures =====
    LSNMP_MIB() {
        // Device Structure (Structure 1, indexed by deviceIndex)
        // Device 1
        addDeviceObject(1, 1, LSNMP_ValueType::INTEGER, 42);                    // lMibId
        addDeviceObject(1, 2, LSNMP_ValueType::STRING, 0, "00:11:22:33:44:55"); // id (e.g. MAC)
        addDeviceObject(1, 3, LSNMP_ValueType::STRING, 0, "Sensing Hub 1");     // type
        addDeviceObject(1, 4, LSNMP_ValueType::INTEGER, 5);                     // beaconRate
        addDeviceObject(1, 5, LSNMP_ValueType::INTEGER, 2);                     // nSensors
        {
            // for object 6 (dateAndTime), we need to get the current time
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm* tm_now = std::localtime(&t);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            LSNMP_Timestamp ts;
            ts.day = tm_now->tm_mday;
            ts.month = tm_now->tm_mon + 1;
            ts.year = tm_now->tm_year + 1900;
            ts.hour = tm_now->tm_hour;
            ts.minute = tm_now->tm_min;
            ts.second = tm_now->tm_sec;
            ts.millisecond = static_cast<uint16_t>(ms.count());

            addDeviceObject(1, 6, LSNMP_ValueType::TIMESTAMP, 0, ts); // dateAndTime
        }
        addDeviceObject(1, 7, LSNMP_ValueType::TIMESTAMP, 0, getAgentUptime()); // uptime
        addDeviceObject(1, 8, LSNMP_ValueType::INTEGER, 1);                     // opStatus
        addDeviceObject(1, 9, LSNMP_ValueType::INTEGER, 0);                     // reset

        // Device 2
        addDeviceObject(2, 1, LSNMP_ValueType::INTEGER, 43);
        addDeviceObject(2, 2, LSNMP_ValueType::STRING, 0, "66:77:88:99:AA:BB");
        addDeviceObject(2, 3, LSNMP_ValueType::STRING, 0, "Sensing Hub 2");
        addDeviceObject(2, 4, LSNMP_ValueType::INTEGER, 10);
        addDeviceObject(2, 5, LSNMP_ValueType::INTEGER, 2);
        {
            // for object 6 (dateAndTime), we need to get the current time
            auto now = std::chrono::system_clock::now();
            std::time_t t = std::chrono::system_clock::to_time_t(now);
            std::tm* tm_now = std::localtime(&t);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

            LSNMP_Timestamp ts;
            ts.day = tm_now->tm_mday;
            ts.month = tm_now->tm_mon + 1;
            ts.year = tm_now->tm_year + 1900;
            ts.hour = tm_now->tm_hour;
            ts.minute = tm_now->tm_min;
            ts.second = tm_now->tm_sec;
            ts.millisecond = static_cast<uint16_t>(ms.count());

            addDeviceObject(2, 6, LSNMP_ValueType::TIMESTAMP, 0, ts); // dateAndTime
        }
        addDeviceObject(2, 7, LSNMP_ValueType::TIMESTAMP, 0, getAgentUptime());
        addDeviceObject(2, 8, LSNMP_ValueType::INTEGER, 1);
        addDeviceObject(2, 9, LSNMP_ValueType::INTEGER, 0);

        // Sensor Structure (Structure 2, indexed by deviceIndex and sensorIndex)
        // Device 1 Sensors
        addSensorRow(1, 1, "00:11:22:33:44:66", "temperature", 20, -10, 40, {0,0,0,1,10,0,100}, 10);
        addSensorRow(1, 2, "00:11:22:33:44:77", "humidity", 50, 0, 100, {0,0,0,1,10,0,200}, 10);

        // Device 2 Sensors
        addSensorRow(2, 1, "66:77:88:99:AA:CC", "temperature", 20, -10, 40, {0,0,0,0,45,0,200}, 10);
        addSensorRow(2, 2, "66:77:88:99:AA:DD", "humidity", 50, 0, 100, {0,0,0,0,45,0,300}, 10);
    
        // Initialize device boot times (for uptime calculations)
        auto agentStart = std::chrono::system_clock::now() -
            (std::chrono::steady_clock::now() - agentRebootTime);
        deviceBootTimes[1] = agentStart;
        deviceBootTimes[2] = agentStart;
    }

    // ===== MIB Query Methods =====

    // Checks if the MIB has objects for the given IID
    bool has(const LSNMP_IID& iid) const {
        if (iid.object == 0) return true; // Object 0 is for number of objects in the group/table
        if (iid.structure == 1) return deviceData.count(iid) > 0;
        if (iid.structure == 2) return sensorsData.count(iid) > 0;
        return false;
    }

    // Normalizes the IID (this is to ensure object == 0 is handled correctly)
    // If object == 0, it returns a normalized IID with object set to 0 and index1/index2 set to 0
    LSNMP_IID normalize(const LSNMP_IID& iid) const {
        if (iid.object == 0) {
            return LSNMP_IID{iid.structure, 0, 0, 0, false, false};
        }
        return iid;
    }

    // Gets the value for a specific IID
    LSNMP_Value get(const LSNMP_IID& iid) const {
        LSNMP_IID norm = normalize(iid); // Normalize the IID first to handle if object == 0

        // Handle object == 0 (column count)
        if (norm.object == 0) {
            if (norm.structure == 1) return {LSNMP_ValueType::INTEGER, 9}; // static count...
            if (norm.structure == 2) return {LSNMP_ValueType::INTEGER, 7};
            throw std::out_of_range("Invalid structure for normalized object == 0");
        }

        // Handle device fields (structure 1)
        if (norm.structure == 1) {
            switch (norm.object) {
                case 6: { // System date & time (object 6)

                    // If set, reconstruct the timestamp
                    auto itBase = dateAndTimeSetBase.find(norm);
                    auto itVal = dateAndTimeSetValue.find(norm);
                    if (itBase != dateAndTimeSetBase.end() && itVal != dateAndTimeSetValue.end()) {
                        // Convert LSNMP_Timestamp to std::tm
                        const LSNMP_Timestamp& setTs = itVal->second;
                        std::tm tm_set = {};
                        tm_set.tm_mday = setTs.day;
                        tm_set.tm_mon  = setTs.month - 1;
                        tm_set.tm_year = setTs.year - 1900;
                        tm_set.tm_hour = setTs.hour;
                        tm_set.tm_min  = setTs.minute;
                        tm_set.tm_sec  = setTs.second;
                        auto base = std::chrono::system_clock::from_time_t(std::mktime(&tm_set))
                                    + std::chrono::milliseconds(setTs.millisecond);

                        auto now = std::chrono::system_clock::now();
                        auto elapsed = now - itBase->second;
                        auto current = base + elapsed;

                        std::time_t t = std::chrono::system_clock::to_time_t(current);
                        std::tm* tm_now = std::localtime(&t);
                        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(current.time_since_epoch()) % 1000;

                        LSNMP_Timestamp ts;
                        ts.day = tm_now->tm_mday;
                        ts.month = tm_now->tm_mon + 1;
                        ts.year = tm_now->tm_year + 1900;
                        ts.hour = tm_now->tm_hour;
                        ts.minute = tm_now->tm_min;
                        ts.second = tm_now->tm_sec;
                        ts.millisecond = static_cast<uint16_t>(ms.count());
                        return {LSNMP_ValueType::TIMESTAMP, 0, "", ts};
                    }

                    // If not set, return current time
                    auto now = std::chrono::system_clock::now();
                    std::time_t t = std::chrono::system_clock::to_time_t(now);
                    std::tm* tm_now = std::localtime(&t);
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

                    LSNMP_Timestamp ts;
                    ts.day = tm_now->tm_mday;
                    ts.month = tm_now->tm_mon + 1;
                    ts.year = tm_now->tm_year + 1900;
                    ts.hour = tm_now->tm_hour;
                    ts.minute = tm_now->tm_min;
                    ts.second = tm_now->tm_sec;
                    ts.millisecond = static_cast<uint16_t>(ms.count());
                    return {LSNMP_ValueType::TIMESTAMP, 0, "", ts};
                }

                case 7: { // Uptime (per device)
                    auto it = deviceBootTimes.find(norm.index1);
                    if (it != deviceBootTimes.end()) {
                        auto now = std::chrono::system_clock::now();
                        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
                        LSNMP_Timestamp ts;
                        ts.day = ms / (1000 * 60 * 60 * 24);
                        ms %= (1000 * 60 * 60 * 24);
                        ts.hour = ms / (1000 * 60 * 60);
                        ms %= (1000 * 60 * 60);
                        ts.minute = ms / (1000 * 60);
                        ms %= (1000 * 60);
                        ts.second = ms / 1000;
                        ts.millisecond = ms % 1000;
                        ts.month = 0;
                        ts.year = 0;
                        return {LSNMP_ValueType::TIMESTAMP, 0, "", ts};
                    }
                    break;
                }

                default: { // All other device objects (no special handling)
                    auto it = deviceData.find(norm);
                    if (it != deviceData.end()) return it->second;
                    break;
                }
            }
        }

        // Handle sensor fields (structure 2)
        if (norm.structure == 2) {

            // Special handling for lastSamplingTime (object 6)
            if (norm.object == 6) {
                auto it = sensorsData.find(norm);
                if (it != sensorsData.end() && it->second.type == LSNMP_ValueType::TIMESTAMP) {
                    // Get the absolute timestamp of the last sample
                    const LSNMP_Timestamp& absTs = it->second.tsValue;

                    // Convert absTs to std::tm
                    std::tm tm_sample = {};
                    tm_sample.tm_mday = absTs.day;
                    tm_sample.tm_mon  = absTs.month - 1;
                    tm_sample.tm_year = absTs.year - 1900;
                    tm_sample.tm_hour = absTs.hour;
                    tm_sample.tm_min  = absTs.minute;
                    tm_sample.tm_sec  = absTs.second;
                    auto sample_tp = std::chrono::system_clock::from_time_t(std::mktime(&tm_sample))
                                    + std::chrono::milliseconds(absTs.millisecond);

                    // Get current time
                    auto now = std::chrono::system_clock::now();

                    // Get agent reboot time as system_clock
                    auto agentUptime = getAgentUptime();
                    auto agentStart = now - std::chrono::milliseconds(
                        ((uint64_t)agentUptime.day) * 24 * 60 * 60 * 1000 +
                        ((uint64_t)agentUptime.hour) * 60 * 60 * 1000 +
                        ((uint64_t)agentUptime.minute) * 60 * 1000 +
                        ((uint64_t)agentUptime.second) * 1000 +
                        ((uint64_t)agentUptime.millisecond)
                    );

                    // Calculate time since agent reboot when sample was taken
                    auto ms_since_reboot = std::chrono::duration_cast<std::chrono::milliseconds>(sample_tp - agentStart).count();
                    if (ms_since_reboot < 0) ms_since_reboot = 0; // Clamp to 0

                    // Convert ms_since_reboot to LSNMP_Timestamp (relative)
                    LSNMP_Timestamp relTs;
                    relTs.day = ms_since_reboot / (1000 * 60 * 60 * 24);
                    ms_since_reboot %= (1000 * 60 * 60 * 24);
                    relTs.hour = ms_since_reboot / (1000 * 60 * 60);
                    ms_since_reboot %= (1000 * 60 * 60);
                    relTs.minute = ms_since_reboot / (1000 * 60);
                    ms_since_reboot %= (1000 * 60);
                    relTs.second = ms_since_reboot / 1000;
                    relTs.millisecond = ms_since_reboot % 1000;
                    relTs.month = 0;
                    relTs.year = 0;
                    return {LSNMP_ValueType::TIMESTAMP, 0, "", relTs};
                }
            }
            
            // All other sensor objects
            auto it = sensorsData.find(norm);
            if (it != sensorsData.end()) return it->second;
        }

        throw std::out_of_range("Invalid IID structure or object");
    }

    // Sets the value for a specific IID
    // Returns true if successful, false if not writable or invalid
    bool set(const LSNMP_IID& iid, const LSNMP_Value& value) {

        // Device objects: beaconRate (4), dateAndTime (6), reset (9) are writable
        if (iid.structure == 1 &&
            (iid.object == 4 || iid.object == 6 || iid.object == 9) &&
            deviceData.count(iid) &&
            deviceData.at(iid).type == value.type) {

            // Restrict reset (object 9) to only accept 0 or 1
            if (iid.object == 9) {
                if (value.intValue != 0 && value.intValue != 1) {
                    return false; // Only accept 0 or 1
                }
                if (value.intValue == 1) {
                    // Reset only this device's uptime
                    deviceBootTimes[iid.index1] = std::chrono::system_clock::now();
                }
                deviceData[iid] = value;
                return true;
            }

            // Only accept TIMESTAMP format for dateAndTime (object 6)
            if (iid.object == 6) {
                if (value.type != LSNMP_ValueType::TIMESTAMP) {
                    return false; // Only accept TIMESTAMP
                }
                deviceData[iid] = value;
                dateAndTimeSetBase[iid] = std::chrono::system_clock::now();
                dateAndTimeSetValue[iid] = value.tsValue;
                return true;
            }

            // For beaconRate (4), just set as usual
            deviceData[iid] = value;
            return true;
        }

        // Sensor objects: Sample rate (7) is writable
        if (iid.structure == 2 && iid.object == 7 &&
            sensorsData.count(iid) &&
            sensorsData.at(iid).type == value.type) {

            sensorsData[iid] = value;
            return true;
        }

        // Other objects are not writable
        return false;
    }

    // Used to record the last update time for each IID (after a SET operation)
    std::map<LSNMP_IID, std::chrono::steady_clock::time_point> lastUpdateTimes;

    // Record the current time as the last update time for a specific IID
    void recordUpdateTime(const LSNMP_IID& iid) {
        lastUpdateTimes[iid] = std::chrono::steady_clock::now();
    }

    // Get the age of the last update for a specific IID
    bool getLastUpdateAge(const LSNMP_IID& iid, LSNMP_Timestamp& age) const {
        auto it = lastUpdateTimes.find(iid);
        if (it == lastUpdateTimes.end()) return false;

        using namespace std::chrono;
        auto ms = duration_cast<milliseconds>(steady_clock::now() - it->second).count();
        age.day = ms / (1000 * 60 * 60 * 24);
        ms %= (1000 * 60 * 60 * 24);
        age.hour = ms / (1000 * 60 * 60);
        ms %= (1000 * 60 * 60);
        age.minute = ms / (1000 * 60);
        ms %= (1000 * 60);
        age.second = ms / 1000;
        age.millisecond = ms % 1000;
        age.month = 0;
        age.year = 0;
        return true;
    }

    // Getters for device and sensor data
    const std::map<LSNMP_IID, LSNMP_Value>& getAllDeviceData() const { return deviceData; }
    const std::map<LSNMP_IID, LSNMP_Value>& getAllSensorData() const { return sensorsData; }

    // Internal-only: update sampleValue (object 3)
    void updateSampleValue(const LSNMP_IID& iid, const LSNMP_Value& value) {
        sensorsData[iid] = value;
    }
    // Internal-only: update lastSamplingTime (object 6)
    void updateLastSamplingTime(const LSNMP_IID& iid, const LSNMP_Value& value) {
        sensorsData[iid] = value;
    }

private:
    // ===== Data Storage =====
    std::map<LSNMP_IID, LSNMP_Value> deviceData;
    std::map<LSNMP_IID, LSNMP_Value> sensorsData;

    // For tracking set values and set times for dateAndTime (object 1.6)
    mutable std::map<LSNMP_IID, std::chrono::system_clock::time_point> dateAndTimeSetBase;
    mutable std::map<LSNMP_IID, LSNMP_Timestamp> dateAndTimeSetValue;

    // For tracking device up times (object 1.7)
    std::map<uint16_t, std::chrono::system_clock::time_point> deviceBootTimes;


    // ===== Helpers for Adding Objects =====

    // Device Object Helper (Structure 1, object with device index and integer value)
    void addDeviceObject(uint16_t deviceIndex, uint8_t object, LSNMP_ValueType type, int intValue) {
        LSNMP_IID iid{1, object, deviceIndex, 0, true, false};
        deviceData[iid] = {type, intValue};
    }

    // Device Object Helper (Structure 1, object with device index and string value)
    void addDeviceObject(uint16_t deviceIndex, uint8_t object, LSNMP_ValueType type, uint16_t, const std::string& strVal) {
        LSNMP_IID iid{1, object, deviceIndex, 0, true, false};
        deviceData[iid] = {type, 0, strVal};
    }

    // Device Object Helper (Structure 1, object with device index and timestamp value)
    void addDeviceObject(uint16_t deviceIndex, uint8_t object, LSNMP_ValueType type, uint16_t, const LSNMP_Timestamp& tsVal) {
        LSNMP_IID iid{1, object, deviceIndex, 0, true, false};
        deviceData[iid] = {type, 0, "", tsVal};
    }

    // Sensor Table Helper (Structure 2, object with device and sensor index)
    void addSensorRow(uint16_t deviceIndex, uint16_t sensorIndex,
                      const std::string& idStr,
                      const std::string& typeStr,
                      int sampleValue,
                      int minValue,
                      int maxValue,
                      const LSNMP_Timestamp& lastSamplingTime,
                      int samplingRate) {
        sensorsData[{2, 1, deviceIndex, sensorIndex, true, true}] = {LSNMP_ValueType::STRING, 0, idStr};
        sensorsData[{2, 2, deviceIndex, sensorIndex, true, true}] = {LSNMP_ValueType::STRING, 0, typeStr};
        sensorsData[{2, 3, deviceIndex, sensorIndex, true, true}] = {LSNMP_ValueType::INTEGER, sampleValue};
        sensorsData[{2, 4, deviceIndex, sensorIndex, true, true}] = {LSNMP_ValueType::INTEGER, minValue};
        sensorsData[{2, 5, deviceIndex, sensorIndex, true, true}] = {LSNMP_ValueType::INTEGER, maxValue};
        sensorsData[{2, 6, deviceIndex, sensorIndex, true, true}] = {LSNMP_ValueType::TIMESTAMP, 0, "", lastSamplingTime};
        sensorsData[{2, 7, deviceIndex, sensorIndex, true, true}] = {LSNMP_ValueType::INTEGER, samplingRate};
    }
};

#endif
