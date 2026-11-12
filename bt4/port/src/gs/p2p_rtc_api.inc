struct DirectRtcApi {
    VoiceModule module=nullptr;bool ready=false,attempted=false;std::mutex lock;
#define LF(n) decltype(&::n) n=nullptr;
    LF(rtcCreatePeerConnection) LF(rtcClosePeerConnection) LF(rtcDeletePeerConnection)
    LF(rtcSetUserPointer) LF(rtcSetLocalDescriptionCallback) LF(rtcSetLocalCandidateCallback)
    LF(rtcSetRemoteDescription) LF(rtcSetLocalDescription) LF(rtcAddRemoteCandidate)
    LF(rtcSetStateChangeCallback) LF(rtcSetDataChannelCallback) LF(rtcCreateDataChannelEx)
    LF(rtcDeleteDataChannel) LF(rtcSetOpenCallback) LF(rtcSetClosedCallback)
    LF(rtcSetMessageCallback) LF(rtcSendMessage) LF(rtcIsOpen) LF(rtcGetBufferedAmount)
    LF(rtcGetDataChannelProtocol) LF(rtcGetDataChannelReliability) LF(rtcGetSelectedCandidatePair)
#undef LF
    bool load(){std::lock_guard<std::mutex> g(lock);if(attempted)return ready;attempted=true;
        module=voiceLibrary("libdatachannel.dll","libdatachannel.so","libdatachannel.dylib");if(!module)return false;
#define LF(n) n=reinterpret_cast<decltype(n)>(voiceFunction(module,#n));if(!n)return false;
        LF(rtcCreatePeerConnection) LF(rtcClosePeerConnection) LF(rtcDeletePeerConnection)
        LF(rtcSetUserPointer) LF(rtcSetLocalDescriptionCallback) LF(rtcSetLocalCandidateCallback)
        LF(rtcSetRemoteDescription) LF(rtcSetLocalDescription) LF(rtcAddRemoteCandidate)
        LF(rtcSetStateChangeCallback) LF(rtcSetDataChannelCallback) LF(rtcCreateDataChannelEx)
        LF(rtcDeleteDataChannel) LF(rtcSetOpenCallback) LF(rtcSetClosedCallback)
        LF(rtcSetMessageCallback) LF(rtcSendMessage) LF(rtcIsOpen) LF(rtcGetBufferedAmount)
        LF(rtcGetDataChannelProtocol) LF(rtcGetDataChannelReliability) LF(rtcGetSelectedCandidatePair)
#undef LF
        ready=true;return true;}
} rtc;