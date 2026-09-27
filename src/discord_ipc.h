#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "json.hpp"

namespace chuni {
class DiscordIpc {
public:
    using Log = std::function<void(const std::string&)>;
    explicit DiscordIpc(std::string app_id, Log log = {}, std::wstring test_pipe = {});
    ~DiscordIpc();
    void desired(const nlohmann::json& command);
    void poll(uint64_t now);
    bool ready() const { return ready_; }
    uint64_t acknowledgements() const { return acknowledgements_; }
    uint64_t generation() const { return generation_; }
    bool synchronized() const { return ready_ && acknowledged_ == generation_; }
private:
    void disconnect(uint64_t now, const std::string& reason);
    bool enqueue(uint32_t opcode, const std::string& payload);
    bool frame(uint32_t opcode, const std::string& payload, uint64_t now);
    void message(const std::string& text);
    std::string app_id_;
    Log log_;
    std::wstring test_pipe_;
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    std::vector<char> receive_, transmit_;
    bool ready_ = false;
    uint64_t next_connect_ = 0, opened_ = 0, next_send_ = 0, pending_deadline_ = 0;
    uint64_t generation_ = 0, sent_generation_ = 0, acknowledged_ = UINT64_MAX, acknowledgements_ = 0, nonce_ = 0;
    std::string pending_;
    nlohmann::json args_;
};
}
