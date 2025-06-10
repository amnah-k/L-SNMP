/*
 * File: manager.cpp
 * Author: Amneh Alissa
 * Description:
 *   Implements the L-SNMPvS manager console. Sends GET/SET requests, receives responses,
 *   and displays device/sensor information.
 * 
 * Main Functions/Classes:
 *   - main(): Runs the manager command loop.
 *   - parseIID(): Parses IID strings into LSNMP_IID structures.
 *   - sendRequest(): Sends requests and prints responses.
 *   - printSummary(): Prints summary of all devices and sensors.
 *   - describeIID(): Returns a human-readable description of an IID.
 *   - describeError(): Returns a human-readable description of an error code.
 * 
 * Data Types/Variables:
 *   - LSNMP_PDU: Structure for protocol data units.
 *   - LSNMP_IID, LSNMP_Value: Identifiers and values for MIB objects.
 *   - lastRequest, lastResponse: Store last sent/received PDUs for debugging.
 */

#include "PDU-header2.hpp"

#define PORT 9000 // Port number for L-SNMPvS
#define BUFFER_SIZE 1024 // Buffer size for receiving data

// Global variables to store last request and response PDUs for debugging
LSNMP_PDU lastRequest;
LSNMP_PDU lastResponse;

// --- Function Definitions ---

// Function to parse an IID string into LSNMP_IID structure (with optional formats)
bool parseIID(const std::string& str, LSNMP_IID& iid) {
    int structure = 0, object = 0, index1 = 1, index2 = 0; // defaults
    char dot1, dot2, dot3;
    std::istringstream ss(str);

    // Try full form with three dots: structure.object.index1.index2
    if (ss >> structure >> dot1 >> object >> dot2 >> index1 >> dot3 >> index2) {
        if (dot1 == '.' && dot2 == '.' && dot3 == '.') {
            iid.structure = static_cast<uint8_t>(structure);
            iid.object = static_cast<uint8_t>(object);
            iid.index1 = static_cast<uint16_t>(index1);
            iid.index2 = static_cast<uint16_t>(index2);
            iid.hasIndex1 = true;
            iid.hasIndex2 = true;
            return true;
        }
    }

    // For sensors, do not allow short forms! (Because they require both indices)
    // Only allow short forms for structure 1 (devices)
    ss.clear();
    ss.str(str);
    if (ss >> structure >> dot1 >> object >> dot2 >> index1) {
        if (dot1 == '.' && dot2 == '.') {
            iid.structure = static_cast<uint8_t>(structure);
            iid.object = static_cast<uint8_t>(object);
            iid.index1 = static_cast<uint16_t>(index1);
            if (iid.structure == 1) {
                iid.hasIndex1 = true;
                iid.hasIndex2 = false;
                iid.index2 = 0;
                return true;
            } else {
                // For sensors, require full IID
                return false;
            }
        }
    }

    // Shortest form: structure.object (also only for devices)
    ss.clear();
    ss.str(str);
    if (ss >> structure >> dot1 >> object) {
        if (dot1 == '.') {
            iid.structure = static_cast<uint8_t>(structure);
            iid.object = static_cast<uint8_t>(object);
            iid.index1 = 1;      // default index1
            iid.hasIndex1 = true;
            iid.index2 = 0;      // default index2
            iid.hasIndex2 = false;
            return iid.structure == 1; // Only allow for devices
        }
    }

    return false;
}

// Function to describe the returned IID in the response in a human-readable format
std::string describeIID(const LSNMP_IID& iid) {
    std::ostringstream desc;

    if (iid.structure == 1) { // Device Group
        switch (iid.object) {
            case 0: desc << "# of objects in group/table"; break;
            case 1: desc << "L-MIB ID"; break;
            case 2: desc << "Device ID"; break;
            case 3: desc << "Device Type"; break;
            case 4: desc << "Beacon Rate (s)"; break;
            case 5: desc << "Sensor Count"; break;
            case 6: desc << "System Date and Time"; break;
            case 7: desc << "Device Uptime"; break;
            case 8: desc << "Operational Status"; break;
            case 9: desc << "Reset Flag"; break;
            default: desc << "Unknown Device Object"; break;
        }
        if (iid.hasIndex1) {
            desc << " [Device " << iid.index1 << "]";
        }
    } else if (iid.structure == 2) { // Sensors Table
        switch (iid.object) {
            case 0: desc << "# of objects in group/table"; break;
            case 1: desc << "Sensor ID"; break;
            case 2: desc << "Sensor Type"; break;
            case 3: desc << "Sample Value"; break;
            case 4: desc << "Min Value"; break;
            case 5: desc << "Max Value"; break;
            case 6: desc << "Last Sampling Time"; break;
            case 7: desc << "Sampling Rate"; break;
            default: desc << "Unknown Sensor Object"; break;
        }
        if (iid.hasIndex1 && iid.hasIndex2) {
            desc << " [Device " << iid.index1 << ", Sensor " << iid.index2 << "]";
        } else if (iid.hasIndex1) {
            desc << " [Sensor " << iid.index1 << "]";
        }
    } else {
        desc << "Unknown Structure";
    }

    return desc.str();
}

// Function to describe the error codes returned in the response in a human-readable format
std::string describeError(int errorCode) {
    switch (errorCode) {
        case 0:  return "No Error (Operation successful)";
        case 1:  return "Message Decoding Error (Malformed or corrupted PDU received)";
        case 2:  return "Tag Error (PDU tag does not match expected value)";
        case 3:  return "Unknown Message Type (PDU type is not recognized)";
        case 4:  return "Duplicate Message (Message ID has already been processed)";
        case 5:  return "Invalid or Unknown IID (Object identifier does not exist)";
        case 6:  return "Unknown Value Type (Value type is not supported for this object)";
        case 7:  return "Unsupported Value (Value is outside allowed range or not permitted for this object)";
        case 8:  return "Value List IID Mismatch (Mismatch between IIDs and values in the request)";
        case 9:  return "Value Exists but is Read-Only (Object cannot be written to, or write not permitted)";
        default: return "Unknown Error Code (Unrecognized error code returned)";
    }
}

// Function to print the IID in a human-readable format
void printIID(const LSNMP_IID& iid) { 
    std::cout << (int)iid.structure << "." << (int)iid.object;
    if (iid.hasIndex1) std::cout << "." << (int)iid.index1;
    if (iid.hasIndex2) std::cout << "." << (int)iid.index2;
}

// Function to send a request PDU to the agent and print the response
void sendRequest(LSNMP_PDU& request, int sockfd, struct sockaddr_in& server_addr, socklen_t server_len) {
    lastRequest = request; // Store for debugging
    std::vector<uint8_t> encoded = encodePDU(request); // Encode the PDU to bytes
    std::vector<uint8_t> secured = encodeSecureMessage(encoded, MANAGER_ID, AGENT_ID, SEC_MODEL_SIMPLE); // Secure the message
    sendto(sockfd, secured.data(), secured.size(), 0, (struct sockaddr*)&server_addr, server_len); // Send the request

    char buffer[BUFFER_SIZE]; // Buffer to receive response
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &server_len); // Receive the response
    if (n > 0) { // If we received data
        std::vector<uint8_t> responseBuf(buffer, buffer + n);
        std::vector<uint8_t> pduBytes, senderID, receiverID;
        uint8_t secModel;
        bool isSecure = decodeSecureMessage(responseBuf, pduBytes, senderID, receiverID, secModel);
        
        // Check if the message is secure and not a beacon (broadcast)
        if (!isSecure) {
            std::cerr << "[Manager] Security check failed or invalid message.\n";
            return;
        }
        LSNMP_PDU response = decodePDU(pduBytes);

        lastResponse = response; // Store for debugging

        // Print the response details
        std::cout << "[Response for MSG-ID " << response.messageID << "]\n";
        for (size_t i = 0; i < response.iidList.size(); ++i) {
            const auto& iid = response.iidList[i];
            const auto& val = response.valueList[i];
            const auto& err = response.errorList[i];

            std::cout << "  IID ";
            printIID(iid);
            std::cout << " => " << describeIID(iid) << " : ";

            // Print the value based on its type
            if (val.type == LSNMP_ValueType::INTEGER)
                std::cout << val.intValue;
            else if (val.type == LSNMP_ValueType::STRING)
                std::cout << val.strValue;
            else if (val.type == LSNMP_ValueType::TIMESTAMP) {
                const auto& ts = val.tsValue;
                char buf[64];
                if (ts.year == 0) {
                    // Relative: days:hour:min:sec:ms
                    snprintf(buf, sizeof(buf), "%u:%02d:%02d:%02d:%03d",
                        ts.day, ts.hour, ts.minute, ts.second, ts.millisecond);
                } else {
                    // Absolute: day:month:year:hour:min:sec:ms
                    snprintf(buf, sizeof(buf), "%02d:%02d:%04d:%02d:%02d:%02d:%03d",
                        ts.day, ts.month, ts.year, ts.hour, ts.minute, ts.second, ts.millisecond);
                }
                std::cout << buf;
            }

            // Print the error code using the describeError function
            std::cout << "\n  Error: " << static_cast<int>(err) << " (" << describeError(err) << ")\n\n";
        }
    }
}

// Function to print the full PDU information for both the request and response for debugging (for the 'pdu' command)
void printPDUInfo(const LSNMP_PDU& response) {
    std::cout << "MSG-ID: " << response.messageID << "\n";
    std::cout << "Type: " << static_cast<int>(response.type) << "\n";
    std::cout << "Timestamp: " << response.timestamp << "\n";

    std::cout << "IIDs (" << response.iidList.size() << "):\n";
    for (const auto& iid : response.iidList) {
        std::cout << "  (";
        printIID(iid);
        std::cout << ") - " << describeIID(iid) << "\n";
    }

    std::cout << "Values (" << response.valueList.size() << "):\n";
    for (const auto& val : response.valueList) {
        if (val.type == LSNMP_ValueType::INTEGER)
            std::cout << "  INTEGER: " << val.intValue << "\n";
        else if (val.type == LSNMP_ValueType::STRING)
            std::cout << "  STRING: " << val.strValue << "\n";
        else
            std::cout << "  Unknown type\n";
    }

    std::cout << "Timestamps (" << response.timeList.size() << "):\n";
    for (size_t i = 0; i < response.timeList.size(); ++i) {
        const auto& ts = response.timeList[i];
        std::cout << "  (";
        printIID(response.iidList[i]);
        std::cout << ") - ";
        if (ts.year == 0 && ts.month == 0) {
            // Relative: days:hh:mm:ss:ms
            std::cout << ts.day << ":" << (int)ts.hour << ":" << (int)ts.minute << ":" << (int)ts.second << ":" << ts.millisecond;
        } else {
            // Absolute: dd:mm:yyyy:hh:mm:ss:ms
            std::cout << (int)ts.day << "/" << (int)ts.month << "/" << ts.year << " "
                    << (int)ts.hour << ":" << (int)ts.minute << ":" << (int)ts.second << "." << ts.millisecond;
        }
        std::cout << "\n";
    }

    std::cout << "Errors (" << response.errorList.size() << "):\n";
    for (const auto& err : response.errorList) {
        std::cout << "  Error code: " << static_cast<int>(err) << "\n";
    }
}

// Function to send a request and get the response (will be used for the 'summary' command)
LSNMP_PDU sendRequestAndGetResponse(int sockfd, struct sockaddr_in& server_addr, socklen_t server_len, LSNMP_PDU& request) {
    std::vector<uint8_t> encoded = encodePDU(request);
    std::vector<uint8_t> secured = encodeSecureMessage(encoded, MANAGER_ID, AGENT_ID, SEC_MODEL_SIMPLE);
    sendto(sockfd, secured.data(), secured.size(), 0, (struct sockaddr*)&server_addr, server_len);

    char buffer[BUFFER_SIZE];
    int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&server_addr, &server_len);
    if (n > 0) {
        std::vector<uint8_t> responseBuf(buffer, buffer + n);
        std::vector<uint8_t> pduBytes, senderID, receiverID;
        uint8_t secModel;
        bool isSecure = decodeSecureMessage(responseBuf, pduBytes, senderID, receiverID, secModel);
        if (!isSecure) {
            std::cerr << "[Manager] Security check failed or invalid message.\n";
            return LSNMP_PDU{};
        }
        return decodePDU(pduBytes);
    }
    // return an empty PDU if no response received
    return LSNMP_PDU{};
}

// Function to print a summary of the connected devices and their sensors (for the 'summary' command)
void printSummary(int sockfd, struct sockaddr_in& server_addr, socklen_t server_len, uint64_t& msgID) {
    std::cout << "\n===================== L-SNMPvS Device & Sensor Summary =====================\n\n";

    // Request device information for each device
    for (int d = 1; d <= 2; ++d) {
        std::vector<LSNMP_IID> devIIDs;
        for (uint8_t obj = 2; obj <= 9; ++obj)
            devIIDs.push_back({1, obj, (uint16_t)d, 0, true, false});

        LSNMP_PDU devReq{};
        devReq.tag = {'L','S','N','M','P','v','S','1'};
        devReq.type = GET_REQUEST;
        devReq.messageID = msgID++;
        devReq.iidList = devIIDs;

        // Send the request and get the response to retrieve device information
        LSNMP_PDU devResp = sendRequestAndGetResponse(sockfd, server_addr, server_len, devReq);

        std::string id = (devResp.valueList[0].type == LSNMP_ValueType::STRING) ? devResp.valueList[0].strValue : "N/A";
        std::string type = (devResp.valueList[1].type == LSNMP_ValueType::STRING) ? devResp.valueList[1].strValue : "N/A";
        int beaconRate = (devResp.valueList[2].type == LSNMP_ValueType::INTEGER) ? (int)devResp.valueList[2].intValue : -1;
        int sensorCount = (devResp.valueList[3].type == LSNMP_ValueType::INTEGER) ? (int)devResp.valueList[3].intValue : 0;
        std::string dateTime;
        if (devResp.valueList[4].type == LSNMP_ValueType::TIMESTAMP) {
            const auto& ts = devResp.valueList[4].tsValue;
            char buf[64];
            snprintf(buf, sizeof(buf), "%02d:%02d:%04d %02d:%02d:%02d.%03d",
                ts.day, ts.month, ts.year, ts.hour, ts.minute, ts.second, ts.millisecond);
            dateTime = buf;
        } else {
            dateTime = "N/A";
        }
        std::string uptime;
        if (devResp.valueList[5].type == LSNMP_ValueType::TIMESTAMP) {
            const auto& ts = devResp.valueList[5].tsValue;
            char buf[64];
            snprintf(buf, sizeof(buf), "%02d:%02d:%04d %02d:%02d:%02d.%03d",
                ts.day, ts.month, ts.year, ts.hour, ts.minute, ts.second, ts.millisecond);
            uptime = buf;
        } else {
            uptime = "N/A";
        }
        int status = (devResp.valueList[6].type == LSNMP_ValueType::INTEGER) ? (int)devResp.valueList[6].intValue : -1;

        std::cout << "Device #" << d << " (" << type << ")\n";
        std::cout << "  ID: " << id << "\n";
        std::cout << "  Uptime: " << uptime << "\n";
        std::cout << "  Date/Time: " << dateTime << "\n";
        std::cout << "  Status: " << status << "\n";
        std::cout << "  Sensors: " << sensorCount << "\n";
        std::cout << "    -------------------------------------------------------------------------\n";
        std::cout << "    | # | Type         | Value | Min | Max | LastSampleTime          | Rate |\n";
        std::cout << "    -------------------------------------------------------------------------\n";

        // Request sensor information for each sensor in each device
        for (int s = 1; s <= sensorCount; ++s) {
            std::vector<LSNMP_IID> iids;
            for (uint8_t obj = 2; obj <= 7; ++obj) {
                iids.push_back({2, obj, (uint16_t)d, (uint16_t)s, true, true});
            }

            LSNMP_PDU req{};
            req.tag = {'L','S','N','M','P','v','S','1'};
            req.type = GET_REQUEST;
            req.messageID = msgID++;
            req.iidList = iids;

            // Send the request and get the response for sensor information
            LSNMP_PDU resp = sendRequestAndGetResponse(sockfd, server_addr, server_len, req);

            std::string type = (resp.valueList[0].type == LSNMP_ValueType::STRING) ? resp.valueList[0].strValue : "N/A";
            int value = (resp.valueList[1].type == LSNMP_ValueType::INTEGER) ? (int)resp.valueList[1].intValue : -1;
            int min = (resp.valueList[2].type == LSNMP_ValueType::INTEGER) ? (int)resp.valueList[2].intValue : -1;
            int max = (resp.valueList[3].type == LSNMP_ValueType::INTEGER) ? (int)resp.valueList[3].intValue : -1;
            std::string lastSample;
            if (resp.valueList[4].type == LSNMP_ValueType::TIMESTAMP) {
                const auto& ts = resp.valueList[4].tsValue;
                char buf[64];
                snprintf(buf, sizeof(buf), "%02d:%02d:%04d %02d:%02d:%02d.%03d",
                    ts.day, ts.month, ts.year, ts.hour, ts.minute, ts.second, ts.millisecond);
                lastSample = buf;
            } else {
                lastSample = "N/A";
            }
            int rate = (resp.valueList[5].type == LSNMP_ValueType::INTEGER) ? (int)resp.valueList[5].intValue : -1;

            std::cout << "    | " << s
                      << " | " << std::setw(12) << std::left << type
                      << " | " << std::setw(5) << value
                      << " | " << std::setw(3) << min
                      << " | " << std::setw(3) << max
                      << " | " << std::setw(20) << lastSample
                      << " | " << std::setw(4) << rate
                      << " |\n";
        }
        std::cout << "    -------------------------------------------------------------------------\n\n";
    }
    std::cout << "\n===============================================================================\n";
}

// Function to get the current timestamp for the PDU 
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

// Function to handle beacon PDUs received from the agent
void handleBeacon(const LSNMP_PDU& beacon) {
    std::cout << "=== BEACON RECEIVED ===\n";
    for (size_t i = 0; i < beacon.iidList.size(); ++i) {
        const auto& iid = beacon.iidList[i];
        const auto& val = beacon.valueList[i];
        std::cout << "  " << describeIID(iid) << ": ";
        if (val.type == LSNMP_ValueType::INTEGER) std::cout << val.intValue;
        else if (val.type == LSNMP_ValueType::STRING) std::cout << val.strValue;
        std::cout << "\n";
    }
}

// Main function to run the L-SNMPvS manager console
int main() {
    // Set up the UDP socket for communication with the agent
    int sockfd;
    struct sockaddr_in server_addr{};
    socklen_t server_len = sizeof(server_addr);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed\n");
        return 1;
    }

    server_addr.sin_family = AF_INET; // IPv4 address family
    server_addr.sin_port = htons(PORT); // Set port number
    server_addr.sin_addr.s_addr = INADDR_ANY; // Bind to any available address

    std::cout << "Welcome to L-SNMPvS Manager Console (type 'exit' to quit)\n\n";
    std::cout << "Type 'help' for available commands.\n";
    std::cout << "Type 'summary' to get a brief overview of connected devices and sensors.\n";
    std::cout << "Type 'pdu' to see the last request and response PDU details.\n\n";

    uint64_t msgID = 1000; // Starting message ID for requests
    fd_set readfds;
    struct timeval tv = {0, 100000};

    // Main event loop for the manager console
    while (true) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // Wait for data on the socket or user input
        if (select(sockfd + 1, &readfds, NULL, NULL, &tv) > 0) {
            char buffer[BUFFER_SIZE];
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);
            if (n > 0) {
                std::vector<uint8_t> data(buffer, buffer + n); // Convert received data to vector
                LSNMP_PDU pdu = decodePDU(data); // Decode the received PDU
                if (pdu.type == NOTIFICATION && pdu.messageID == 0) {
                    handleBeacon(pdu);
                    continue; // Skip command prompt if beacon received
                }
            }
        }

        std::cout << "> "; // Prompt for user input
        std::string line; // Read user input
        std::getline(std::cin, line); // Get the entire line of input

        if (line == "exit") break;

        std::istringstream iss(line); // Use string stream to parse the input
        std::string cmd;
        iss >> cmd; // Read the command from input

        // Handle different commands
        if (cmd == "get") {
            std::vector<LSNMP_IID> iids; // List of IIDs to get
            std::string iidStr;
            while(iss >> iidStr) {
                LSNMP_IID iid;
                if (!parseIID(iidStr, iid)) {
                    std::cerr << "Invalid IID format. Use 1.3 or 1.3.1.0 etc.\n";
                    std::cerr << "Type 'help' to know how to query objects.\n";
                    continue;
                }
                iids.push_back(iid); // Add valid IID to the list
            }
            if(iids.empty()) {
                std::cerr << "No valid IIDs provided.\n";
                continue;
            }

            // Create a GET request PDU
            LSNMP_PDU request;
            request.tag = {'L', 'S', 'N', 'M', 'P', 'v', 'S', '1'};
            request.type = GET_REQUEST;
            request.messageID = msgID++;
            request.iidList = iids;
            LSNMP_Timestamp now = getCurrentTimestamp();
            request.timestamp = now;
            
            sendRequest(request, sockfd, server_addr, server_len);

        // Handle SET command
        } else if (cmd == "set") {
            std::vector<LSNMP_IID> iids;
            std::vector<LSNMP_Value> values;
            std::string iidStr;
            int64_t value;
            while(iss >> iidStr) {
                LSNMP_IID iid;
                if (!parseIID(iidStr, iid)) {
                    std::cerr << "Invalid IID format. Use 1.3 or 1.3.1.0 etc.\n";
                    std::cerr << "Type 'help' to know how to query objects.\n";
                    continue;
                }
                // Check if this is a dateAndTime object to make sure of the format
                if (iid.structure == 1 && iid.object == 6) {
                    std::string strValue;
                    if (!(iss >> std::ws) || iss.peek() != '"') {
                        std::cerr << "Expected quoted string for dateAndTime.\n";
                        std::cerr << "Format: \"dd:mm:yyyy:hh:mm:ss:ms\" (e.g., \"23:05:2025:18:30:12:120\")\n";
                        continue;
                    }
                    iss.get(); // consume opening quote
                    std::getline(iss, strValue, '"'); // read until closing quote
                    iids.push_back(iid);
                    LSNMP_Timestamp ts{};
                    sscanf(strValue.c_str(), "%2hu:%2hhu:%4hu:%2hhu:%2hhu:%2hhu:%3hu",
                        &ts.day, &ts.month, &ts.year, &ts.hour, &ts.minute, &ts.second, &ts.millisecond);
                    values.push_back({LSNMP_ValueType::TIMESTAMP, 0, "", ts});
                
                // Other objects (integer values)
                } else {
                    int64_t value;
                    if (!(iss >> value)) {
                        std::cerr << "Invalid value for IID " << iidStr << ". Expected integer value.\n";
                        break;
                    }
                    // Add this check for reset (object 9)
                    if (iid.structure == 1 && iid.object == 9) {
                        if (value != 0 && value != 1) {
                            std::cerr << "Reset value must be 0 or 1.\n";
                            continue;
                        }
                    }
                    iids.push_back(iid);
                    values.push_back({LSNMP_ValueType::INTEGER, value});
                }
            }

            if(iids.empty() || values.empty()) {
                std::cerr << "Error: No valid IIDs or values provided.\n";
                continue;
            }

            // Create a SET request PDU
            LSNMP_PDU request;
            request.tag = {'L', 'S', 'N', 'M', 'P', 'v', 'S', '1'};
            request.type = SET_REQUEST;
            request.messageID = msgID++;
            request.iidList = iids;
            LSNMP_Timestamp now = getCurrentTimestamp();
            request.timestamp = now;
            request.valueList = values;

            sendRequest(request, sockfd, server_addr, server_len);

        // Handle other helpful commands
        } else if (cmd == "summary") {
            printSummary(sockfd, server_addr, server_len, msgID);
        
        } else if (cmd == "help") {
            std::cout << "\n*Available commands:\n";
            std::cout << "  get <IID>           - Get the value of a specific object (e.g., get 1.3.1.0)\n";
            std::cout << "  set <IID> <value>   - Set an integer value for a specific object (e.g., set 1.4.1 10)\n";
            std::cout << "  summary             - Print summary of connected devices and sensors\n";
            std::cout << "  pdu                 - Show the last request and response PDU details\n";
            std::cout << "  help                - Show this help message\n";
            std::cout << "  exit                - Exit the manager console\n";
            std::cout << "\n"
            << "*How to query:\n"
            << "  Device objects use IID format: 1.<object>.<deviceIndex>\n"
            << "    e.g., 1.3.1 → Device 1, object 3 (\"Sensing Hub\" type)\n\n"
            << "  Sensor objects use IID format: 2.<object>.<deviceIndex>.<sensorIndex>\n"
            << "    e.g., 2.3.1.1 → Device 1, sensor 1, object 3 (sampleValue = e.g., 50)\n\n";
        
        } else if (cmd == "pdu"){
            std::cout << "\nLast Request PDU details:\n";
            printPDUInfo(lastRequest);
            std::cout << "\nLast Response PDU details:\n";
            printPDUInfo(lastResponse);

        } else {
            std::cout << "Unknown command. Use:\n"
                 "  get 1.3\n"
                 "  get 1.3.1.0\n"
                 "  set 1.2.1 10\n"
                 "  summary\n"
                 "  help\n"
                 "  pdu\n"
                 "  exit\n";
        }
    }

    close(sockfd);
    std::cout << "Manager exited.\n";
    return 0;
}
