/*
 * File: PDU-header2.hpp
 * Author: Amneh Alissa
 * Description:
 *   Defines shared data structures, enums, and encoding/decoding functions for L-SNMPvS.
 *   Used by both agent and manager for protocol communication.
 * 
 * Main Classes/Functions:
 *   - LSNMP_PDU, LSNMP_IID, LSNMP_Value, LSNMP_Timestamp: Core protocol structures.
 *   - encodePDU(), decodePDU(): Encode/decode protocol data units.
 *   - encodeSecureMessage(), decodeSecureMessage(): Secure message envelope functions.
 * 
 * Data Types/Variables:
 *   - LSNMP_Type, LSNMP_ErrorCode, LSNMP_ValueType: Protocol enums.
 *   - AGENT_ID, MANAGER_ID, BROADCAST_ID: Identifiers for secure messaging.
 *   - SEC_MODEL_SIMPLE, SEC_MODEL_NONE: Security model identifiers.
 */

#ifndef LSNMPVS_PDU_HPP
#define LSNMPVS_PDU_HPP

#pragma once  // Ensure this header is included only once

// ========== Standard Libraries (used in all files) ==========

#include <iostream> // For input/output streams
#include <sstream> // For string stream operations
#include <thread> // For multithreading (e.g., std::thread)
#include <mutex> // For thread synchronization (e.g., std::mutex)
#include <atomic> // For atomic operations (e.g., std::atomic<uint64_t>)

#include <unistd.h> // For POSIX API (e.g., close, read, write)
#include <arpa/inet.h> // For network functions (e.g., socket, sendto, recvfrom)

#include <string> // For string manipulation
#include <cstring> // For C-style string functions (e.g., memset)
#include <regex> // For regular expressions (e.g., to validate dateAndTime format)
#include <cstdint> // For fixed-width integer types (e.g., uint8_t, uint16_t, uint64_t)
#include <random> // For random number generation (e.g., std::mt19937)

#include <ctime> // For time functions (e.g., std::time_t, std::localtime)
#include <chrono> // For high-resolution timing (e.g., std::chrono::steady_clock)

#include <vector> // For dynamic arrays (e.g., std::vector<uint8_t>)
#include <map> // For key-value pairs (e.g., std::map<LSNMP_IID, LSNMP_Value>)
#include <unordered_map> // For fast lookups (e.g., std::unordered_map<LSNMP_IID, LSNMP_Value>)
#include <iomanip> // For input/output manipulators (e.g., std::setw, std::setfill)

#include <openssl/sha.h> // For SHA-1 hashing (e.g., SHA1_Init, SHA1_Update, SHA1_Final)

// ========== ENUMS AND STRUCTS ==========

// L-SNMPvS Message Types
enum LSNMP_Type : uint8_t {
    GET_REQUEST = 0,
    SET_REQUEST = 1,
    NOTIFICATION = 2,
    RESPONSE = 3
};

// L-SNMPvS Error Codes
enum LSNMP_ErrorCode : uint8_t {
    NO_ERROR = 0,
    DECODE_ERROR = 1,
    TAG_ERROR = 2,
    UNKNOWN_MSG_TYPE = 3,
    DUPLICATE_MSG = 4,
    INVALID_IID = 5,
    UNKNOWN_VALUE_TYPE = 6,
    UNSUPPORTED_VALUE = 7,
    VALUE_LIST_IID_MISMATCH = 8,
    NOT_WRITABLE = 9
};

// L-SNMPvS Timestamp Structure
struct LSNMP_Timestamp {
    uint16_t day;
    uint8_t month;       // 1-12 for absolute, 0 for relative
    uint16_t year;       // 2000+ for absolute, 0 for relative
    uint8_t hour, minute, second;
    uint16_t millisecond;
};

// Operator to print LSNMP_Timestamp
inline std::ostream& operator<<(std::ostream& os, const LSNMP_Timestamp& ts) {
    os << static_cast<int>(ts.day) << "/"
       << static_cast<int>(ts.month) << "/"
       << ts.year << " "
       << static_cast<int>(ts.hour) << ":"
       << static_cast<int>(ts.minute) << ":"
       << static_cast<int>(ts.second) << "."
       << ts.millisecond;
    return os;
}

// L-SNMPvS IID Structure
struct LSNMP_IID {
    uint8_t structure;
    uint8_t object;
    uint16_t index1 = 0;
    uint16_t index2 = 0;
    bool hasIndex1 = false; // Indicates if index1 is present
    bool hasIndex2 = false; // Indicates if index2 is present

    // Operator to compare LSNMP_IID for (used in std::map)
    bool operator<(const LSNMP_IID& other) const {
        return std::tie(structure, object, hasIndex1, index1, hasIndex2, index2)
             < std::tie(other.structure, other.object, other.hasIndex1, other.index1, other.hasIndex2, other.index2);
    }
};

// Operator to print LSNMP_IID
inline std::ostream& operator<<(std::ostream& os, const LSNMP_IID& iid) {
    os << "S:" << static_cast<int>(iid.structure)
       << " O:" << static_cast<int>(iid.object);
    if (iid.hasIndex1) os << " I1:" << iid.index1;
    if (iid.hasIndex2) os << " I2:" << iid.index2;
    return os;
}

// L-SNMPvS Value Types
enum class LSNMP_ValueType {
    INTEGER,
    STRING,
    TIMESTAMP,
    IID
};

// L-SNMPvS Value Structure
struct LSNMP_Value {
    LSNMP_ValueType type;
    int64_t intValue = 0;
    std::string strValue;
    LSNMP_Timestamp tsValue;
    LSNMP_IID iidValue;
};

// Operator to print LSNMP_Value
inline std::ostream& operator<<(std::ostream& os, const LSNMP_Value& v) {
    switch (v.type) {
        case LSNMP_ValueType::INTEGER: os << v.intValue; break;
        case LSNMP_ValueType::STRING: os << v.strValue; break;
        case LSNMP_ValueType::TIMESTAMP: os << v.tsValue; break;
        case LSNMP_ValueType::IID: os << v.iidValue; break;
    }
    return os;
}

// L-SNMPvS Data Types (used for encoding/decoding)
enum LSNMP_DataType : uint8_t {
    BYTE_TYPE = 0x00,          // 00000000
    BYTE_SHORT_SEQ = 0x01,     // 00000001
    BYTE_LONG_SEQ = 0x02,      // 00000010
    INTEGER_TYPE = 0x04,       // 00000100 (last 2 bits for size)
    INTEGER_SHORT_SEQ = 0x08,  // 00001000 (last 2 bits for size)
    INTEGER_LONG_SEQ = 0x0C,   // 00001100 (last 2 bits for size)
    TIMESTAMP_TYPE = 0x10,     // 00010000 (last bit for type)
    STRING_TYPE = 0x20,        // 00100000 (last 4 bits for encoding)
    IID_TYPE = 0x40            // 01000000 (last 2 bits for index presence)
};

// L-SNMPvS PDU Structure
struct LSNMP_PDU {
    std::vector<uint8_t> tag;
    LSNMP_Type type;
    LSNMP_Timestamp timestamp;
    uint64_t messageID;
    std::vector<LSNMP_IID> iidList;
    std::vector<LSNMP_Value> valueList;
    std::vector<LSNMP_Timestamp> timeList;
    std::vector<LSNMP_ErrorCode> errorList;
};

// ========== L-SNMPvS SECURE MESSAGE ENVELOPE ==========

const std::vector<uint8_t> AGENT_ID = {'A','G','E','N','T',0,0,1};
const std::vector<uint8_t> MANAGER_ID = {'M','A','N','A','G','E','R',1};
const std::vector<uint8_t> BROADCAST_ID(8, 0);
const uint8_t SEC_MODEL_NONE = 0; // no security model, plain PDU
const uint8_t SEC_MODEL_SIMPLE = 128; // model with simple XOR encryption and SHA-256 hash

const std::vector<uint8_t> SHARED_KEY = {'s','e','c','r','e','t','k','y'}; // 8 bytes

// XOR encryption/decryption function
inline void xorEncryptDecrypt(std::vector<uint8_t>& data, const std::vector<uint8_t>& key) {
    for (size_t i = 0; i < data.size(); ++i)
        data[i] ^= key[i % key.size()];
}

// SHA-256 hashing function
inline std::vector<uint8_t> sha256(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    SHA256(data.data(), data.size(), hash.data());
    return hash;
}

// Secure message encoding
inline std::vector<uint8_t> encodeSecureMessage(
    const std::vector<uint8_t>& pdu,
    const std::vector<uint8_t>& senderID,
    const std::vector<uint8_t>& receiverID,
    uint8_t secModel = SEC_MODEL_SIMPLE) // default to simple security model
{
    std::vector<uint8_t> buf;
    // Sender-ID (8 bytes)
    buf.insert(buf.end(), senderID.begin(), senderID.end());
    // Receiver-ID (8 bytes)
    buf.insert(buf.end(), receiverID.begin(), receiverID.end());
    // Sec-Model-ID (1 byte)
    buf.push_back(secModel);

    std::vector<uint8_t> secData;
    if (secModel == SEC_MODEL_NONE) {
        secData = pdu;
    } else {
        secData = pdu;
        xorEncryptDecrypt(secData, SHARED_KEY);
        auto hash = sha256(secData);
        secData.insert(secData.end(), hash.begin(), hash.end());
    }

    // Sec-Data-Size (4 bytes)
    uint32_t sz = secData.size();
    buf.push_back((sz >> 24) & 0xFF);
    buf.push_back((sz >> 16) & 0xFF);
    buf.push_back((sz >> 8) & 0xFF);
    buf.push_back(sz & 0xFF);

    // Sec-Data
    buf.insert(buf.end(), secData.begin(), secData.end());
    return buf;
}

// Secure message decoding
inline bool decodeSecureMessage(
    const std::vector<uint8_t>& buf,
    std::vector<uint8_t>& pdu,
    std::vector<uint8_t>& senderID,
    std::vector<uint8_t>& receiverID,
    uint8_t& secModel)
{
    if (buf.size() < 21) return false;
    senderID.assign(buf.begin(), buf.begin() + 8);
    receiverID.assign(buf.begin() + 8, buf.begin() + 16);
    secModel = buf[16];
    uint32_t sz = (buf[17] << 24) | (buf[18] << 16) | (buf[19] << 8) | buf[20];
    if (buf.size() < 21 + sz) return false;
    std::vector<uint8_t> secData(buf.begin() + 21, buf.begin() + 21 + sz);

    if (secModel == SEC_MODEL_NONE) {
        pdu = secData;
        return true;
    } else {
        if (secData.size() < SHA256_DIGEST_LENGTH) return false;
        std::vector<uint8_t> data(secData.begin(), secData.end() - SHA256_DIGEST_LENGTH);
        std::vector<uint8_t> hash(secData.end() - SHA256_DIGEST_LENGTH, secData.end());
        auto calcHash = sha256(data);
        if (hash != calcHash) return false;
        xorEncryptDecrypt(data, SHARED_KEY);
        pdu = data;
        return true;
    }
}

// ========== ENCODING/DECODING HELPERS ==========

// Write a 16-bit unsigned integer to the buffer
inline void writeUint16(std::vector<uint8_t>& buf, uint16_t val) {
    buf.push_back((val >> 8) & 0xFF);
    buf.push_back(val & 0xFF);
}

// Read a 16-bit unsigned integer from the buffer and update the offset
inline uint16_t readUint16(const std::vector<uint8_t>& buf, size_t& offset) {
    if (offset + 2 > buf.size()) throw std::runtime_error("Buffer underrun (uint16)");
    return (buf[offset++] << 8) | buf[offset++];
}

// Write a 64-bit unsigned integer to the buffer
inline void writeUint64(std::vector<uint8_t>& buf, uint64_t val) {
    for (int i = 7; i >= 0; --i)
        buf.push_back((val >> (i * 8)) & 0xFF);
}

// Read a 64-bit unsigned integer from the buffer and update the offset
inline uint64_t readUint64(const std::vector<uint8_t>& buf, size_t& offset) {
    if (offset + 8 > buf.size()) throw std::runtime_error("Buffer underrun (uint64)");
    uint64_t val = 0;
    for (int i = 0; i < 8; ++i)
        val = (val << 8) | buf[offset++];
    return val;
}

// Write a variable-length integer (N bytes) to the buffer
inline void writeIntN(std::vector<uint8_t>& buf, int64_t val, int nbytes) {
    for (int i = nbytes - 1; i >= 0; --i)
        buf.push_back((val >> (i * 8)) & 0xFF);
}

// Read a variable-length integer (N bytes) from the buffer and update the offset
inline int64_t readIntN(const std::vector<uint8_t>& buf, size_t& offset, int nbytes) {
    if (offset + nbytes > buf.size()) throw std::runtime_error("Buffer underrun (intN)");
    int64_t val = 0;
    for (int i = 0; i < nbytes; ++i)
        val = (val << 8) | buf[offset++];
    int shift = (8 - nbytes) * 8;
    return (val << shift) >> shift;
}

// ========== TIMESTAMP ENCODING ==========

// Write a timestamp to the buffer
inline void writeTimestamp(std::vector<uint8_t>& buf, const LSNMP_Timestamp& ts, bool isRelative = false) {
    uint16_t value1 = ts.second * 1000 + ts.millisecond;
    uint16_t value2 = ts.hour * 60 + ts.minute;
    if (isRelative) {
        writeUint16(buf, value1);    // seconds+ms
        writeUint16(buf, value2);    // hours+min
        writeUint16(buf, ts.day);    // days
    } else {
        uint16_t date = (ts.year - 2000) * 512 + ts.month * 32 + ts.day;
        writeUint16(buf, value1);    // seconds+ms
        writeUint16(buf, value2);    // hours+min
        writeUint16(buf, date);      // encoded date
    }
}

// Read a timestamp from the buffer and update the offset
inline LSNMP_Timestamp readTimestamp(const std::vector<uint8_t>& buf, size_t& offset, bool isRelative = false) {
    LSNMP_Timestamp ts{};
    uint16_t value1 = readUint16(buf, offset);
    uint16_t value2 = readUint16(buf, offset);
    uint16_t value3 = readUint16(buf, offset);

    ts.second = value1 / 1000;
    ts.millisecond = value1 % 1000;
    ts.hour = value2 / 60;
    ts.minute = value2 % 60;

    if (isRelative) {
        ts.day = value3;
        ts.month = 0;
        ts.year = 0;
    } else {
        ts.year = 2000 + (value3 >> 9);
        ts.month = (value3 >> 5) & 0x0F;
        ts.day = value3 & 0x1F;
    }
    return ts;
}

// ========== IID ENCODING ==========

// Write an IID to the buffer
inline void writeIID(std::vector<uint8_t>& buf, const LSNMP_IID& iid) {
    buf.push_back(iid.structure);
    buf.push_back(iid.object);
    if (iid.hasIndex1 && iid.hasIndex2) {
        writeUint16(buf, iid.index1);
        writeUint16(buf, iid.index2);
    } else if (iid.hasIndex1) {
        writeUint16(buf, iid.index1);
    }
}

// Read an IID from the buffer and update the offset
inline LSNMP_IID readIID(const std::vector<uint8_t>& buf, size_t& offset, uint8_t iidType) {
    LSNMP_IID iid;
    iid.structure = buf[offset++];
    iid.object = buf[offset++];
    iid.hasIndex1 = (iidType & 0x01) != 0;
    iid.hasIndex2 = (iidType & 0x02) != 0;
    if (iid.hasIndex1) iid.index1 = readUint16(buf, offset);
    if (iid.hasIndex2) iid.index2 = readUint16(buf, offset);
    return iid;
}

// ========== VALUE ENCODING/DECODING ==========

// Write a value to the buffer based on its type
inline void writeValue(std::vector<uint8_t>& buf, const LSNMP_Value& val) {
    
    switch (val.type) {
        // Handle each value type
        case LSNMP_ValueType::INTEGER: {
            uint8_t type = INTEGER_TYPE;
            if (val.intValue >= INT8_MIN && val.intValue <= INT8_MAX) {
                type |= 0x00;
                buf.push_back(static_cast<uint8_t>(type));
                buf.push_back(static_cast<int8_t>(val.intValue));
            } else if (val.intValue >= INT16_MIN && val.intValue <= INT16_MAX) {
                type |= 0x01;
                buf.push_back(static_cast<uint8_t>(type));
                writeUint16(buf, static_cast<int16_t>(val.intValue));
            } else if (val.intValue >= INT32_MIN && val.intValue <= INT32_MAX) {
                type |= 0x02;
                buf.push_back(static_cast<uint8_t>(type));
                int32_t v = static_cast<int32_t>(val.intValue);
                buf.push_back((v >> 24) & 0xFF);
                buf.push_back((v >> 16) & 0xFF);
                buf.push_back((v >> 8) & 0xFF);
                buf.push_back(v & 0xFF);
            } else {
                type |= 0x03;
                buf.push_back(static_cast<uint8_t>(type));
                writeUint64(buf, static_cast<uint64_t>(val.intValue));
            }
            break;
        }

        case LSNMP_ValueType::STRING: {
            buf.push_back(static_cast<uint8_t>(STRING_TYPE | 0x00));
            writeUint16(buf, val.strValue.size());
            buf.insert(buf.end(), val.strValue.begin(), val.strValue.end());
            break;
        }

        case LSNMP_ValueType::TIMESTAMP: {
            bool isRelative = (val.tsValue.year == 0);
            buf.push_back(static_cast<uint8_t>(TIMESTAMP_TYPE | (isRelative ? 0x01 : 0x00)));
            writeTimestamp(buf, val.tsValue, isRelative);
            break;
        }

        case LSNMP_ValueType::IID: {
            uint8_t iidType = 0x00;
            if (val.iidValue.hasIndex1 && val.iidValue.hasIndex2) iidType = 0x03;
            else if (val.iidValue.hasIndex1) iidType = 0x01;
            buf.push_back(static_cast<uint8_t>(IID_TYPE | iidType));
            writeIID(buf, val.iidValue);
            break;
        }
    }
}

// Read a value from the buffer and update the offset
inline LSNMP_Value readValue(const std::vector<uint8_t>& buf, size_t& offset) {
    if (offset >= buf.size()) throw std::runtime_error("Buffer underrun in readValue");
    
    LSNMP_Value val;
    uint8_t dataType = buf[offset++];
    uint8_t baseType = dataType & 0xF0;

    switch (baseType) {
        // Handle each base type
        case BYTE_TYPE:
            val.type = LSNMP_ValueType::INTEGER;
            val.intValue = buf[offset++];
            break;
        case BYTE_SHORT_SEQ: {
            val.type = LSNMP_ValueType::STRING;
            uint8_t len = buf[offset++];
            val.strValue = std::string(buf.begin() + offset, buf.begin() + offset + len);
            offset += len;
            break;
        }
        case BYTE_LONG_SEQ: {
            val.type = LSNMP_ValueType::STRING;
            uint16_t len = readUint16(buf, offset);
            val.strValue = std::string(buf.begin() + offset, buf.begin() + offset + len);
            offset += len;
            break;
        }
        // all integer types share the same base type
        case INTEGER_TYPE:
        case INTEGER_SHORT_SEQ:
        case INTEGER_LONG_SEQ: {
            val.type = LSNMP_ValueType::INTEGER;
            uint8_t sizeBits = dataType & 0x03;
            if (sizeBits == 0x00) val.intValue = static_cast<int8_t>(buf[offset++]);
            else if (sizeBits == 0x01) val.intValue = static_cast<int16_t>(readUint16(buf, offset));
            else if (sizeBits == 0x02) {
                int32_t v = (static_cast<int32_t>(buf[offset]) << 24) |
                            (static_cast<int32_t>(buf[offset + 1]) << 16) |
                            (static_cast<int32_t>(buf[offset + 2]) << 8) |
                            (static_cast<int32_t>(buf[offset + 3]));
                offset += 4;
                val.intValue = v;
            } else val.intValue = static_cast<int64_t>(readUint64(buf, offset));
            break;
        }
        case TIMESTAMP_TYPE: {
            val.type = LSNMP_ValueType::TIMESTAMP;
            bool isRelative = (dataType & 0x01) != 0;
            val.tsValue = readTimestamp(buf, offset, isRelative);
            break;
        }
        case STRING_TYPE: {
            val.type = LSNMP_ValueType::STRING;
            uint8_t encoding = dataType & 0x0F;
            uint16_t len = readUint16(buf, offset);
            val.strValue = std::string(buf.begin() + offset, buf.begin() + offset + len);
            offset += len;
            break;
        }
        case IID_TYPE: {
            val.type = LSNMP_ValueType::IID;
            uint8_t iidType = dataType & 0x03;
            val.iidValue = readIID(buf, offset, iidType);
            break;
        }
        default:
            throw std::runtime_error(
                            std::string("Invalid data type byte: 0x") + 
                            std::to_string(static_cast<int>(dataType))
                        );
    }
    return val;
}

// ========== LIST ENCODING/DECODING ==========

// Write a list of items to the buffer
template<typename T>
void writeList(std::vector<uint8_t>& buf, const std::vector<T>& list, 
               std::function<void(std::vector<uint8_t>&, const T&)> writer) {
    writeUint16(buf, list.size());
    for (const auto& item : list) writer(buf, item);
}

// Read a list of items from the buffer and update the offset
template<typename T>
std::vector<T> readList(const std::vector<uint8_t>& buf, size_t& offset, 
                        std::function<T(const std::vector<uint8_t>&, size_t&)> reader) {
    std::vector<T> list;
    uint16_t count = readUint16(buf, offset);
    list.reserve(count);
    for (uint16_t i = 0; i < count; i++) list.push_back(reader(buf, offset));
    return list;
}

// ========== FULL PDU ENCODE/DECODE ==========

// Encode a complete LSNMP_PDU into a byte vector
inline std::vector<uint8_t> encodePDU(const LSNMP_PDU& pdu) {
    std::vector<uint8_t> buf;

    if (pdu.tag.size() != 8)
        throw std::runtime_error("Tag must be exactly 8 bytes");

    buf.insert(buf.end(), pdu.tag.begin(), pdu.tag.end());
    buf.push_back(static_cast<uint8_t>(pdu.type));
    writeTimestamp(buf, pdu.timestamp);
    writeUint64(buf, pdu.messageID);

    // Write lists based on their types
    writeList<LSNMP_IID>(buf, pdu.iidList, [](std::vector<uint8_t>& b, const LSNMP_IID& iid) {
        uint8_t iidType = 0x00;
        if (iid.hasIndex1 && iid.hasIndex2) iidType = 0x03;
        else if (iid.hasIndex1) iidType = 0x01;
        b.push_back(IID_TYPE | iidType);
        writeIID(b, iid);
    });

    writeList<LSNMP_Value>(buf, pdu.valueList, [](std::vector<uint8_t>& b, const LSNMP_Value& val) {
        writeValue(b, val);
    });

    writeList<LSNMP_Timestamp>(buf, pdu.timeList, [](std::vector<uint8_t>& b, const LSNMP_Timestamp& rt) {
        writeTimestamp(b, rt, true);
    });

    writeList<LSNMP_ErrorCode>(buf, pdu.errorList, [](std::vector<uint8_t>& b, const LSNMP_ErrorCode& e) {
        b.push_back(static_cast<uint8_t>(e));
    });

    return buf;
}

// Decode a byte vector into a complete LSNMP_PDU
inline LSNMP_PDU decodePDU(const std::vector<uint8_t>& buf) {
    size_t offset = 0;
    LSNMP_PDU pdu;

    for (int i = 0; i < 8; i++)
        pdu.tag.push_back(buf[offset++]);

    pdu.type = static_cast<LSNMP_Type>(buf[offset++]);
    pdu.timestamp = readTimestamp(buf, offset);
    pdu.messageID = readUint64(buf, offset);

    // Read lists based on their types
    pdu.iidList = readList<LSNMP_IID>(buf, offset, [](const std::vector<uint8_t>& b, size_t& o) {
        uint8_t typeByte = b[o++];
        uint8_t iidType = typeByte & 0x03;
        return readIID(b, o, iidType);
    });

    pdu.valueList = readList<LSNMP_Value>(buf, offset, [](const std::vector<uint8_t>& b, size_t& o) {
        return readValue(b, o);
    });

    pdu.timeList = readList<LSNMP_Timestamp>(buf, offset, [](const std::vector<uint8_t>& b, size_t& o) {
        return readTimestamp(b, o, true);

    });

    pdu.errorList = readList<LSNMP_ErrorCode>(buf, offset, [](const std::vector<uint8_t>& b, size_t& o) {
        return static_cast<LSNMP_ErrorCode>(b[o++]);
    });

    return pdu;
}

#endif
