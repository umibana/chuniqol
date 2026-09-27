#include "discord_ipc.h"
#include "core.h"
#include <algorithm>
#include <cstring>

namespace chuni {
static constexpr uint32_t MAX_FRAME = 65536;
DiscordIpc::DiscordIpc(std::string app_id, Log log, std::wstring test_pipe)
    : app_id_(std::move(app_id)), log_(std::move(log)), test_pipe_(std::move(test_pipe)),
      args_({{"pid", GetCurrentProcessId()}, {"activity", nullptr}}) {}
DiscordIpc::~DiscordIpc() { if (pipe_ != INVALID_HANDLE_VALUE) CloseHandle(pipe_); }
void DiscordIpc::message(const std::string& text) { if (log_) log_(text); }
void DiscordIpc::desired(const nlohmann::json& command) {
    if (command.at("args") != args_) { args_ = command.at("args"); ++generation_; }
}
void DiscordIpc::disconnect(uint64_t now, const std::string& reason) {
    if (pipe_ != INVALID_HANDLE_VALUE) CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
    ready_ = false;
    receive_.clear(); transmit_.clear(); pending_.clear();
    acknowledged_ = UINT64_MAX;
    next_connect_ = now + 3000;
    message(reason);
}
bool DiscordIpc::enqueue(uint32_t opcode, const std::string& payload) {
    if (payload.size() > MAX_FRAME || transmit_.size() + payload.size() + 8 > MAX_FRAME * 2) return false;
    uint32_t header[2] = {opcode, (uint32_t)payload.size()};
    auto bytes = reinterpret_cast<const char*>(header);
    transmit_.insert(transmit_.end(), bytes, bytes + 8);
    transmit_.insert(transmit_.end(), payload.begin(), payload.end());
    return true;
}
bool DiscordIpc::frame(uint32_t opcode, const std::string& payload, uint64_t now) {
    if (opcode == 3) return enqueue(4, payload); // PING -> identical PONG
    if (opcode == 4) return true;
    if (opcode == 2) { disconnect(now, "Discord closed IPC; reconnecting."); return false; }
    if (opcode != 1) { disconnect(now, "Unknown Discord IPC opcode."); return false; }
    auto event = nlohmann::json::parse(payload, nullptr, false);
    if (event.is_discarded() || !event.is_object()) { disconnect(now, "Invalid Discord JSON."); return false; }
    const auto evt = event.contains("evt") && event["evt"].is_string() ? event["evt"].get<std::string>() : "";
    if (evt == "READY") {
        ready_ = true;
        acknowledged_ = UINT64_MAX;
        message("Discord READY.");
    } else if (evt == "ERROR") {
        int code = 0;
        if (event.contains("data") && event["data"].is_object() && event["data"].contains("code") && event["data"]["code"].is_number_integer())
            code = event["data"]["code"].get<int>();
        message("Discord rejected a request (code " + std::to_string(code) + ").");
        pending_.clear();
        next_send_ = now + 15000;
    } else if (event.contains("nonce") && event["nonce"].is_string() && event["nonce"] == pending_ && !pending_.empty()) {
        acknowledged_ = sent_generation_;
        ++acknowledgements_;
        pending_.clear();
        message("Discord accepted activity update.");
    }
    return true;
}
void DiscordIpc::poll(uint64_t now) {
    if (pipe_ == INVALID_HANDLE_VALUE) {
        if (now < next_connect_ || !valid_app_id(app_id_)) return;
        next_connect_ = now + 3000;
        for (int index = 0; index < (test_pipe_.empty() ? 10 : 1); ++index) {
            auto name = test_pipe_.empty() ? L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(index) : test_pipe_;
            pipe_ = CreateFileW(name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (pipe_ != INVALID_HANDLE_VALUE) break;
        }
        if (pipe_ == INVALID_HANDLE_VALUE) return;
        // Windows nonblocking byte-mode pipes: neither reads nor writes wait
        // for Discord. Partial writes remain queued for a later worker poll.
        DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
        if (!SetNamedPipeHandleState(pipe_, &mode, nullptr, nullptr)) {
            disconnect(now, "Could not set nonblocking Discord IPC mode."); return;
        }
        opened_ = now;
        ready_ = false;
        pending_.clear();
        acknowledged_ = UINT64_MAX;
        next_send_ = now;
        if (!enqueue(0, nlohmann::json({{"v", 1}, {"client_id", app_id_}}).dump())) {
            disconnect(now, "Handshake exceeds IPC limit."); return;
        }
    }
    for (int count = 0; count < 16; ++count) {
        char bytes[4096]; DWORD got = 0;
        if (!ReadFile(pipe_, bytes, sizeof(bytes), &got, nullptr)) {
            auto error = GetLastError();
            if (error == ERROR_NO_DATA) break;
            disconnect(now, "Discord IPC disconnected; reconnecting."); return;
        }
        if (!got) break;
        receive_.insert(receive_.end(), bytes, bytes + got);
        if (receive_.size() > MAX_FRAME * 2) { disconnect(now, "Discord IPC input exceeds limit."); return; }
        while (receive_.size() >= 8) {
            uint32_t header[2]; memcpy(header, receive_.data(), 8);
            if (header[1] > MAX_FRAME) { disconnect(now, "Discord IPC frame exceeds limit."); return; }
            if (receive_.size() < 8 + header[1]) break;
            std::string body(receive_.data() + 8, header[1]);
            receive_.erase(receive_.begin(), receive_.begin() + 8 + header[1]);
            if (!frame(header[0], body, now)) return;
        }
    }
    if ((!ready_ && now >= opened_ + 5000) || (!pending_.empty() && now >= pending_deadline_)) {
        disconnect(now, "Discord response timed out; reconnecting."); return;
    }
    if (ready_ && pending_.empty() && generation_ != acknowledged_ && now >= next_send_) {
        pending_ = std::to_string(++nonce_);
        sent_generation_ = generation_;
        pending_deadline_ = now + 5000;
        next_send_ = now + 1000;
        auto command = nlohmann::json{{"cmd", "SET_ACTIVITY"}, {"args", args_}, {"nonce", pending_}};
        if (!enqueue(1, command.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace))) {
            disconnect(now, "Activity exceeds IPC limit."); return;
        }
    }
    if (!transmit_.empty()) {
        DWORD wrote = 0;
        if (!WriteFile(pipe_, transmit_.data(), (DWORD)transmit_.size(), &wrote, nullptr)) {
            disconnect(now, "Discord IPC write failed; reconnecting."); return;
        }
        if (wrote) transmit_.erase(transmit_.begin(), transmit_.begin() + wrote);
    }
}
}
