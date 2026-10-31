#pragma once
#include <cstdint>
#include <stdexcept>

// Diagnostic ordering experiment. Poll counts are chosen, not captured EE cycles.
// This component neither configures video timing nor delivers interrupt callbacks.
class IntcCheckpoint {
public:
    void arm(std::uint32_t status, std::uint32_t mask) {
        if (armed_ || status != 12 || mask != 0x1802)
            throw std::runtime_error("Unsupported/repeated INTC checkpoint");
        status_=status;mask_=mask;armed_=true;
    }
    void acknowledge(std::uint32_t bits) {
        if (!armed_ || bits!=4 || acknowledgements>=2
            || (acknowledgements==1 && events!=1))
            throw std::runtime_error("Unsupported INTC acknowledgement order");
        status_ &= ~bits; ++acknowledgements;
    }
    std::uint32_t poll(bool event_enabled=true) {
        if (!armed_ || acknowledgements!=1 || polls>=64)
            throw std::runtime_error("Unsupported/exhausted INTC wait");
        // First poll must observe the cleared status. Event delivery is separate
        // from acknowledgement, after that unsatisfied diagnostic poll.
        if (polls && !events && event_enabled) {status_|=4; ++events;}
        ++polls;return status_;
    }
    std::uint32_t status() const {return status_;}
    bool callbackPending() const {return (status_ & mask_) != 0;}
    unsigned acknowledgements=0,polls=0,events=0;
private:
    bool armed_=false;
    std::uint32_t status_=0,mask_=0;
};
