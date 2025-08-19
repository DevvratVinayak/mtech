#ifndef CONSTANTS_H
#define CONSTANTS_H
#include <string>
#include <regex>

std::regex THREAD_ENDED("Thread ended:\\s*(\\d+)");
std::regex THREAD_BEGIN("Thread begin:\\s*(\\d+)");
std::regex AFTER_LOCK_RELEASE("^After lock release: ");
std::regex PARENT("Parent:\\s*(\\d+)");
std::regex AFTER_LOCK_ACQUIRE("^After lock acquire: ");
std::regex TID_BEGIN("^TID: ");
std::regex TID(R"(TID:\s*(\d+))");
std::regex ADDR(R"(ADDR:\s*(0x[0-9a-fA-F]+))");
std::regex ISREAD(R"(isRead:\s*(\d))");
std::regex SIZE(R"(Size \(B\):\s*(\d+))");
std::regex LOCKADDRESS(R"(Lock address: (\S+))");
enum DataRaceType {
    RW=0, WR, WW
};
const char* dr_strings[] = {
    "R-W", "W-R", "W-W"
};
#endif
