#pragma once
#include <string>

class IEmulatorCore {
public:
    virtual bool loadROM(const std::string& path) = 0;
    virtual void reset() = 0;
    virtual void runFrame() = 0;
    virtual void setInput(int player, int buttons) = 0;
    virtual const void* getVideoBuffer() = 0;
    virtual ~IEmulatorCore() = default;
};
