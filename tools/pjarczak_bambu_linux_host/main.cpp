#include "LinuxPluginHost.hpp"
#include "../../src/slic3r/Utils/PJarczakLinuxBridge/PJarczakLinuxSoBridgeRpcProtocol.hpp"

#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <execinfo.h>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>
#include <fstream>
#include <unistd.h>
#include <cstdio>
#include <thread>

using namespace Slic3r::PJarczakLinuxBridge;

namespace {
// The vendor libraries run in this process; a crash in them used to leave no trace.
// Print the signal, fault address and a backtrace to stderr (the host log) first.
void write_stderr(const char* s)
{
    [[maybe_unused]] auto unused = ::write(STDERR_FILENO, s, std::strlen(s));
}

void write_stderr_hex(std::uintptr_t v)
{
    char buf[2 + sizeof(v) * 2 + 1];
    char* p = buf + sizeof(buf) - 1;
    *p = '\0';
    do {
        *--p = "0123456789abcdef"[v & 0xf];
        v >>= 4;
    } while (v != 0);
    *--p = 'x';
    *--p = '0';
    write_stderr(p);
}

void crash_handler(int sig, siginfo_t* info, void*)
{
    write_stderr("[PJBRIDGE] fatal signal ");
    write_stderr_hex(static_cast<std::uintptr_t>(sig));
    write_stderr(" at address ");
    write_stderr_hex(reinterpret_cast<std::uintptr_t>(info ? info->si_addr : nullptr));
    write_stderr(", backtrace:\n");
    void* frames[64];
    const int n = ::backtrace(frames, 64);
    ::backtrace_symbols_fd(frames, n, STDERR_FILENO);
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

void install_crash_handler()
{
    void* warmup[1];
    ::backtrace(warmup, 1); // loads libgcc now, not inside the signal handler
    struct sigaction sa {};
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);
    for (int sig : {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT})
        ::sigaction(sig, &sa, nullptr);
}

int run_probe_auth()
{
    LinuxPluginHost host;
    auto hs = host.handle("bridge.handshake", nlohmann::json::object());
    if (!hs.value("network_loaded", false))
        return 2;

    const std::string log_dir = std::getenv("PJARCZAK_BAMBU_PROBE_LOG_DIR") ? std::getenv("PJARCZAK_BAMBU_PROBE_LOG_DIR") : std::string(".");
    const std::string country = std::getenv("PJARCZAK_BAMBU_COUNTRY_CODE") ? std::getenv("PJARCZAK_BAMBU_COUNTRY_CODE") : std::string("PL");
    auto created = host.handle("net.create_agent", {{"log_dir", log_dir}, {"country_code", country}});
    if (!created.value("ok", false))
        return 3;
    const auto agent = created.value("value", 0LL);
    if (agent <= 0)
        return 4;

    auto step = [&](const char* method, nlohmann::json payload) -> bool {
        payload["agent"] = agent;
        auto r = host.handle(method, payload);
        return r.value("ok", false);
    };

    if (!step("net.set_config_dir", {{"config_dir", log_dir}}))
        return 5;
    if (!step("net.init_log", nlohmann::json::object()))
        return 6;
    step("net.set_country_code", {{"country_code", country}});
    step("net.start", nlohmann::json::object());

    auto is_login = host.handle("net.is_user_login", {{"agent", agent}});
    if (!is_login.value("ok", false))
        return 7;

    return 0;
}
}

int main(int argc, char** argv)
{
    std::ios::sync_with_stdio(false);
    install_crash_handler();

    if (argc > 1 && std::string(argv[1]) == "--probe-auth")
        return run_probe_auth();

    const int rpc_fd = ::dup(STDOUT_FILENO);
    if (rpc_fd < 0)
        return 100;
    std::fflush(stdout);
    ::dup2(STDERR_FILENO, STDOUT_FILENO);
    std::ofstream rpc_out(std::string("/proc/self/fd/") + std::to_string(rpc_fd), std::ios::binary | std::ios::out);
    if (!rpc_out.good())
        return 101;

    LinuxPluginHost host;
    std::mutex out_mutex;

    while (true) {
        RawRpcFrame raw;
        std::string err;
        if (!read_raw_frame(std::cin, raw, err))
            break;

        RpcFrame req;
        if (!read_request_frame(raw, req, err)) {
            std::lock_guard<std::mutex> lock(out_mutex);
            write_json_frame(rpc_out, RpcFrameType::json_response, 0, {{"ok", false}, {"error", err}});
            continue;
        }

        std::vector<unsigned char> request_binary;
        if (req.payload.value("__binary_request", false)) {
            RawRpcFrame binary_raw;
            if (!read_raw_frame(std::cin, binary_raw, err) ||
                binary_raw.type != RpcFrameType::binary_data ||
                binary_raw.id != req.id) {
                std::lock_guard<std::mutex> lock(out_mutex);
                write_json_frame(rpc_out, RpcFrameType::json_response, req.id, {{"ok", false}, {"error", "missing binary request payload"}});
                continue;
            }
            request_binary = std::move(binary_raw.payload);
        }

        std::thread([&host, &rpc_out, &out_mutex, req_id = req.id, req_method = req.method, req_payload = req.payload, request_binary = std::move(request_binary)]() mutable {
            LinuxPluginHost::set_thread_request_binary(std::move(request_binary));
            nlohmann::json resp = host.handle(req_method, req_payload);

            std::vector<unsigned char> reply_binary;
            const bool has_reply_binary = LinuxPluginHost::consume_thread_reply_binary(reply_binary);

            std::lock_guard<std::mutex> lock(out_mutex);
            write_json_frame(rpc_out, RpcFrameType::json_response, req_id, resp);
            if (has_reply_binary)
                write_raw_frame(rpc_out, RpcFrameType::binary_data, req_id, reply_binary.data(), reply_binary.size());
        }).detach();
    }

    return 0;
}
