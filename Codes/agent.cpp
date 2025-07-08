/*
 * File: agent.cpp
 * Author: Amneh Alissa
 * Description:
 *   Implements the L-SNMPvS agent. Handles UDP communication, MIB management,
 *   sensor simulation, and beacon broadcasting.
 * 
 * Main Functions/Classes:
 *   - main(): Runs the agent event loop.
 *   - sensorUpdateThread(): Simulates sensor value changes.
 *   - sendBeacon(): Broadcasts device status.
 *   - getAgentUptime(): Calculates agent uptime.
 *   - getCurrentTimestamp(): Gets the current absolute timestamp.
 * 
 * Data Types/Variables:
 *   - LSNMP_MIB: MIB instance for device/sensor data.
 *   - agentRebootTime: Tracks agent start time.
 *   - running: Controls sensor update thread.
 *   - mibMutex: Mutex for thread-safe MIB access.
 *   - PORT, BUFFER_SIZE: Network configuration constants.
 */

#include "PDU-header2.hpp"
#include "MIB-header.hpp"

#define PORT 9000 // Port number for L-SNMPvS agent
#define BUFFER_SIZE 1024 // Buffer size for receiving data

// Global variables
std::chrono::steady_clock::time_point agentRebootTime = std::chrono::steady_clock::now(); // Agent start time
std::atomic<bool> running{true}; // Control flag for sensor update thread
std::mutex mibMutex; // Mutex for thread-safe MIB access

// --- Function Definitions ---

// Function to return the agent's uptime as a relative timestamp
LSNMP_Timestamp getAgentUptime() {
    using namespace std::chrono;
    auto now = steady_clock::now();
    auto ms = duration_cast<milliseconds>(now - agentRebootTime).count();
    LSNMP_Timestamp rt;
    rt.day = ms / (1000 * 60 * 60 * 24);
    ms %= (1000 * 60 * 60 * 24);
    rt.hour = ms / (1000 * 60 * 60);
    ms %= (1000 * 60 * 60);
    rt.minute = ms / (1000 * 60);
    ms %= (1000 * 60);
    rt.second = ms / 1000;
    rt.millisecond = ms % 1000;
    rt.month = 0; // Relative time
    rt.year = 0;  // Relative time
    return rt;
}

// Function to get the current timestamp in absolute format
LSNMP_Timestamp getCurrentTimestamp() {
    using namespace std::chrono;

    auto now = system_clock::now();
    auto ms = duration_cast<milliseconds>(now.time_since_epoch());
    auto s = duration_cast<seconds>(ms);
    auto ms_part = ms - s;

    std::time_t time_now = system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&time_now);

    LSNMP_Timestamp ts;
    ts.day = tm_now->tm_mday;
    ts.month = tm_now->tm_mon + 1;
    ts.year = tm_now->tm_year + 1900;
    ts.hour = tm_now->tm_hour;
    ts.minute = tm_now->tm_min;
    ts.second = tm_now->tm_sec;
    ts.millisecond = static_cast<uint16_t>(ms_part.count());

    return ts;
}

// Thread function to simulate sensor updates
void sensorUpdateThread(LSNMP_MIB& mib) {
    std::default_random_engine rng(std::random_device{}());
    while (running) {
        std::lock_guard<std::mutex> lock(mibMutex);
        for (const auto& entry : mib.getAllSensorData()) {
            const LSNMP_IID& iid = entry.first;
            if (iid.object == 3) { // sampleValue
                // Get min/max for this sensor
                LSNMP_IID minIID = iid; minIID.object = 4;
                LSNMP_IID maxIID = iid; maxIID.object = 5;
                int min = mib.get(minIID).intValue;
                int max = mib.get(maxIID).intValue;

                // Generate new sample value
                int newValue = 0;
                std::string type = mib.get({2, 2, iid.index1, iid.index2, true, true}).strValue;
                if (type == "temperature") {
                    std::uniform_int_distribution<int> dist(min, max);
                    newValue = dist(rng);
                } else if (type == "humidity") {
                    std::uniform_int_distribution<int> dist(min, max);
                    newValue = dist(rng); // always 0-100
                }

                // Update sampleValue
                LSNMP_Value val;
                val.type = LSNMP_ValueType::INTEGER;
                val.intValue = newValue;
                mib.updateSampleValue(iid, val);

                // Update lastSamplingTime (absolute timestamp)
                LSNMP_Value tsVal;
                tsVal.type = LSNMP_ValueType::TIMESTAMP;
                tsVal.tsValue = getCurrentTimestamp();
                mib.updateLastSamplingTime({2, 6, iid.index1, iid.index2, true, true}, tsVal);
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10Hz
    }
}

// Function to send a beacon notification to managers
void sendBeacon(int sockfd, const LSNMP_MIB& mib) {
    LSNMP_PDU beacon;
    beacon.tag = {'L','S','N','M','P','v','S','1'};
    beacon.type = NOTIFICATION;
    beacon.messageID = 0; // Special ID for beacons
    beacon.timestamp = getCurrentTimestamp();

    // Add device objects for all devices in the mib to beacon
    std::vector<uint8_t> deviceIndices = {1, 2}; // all device indices in MIB

    for (uint8_t devIndex : deviceIndices) {
        // Add the main device info fields (as per assignment)
        beacon.iidList.push_back(LSNMP_IID{1, 1, devIndex, 0, true, false}); // lMibId
        beacon.iidList.push_back(LSNMP_IID{1, 2, devIndex, 0, true, false}); // id
        beacon.iidList.push_back(LSNMP_IID{1, 3, devIndex, 0, true, false}); // type
        beacon.iidList.push_back(LSNMP_IID{1, 5, devIndex, 0, true, false}); // nSensors
        beacon.iidList.push_back(LSNMP_IID{1, 6, devIndex, 0, true, false}); // dateAndTime
        beacon.iidList.push_back(LSNMP_IID{1, 7, devIndex, 0, true, false}); // upTime
        beacon.iidList.push_back(LSNMP_IID{1, 8, devIndex, 0, true, false}); // opStatus
    }

    // Retrieve values for beacon.iidList entries
    for (const auto& iid : beacon.iidList) {
        if (mib.has(iid)) {
            beacon.valueList.push_back(mib.get(iid));
            beacon.timeList.push_back(getAgentUptime());
        } else {
            beacon.valueList.push_back({LSNMP_ValueType::INTEGER, 0});
            beacon.timeList.push_back({0, 0, 0, 0});
        }
    }

    // Broadcast to all managers
    struct sockaddr_in broadcastAddr{};
    broadcastAddr.sin_family = AF_INET;
    broadcastAddr.sin_port = htons(PORT);
    broadcastAddr.sin_addr.s_addr = inet_addr("255.255.255.255");

    std::vector<uint8_t> encoded = encodePDU(beacon);
    sendto(sockfd, encoded.data(), encoded.size(), 0,
           (struct sockaddr*)&broadcastAddr, sizeof(broadcastAddr));
    std::cout << "[Beacon] Sent beacon notification.\n\n";
}

// Main function to run the L-SNMPvS agent
int main() {

    // Setup socket for UDP communication
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in server_addr{}, client_addr{};
    socklen_t client_len = sizeof(client_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Enable broadcast option
    int broadcastEnable = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    // Initialize MIB and start sensor update thread
    LSNMP_MIB mib;
    std::thread updater(sensorUpdateThread, std::ref(mib));
    std::cout << "L-SNMPvS Agent listening on port " << PORT << "...\n\n";

    // Beacon handling
    time_t lastBeacon = 0;
    const int BEACON_INTERVAL = 10; // seconds

    // Main event loop
    while (true) {
        time_t now = time(nullptr);

        // check if it's time to send a beacon
        if (now - lastBeacon >= BEACON_INTERVAL) {
            sendBeacon(sockfd, mib);
            lastBeacon = now;
        }

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // Set a timeout for select to avoid blocking indefinitely
        struct timeval tv;
        tv.tv_sec = 1; // 1 second timeout
        tv.tv_usec = 0;

        // Wait up to 1 second for incoming data because we are sending beacons every 10 seconds
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &tv);
        if (activity > 0 && FD_ISSET(sockfd, &readfds)) {
            memset(buffer, 0, BUFFER_SIZE);
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&client_addr, &client_len);
            if (n <= 0) continue;

            std::vector<uint8_t> data(buffer, buffer + n);
            std::vector<uint8_t> pduBytes, senderID, receiverID;
            uint8_t secModel;
            bool isSecure = decodeSecureMessage(data, pduBytes, senderID, receiverID, secModel); // Decode the received message

            // Check for beacon (broadcast, no security)
            bool isBeacon = (secModel == SEC_MODEL_NONE) || (receiverID == BROADCAST_ID);
            if (isBeacon) {
                continue;
            }

            // If not secure, ignore the message
            if (!isSecure) {
                std::cerr << "[Agent] Security check failed or invalid message.\n";
                continue;
            }

            std::cout << "[Manager] Received UDP packet, length: " << n << "\n";
            LSNMP_PDU receivedPDU = decodePDU(pduBytes); // decode the PDU

            std::cout << "[Received PDU] MSG-ID: " << receivedPDU.messageID << "\n";

            // Prepare the response PDU
            LSNMP_PDU response;
            response.tag = {'L','S','N','M','P','v','S','1'};
            response.type = RESPONSE;
            response.messageID = receivedPDU.messageID;
            response.timestamp = getCurrentTimestamp();

            // Process the received PDU based on its type
            for (size_t i = 0; i < receivedPDU.iidList.size(); ++i) {
                const auto& iid = receivedPDU.iidList[i];

                if (receivedPDU.type == GET_REQUEST) {

                    if (mib.has(iid)) {
                        response.iidList.push_back(iid);
                        response.valueList.push_back(mib.get(iid)); // Get the value from MIB

                        // Check if this is a read-write object, to determine the timeList entry (if there was a last update)
                        if ((iid.structure == 1 && (iid.object == 4 || iid.object == 6 || iid.object == 9)) ||
                            (iid.structure == 2 && iid.object == 7)) {
                                LSNMP_Timestamp age;
                                if (mib.getLastUpdateAge(iid, age)) { // Get the age since last update
                                    age.year = 0; // Relative time
                                    age.month = 0; // Relative time
                                    response.timeList.push_back(age);
                                } else {
                                    response.timeList.push_back(getAgentUptime());
                                }
                        } else {
                            // if not a read-write object, the age will be the same as agent uptime
                            response.timeList.push_back(getAgentUptime());
                        }
                        // No error for valid IID
                        response.errorList.push_back(NO_ERROR);

                    } else {
                        // If the IID is not found in MIB, return INVALID_IID error
                        response.iidList.push_back(iid);
                        LSNMP_Value v;
                        v.type = LSNMP_ValueType::INTEGER;
                        v.intValue = 0;
                        response.valueList.push_back(v);
                        response.timeList.push_back({0, 0, 0, 0});
                        response.errorList.push_back(INVALID_IID);
                    }

                } else if (receivedPDU.type == SET_REQUEST) {
                    const auto& val = receivedPDU.valueList[i]; // value to set
                    bool success = false; // Flag to track if set was successful

                    // Try setting a writable (device) field
                    if (mib.set(iid, val)) {
                        response.iidList.push_back(iid);
                        success = true;
                        mib.recordUpdateTime(iid); // we have to record the update time for this IID after a successful SET

                        // Specific case: If reset was set to 1, reset the agent's uptime
                        if (iid.structure == 1 && iid.object == 9 && val.intValue == 1) {
                            agentRebootTime = std::chrono::steady_clock::now();
                            // Optionally, set reset back to 0 after performing reset
                            LSNMP_Value resetZero = val;
                            resetZero.intValue = 0;
                            mib.set(iid, resetZero);
                        }
                    }

                    // If set was successful, return the value that was set
                    if (success) {
                        response.valueList.push_back(val); // return the value that was set
                        response.timeList.push_back(LSNMP_Timestamp{0, 0, 0, 0});
                        response.errorList.push_back(NO_ERROR);
                    }
                    // If set was not successful, check if it's a writable object
                    else if ((iid.structure == 1 && mib.has(iid)) || (iid.structure == 2 && mib.has(iid))) {
                        response.iidList.push_back(iid);
                        response.valueList.push_back(mib.get(iid));

                        /// Check if this is a read-write object, to determine the timeList entry
                        if ((iid.structure == 1 && (iid.object == 4 || iid.object == 6 || iid.object == 9)) ||
                            (iid.structure == 2 && iid.object == 7)) { // these are writable objects
                            LSNMP_Timestamp age;
                            if (mib.getLastUpdateAge(iid, age)) {
                                age.year = 0; // Relative time
                                age.month = 0; // Relative time
                                response.timeList.push_back(age);
                            } else {
                                // If no last update age, use agent uptime
                                response.timeList.push_back(getAgentUptime());
                            }
                        } else {
                            // if not a read-write object, the age will be the same as agent uptime
                            response.timeList.push_back(getAgentUptime());
                        }

                        // Add appropriate error based on the object type
                        if (!mib.set(iid, val)) {
                            if (iid.structure == 1 && iid.object == 9 && (val.intValue != 0 && val.intValue != 1)) {
                                response.errorList.push_back(UNSUPPORTED_VALUE); // 7 for reset
                            } else if (iid.structure == 1 && iid.object == 6) {
                                response.errorList.push_back(UNSUPPORTED_VALUE); // 7 for dateAndTime format error
                            } else {
                                response.errorList.push_back(NOT_WRITABLE); // 9 for all other not-writable
                            }
                        }

                    // If the IID is not found in MIB, return INVALID_IID error
                    } else {
                        response.iidList.push_back(iid);
                        LSNMP_Value v;
                        v.type = LSNMP_ValueType::INTEGER;
                        v.intValue = 0;
                        response.valueList.push_back(v);
                        response.timeList.push_back({0, 0, 0, 0});
                        response.errorList.push_back(INVALID_IID);  // Unknown IID
                    }
                }
            }
            
                    // encode the response PDU & then secure it
                    std::vector<uint8_t> encoded = encodePDU(response);
                    std::vector<uint8_t> secured = encodeSecureMessage(encoded, AGENT_ID, senderID, SEC_MODEL_SIMPLE);

                    // Uncomment for debugging: print plain PDU vs secured message as hexdump
                    // for (auto b : encoded) printf("%02X ", b); printf("\n\n\n"); // Plain PDU hexdump
                    // for (auto b : secured) printf("%02X ", b); printf("\n"); // Secured message hexdump

                    // Uncomment for debugging: to test security, flip a bit in the secured data (for debugging only)
                    // if (secured.size() > 21) secured[21] ^= 0xFF;
                    
                    // Send the secured response back to the manager
                    sendto(sockfd, secured.data(), secured.size(), 0, (struct sockaddr*)&client_addr, client_len);
                    std::cout << "  -> Response sent.\n\n";
        }
    }

    close(sockfd);

    running = false; // Stop the sensor update thread
    updater.join(); // Wait for the thread to finish before exiting main

    return 0;
}
