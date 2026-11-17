#include "PJarczakBambuNetworkForwarderState.hpp"
#include "PJarczakLinuxSoBridgeRpcClient.hpp"

#include <functional>
#if defined(_WIN32)
#include <codecvt>
#include <locale>
#endif

namespace Slic3r::PJarczakLinuxBridge {
namespace {
std::mutex g_remote_agents_mutex;
std::map<std::int64_t, BridgeAgent*> g_remote_agents;
std::mutex g_remote_tunnels_mutex;
std::map<std::int64_t, BridgeTunnel*> g_remote_tunnels;

void run_or_queue(BridgeAgent* agent, std::function<void()> fn)
{
    if (!agent)
        return;
    if (auto queue = get_callback(agent, &BridgeAgent::queue_on_main))
        queue(std::move(fn));
    else
        fn();
}

// Calls one of Orca's agent callbacks for an event from the host, on Orca's main thread when it
// gave us queue_on_main. The callback is looked up when the call runs, not when the event
// arrives: on exit Orca clears its callbacks and then runs the calls already queued (GUI_App's
// "draining callbacks"), so by then a callback can be empty (std::bad_function_call, which Orca
// reports as an unhandled exception on closing) or the agent deleted.
template <class Fn, class... Args>
void deliver(BridgeAgent* agent, Fn BridgeAgent::*member, Args... args)
{
    run_or_queue(agent, [remote_handle = agent->remote_handle, member, args...] {
        BridgeAgent* current = find_remote_agent(remote_handle);
        if (!current)
            return;
        if (Fn fn = get_callback(current, member))
            fn(args...);
    });
}

// Calls a job's on_update_status / on_progress on the event pump thread (the native plug-in calls
// them on its own thread as well), unless the job call has returned.
void call_job_callback(const std::shared_ptr<BridgeJobState>& job, const std::function<void()>& fn)
{
    std::lock_guard<std::mutex> lock(job->callbacks_mutex);
    if (!job->finished)
        fn();
}

// Answers the host's job.wait / job.check: runs Orca's callback and sends its result back. The
// callback gets its own thread because Orca's print job waits in on_wait for the printer to report
// the new job, and those printer messages arrive through this event pump. The host holds the job
// call open until the reply, and unregister_job_state() joins the thread before the call returns.
void answer_job_callback(const std::shared_ptr<BridgeJobState>& job, std::int64_t job_id, std::int64_t request_id, std::function<bool()> ask)
{
    auto reply = [job_id, request_id](bool value) {
        RpcClient::instance().invoke_void("bridge.job_wait_reply", {{"job_id", job_id}, {"request_id", request_id}, {"reply", value}});
    };
    if (job) {
        std::lock_guard<std::mutex> lock(job->callbacks_mutex);
        if (!job->finished) {
            job->callback_threads.emplace_back([ask = std::move(ask), reply] {
                bool value = true;
                try {
                    value = ask();
                } catch (...) {
                }
                reply(value);
            });
            return;
        }
    }
    reply(true);
}

#if defined(_WIN32)
std::wstring utf8_to_wstring(const std::string& s)
{
    try {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        return conv.from_bytes(s);
    } catch (...) {
        return std::wstring(s.begin(), s.end());
    }
}
#endif
}

BridgeAgent* as_agent(void* handle)
{
    return reinterpret_cast<BridgeAgent*>(handle);
}

void* new_agent(const std::string& log_dir)
{
    auto* agent = new BridgeAgent();
    agent->log_dir = log_dir;
    return agent;
}

int delete_agent(void* handle)
{
    auto* agent = as_agent(handle);
    unregister_remote_agent(agent);
    delete agent;
    return 0;
}

BridgeTunnel* as_tunnel_handle(void* handle)
{
    return reinterpret_cast<BridgeTunnel*>(handle);
}

void register_remote_agent(BridgeAgent* agent)
{
    if (!agent || !agent->remote_handle)
        return;
    std::lock_guard<std::mutex> lock(g_remote_agents_mutex);
    g_remote_agents[agent->remote_handle] = agent;
}

void unregister_remote_agent(BridgeAgent* agent)
{
    if (!agent || !agent->remote_handle)
        return;
    std::lock_guard<std::mutex> lock(g_remote_agents_mutex);
    auto it = g_remote_agents.find(agent->remote_handle);
    if (it != g_remote_agents.end() && it->second == agent)
        g_remote_agents.erase(it);
}

BridgeAgent* find_remote_agent(std::int64_t remote_handle)
{
    std::lock_guard<std::mutex> lock(g_remote_agents_mutex);
    auto it = g_remote_agents.find(remote_handle);
    return it == g_remote_agents.end() ? nullptr : it->second;
}

void register_remote_tunnel(BridgeTunnel* tunnel)
{
    if (!tunnel || !tunnel->remote_handle)
        return;
    std::lock_guard<std::mutex> lock(g_remote_tunnels_mutex);
    g_remote_tunnels[tunnel->remote_handle] = tunnel;
}

void unregister_remote_tunnel(BridgeTunnel* tunnel)
{
    if (!tunnel || !tunnel->remote_handle)
        return;
    std::lock_guard<std::mutex> lock(g_remote_tunnels_mutex);
    auto it = g_remote_tunnels.find(tunnel->remote_handle);
    if (it != g_remote_tunnels.end() && it->second == tunnel)
        g_remote_tunnels.erase(it);
}

BridgeTunnel* find_remote_tunnel(std::int64_t remote_handle)
{
    std::lock_guard<std::mutex> lock(g_remote_tunnels_mutex);
    auto it = g_remote_tunnels.find(remote_handle);
    return it == g_remote_tunnels.end() ? nullptr : it->second;
}

void dispatch_agent_event(std::int64_t remote_handle, const std::string& name, const nlohmann::json& payload)
{
    BridgeAgent* agent = find_remote_agent(remote_handle);
    if (!agent)
        return;

    if (name == "on_ssdp_msg") {
        const auto msg = payload.value("dev_info_json_str", std::string());
        deliver(agent, &BridgeAgent::on_ssdp_msg, msg);
        return;
    }
    if (name == "on_user_login") {
        const int online_login = payload.value("online_login", 0);
        const bool login = payload.value("login", false);
        agent->logged_in = login;
        deliver(agent, &BridgeAgent::on_user_login, online_login, login);
        return;
    }
    if (name == "on_printer_connected") {
        const auto topic = payload.value("topic_str", std::string());
        deliver(agent, &BridgeAgent::on_printer_connected, topic);
        return;
    }
    if (name == "on_server_connected") {
        const int return_code = payload.value("return_code", 0);
        const int reason_code = payload.value("reason_code", 0);
        agent->server_connected = return_code == 0;
        deliver(agent, &BridgeAgent::on_server_connected, return_code, reason_code);
        return;
    }
    if (name == "on_http_error") {
        const unsigned http_code = payload.value("http_code", 0u);
        const auto body = payload.value("http_body", std::string());
        deliver(agent, &BridgeAgent::on_http_error, http_code, body);
        return;
    }
    if (name == "on_subscribe_failure") {
        const auto topic = payload.value("topic", std::string());
        deliver(agent, &BridgeAgent::on_subscribe_failure, topic);
        return;
    }
    if (name == "callback.get_country_code") {
        std::string value;
        if (auto get_country_code = get_callback(agent, &BridgeAgent::get_country_code))
            value = get_country_code();
        else
            value = agent->country_code;
        RpcClient::instance().invoke_void("bridge.callback_reply", {{"request_id", payload.value("request_id", 0LL)}, {"value", value}});
        return;
    }
    if (name == "on_message") {
        const auto dev_id = payload.value("dev_id", std::string());
        const auto msg = payload.value("msg", std::string());
        deliver(agent, &BridgeAgent::on_message, dev_id, msg);
        return;
    }
    if (name == "on_user_message") {
        const auto dev_id = payload.value("dev_id", std::string());
        const auto msg = payload.value("msg", std::string());
        deliver(agent, &BridgeAgent::on_user_message, dev_id, msg);
        return;
    }
    if (name == "on_local_connect") {
        const int status = payload.value("status", 0);
        const auto dev_id = payload.value("dev_id", std::string());
        const auto msg = payload.value("msg", std::string());
        deliver(agent, &BridgeAgent::on_local_connect, status, dev_id, msg);
        return;
    }
    if (name == "on_local_message") {
        const auto dev_id = payload.value("dev_id", std::string());
        const auto msg = payload.value("msg", std::string());
        deliver(agent, &BridgeAgent::on_local_message, dev_id, msg);
        return;
    }
    if (name == "on_server_error") {
        const auto url = payload.value("url", std::string());
        const int status = payload.value("status", 0);
        deliver(agent, &BridgeAgent::on_server_error, url, status);
        return;
    }
    if (name == "job.update_status") {
        auto job = find_job_state(agent, payload.value("job_id", 0LL));
        if (job && job->on_update_status) {
            const int status = payload.value("status", 0);
            const int code = payload.value("code", 0);
            const auto msg = payload.value("msg", std::string());
            call_job_callback(job, [&] { job->on_update_status(status, code, msg); });
        }
        return;
    }
    if (name == "job.progress") {
        auto job = find_job_state(agent, payload.value("job_id", 0LL));
        if (job && job->on_progress) {
            const int progress = payload.value("progress", 0);
            call_job_callback(job, [&] { job->on_progress(progress); });
        }
        return;
    }
    if (name == "job.check") {
        const auto job_id = payload.value("job_id", 0LL);
        auto job = find_job_state(agent, job_id);
        std::map<std::string, std::string> info;
        if (payload.contains("info") && payload["info"].is_object()) {
            for (auto it = payload["info"].begin(); it != payload["info"].end(); ++it)
                info[it.key()] = it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
        }
        answer_job_callback(job, job_id, payload.value("request_id", 0LL), [job, info] { return job->on_check ? job->on_check(info) : true; });
        return;
    }
    if (name == "job.wait") {
        const auto job_id = payload.value("job_id", 0LL);
        auto job = find_job_state(agent, job_id);
        const int status = payload.value("status", 0);
        const auto info = payload.value("job_info", std::string());
        answer_job_callback(job, job_id, payload.value("request_id", 0LL), [job, status, info] { return job->on_wait ? job->on_wait(status, info) : true; });
        return;
    }
    if (name == "job.complete") {
        auto job = find_job_state(agent, payload.value("job_id", 0LL));
        if (job && job->out_string && payload.contains("out"))
            call_job_callback(job, [&] { *job->out_string = payload.value("out", std::string()); });
        return;
    }
}

void dispatch_tunnel_event(std::int64_t remote_handle, const std::string& name, const nlohmann::json& payload)
{
    BridgeTunnel* tunnel = find_remote_tunnel(remote_handle);
    if (!tunnel)
        return;
    if (name == "logger" && tunnel->logger) {
        const int level = payload.value("level", 0);
        const auto message = payload.value("message", std::string());
        tunnel->logger_message_utf8 = message;
#if defined(_WIN32)
        tunnel->logger_message_wide = utf8_to_wstring(message);
        tunnel->logger(tunnel->logger_ctx, level, tunnel->logger_message_wide.c_str());
#else
        tunnel->logger(tunnel->logger_ctx, level, tunnel->logger_message_utf8.c_str());
#endif
    }
}

std::shared_ptr<BridgeJobState> register_job_state(BridgeAgent* agent, const std::shared_ptr<BridgeJobState>& job)
{
    if (!agent || !job)
        return nullptr;
    std::lock_guard<std::mutex> lock(agent->jobs_mutex);
    agent->jobs[job->job_id] = job;
    return job;
}

std::shared_ptr<BridgeJobState> find_job_state(BridgeAgent* agent, std::int64_t job_id)
{
    if (!agent)
        return nullptr;
    std::lock_guard<std::mutex> lock(agent->jobs_mutex);
    auto it = agent->jobs.find(job_id);
    return it == agent->jobs.end() ? nullptr : it->second;
}

void unregister_job_state(BridgeAgent* agent, std::int64_t job_id)
{
    if (!agent)
        return;
    std::shared_ptr<BridgeJobState> job;
    {
        std::lock_guard<std::mutex> lock(agent->jobs_mutex);
        auto it = agent->jobs.find(job_id);
        if (it != agent->jobs.end()) {
            job = it->second;
            agent->jobs.erase(it);
        }
    }
    if (job) {
        job->stop_cancel_watch = true;
        if (job->cancel_watch.joinable())
            job->cancel_watch.join();
        std::vector<std::thread> callback_threads;
        {
            std::lock_guard<std::mutex> lock(job->callbacks_mutex);
            job->finished = true;
            callback_threads.swap(job->callback_threads);
        }
        for (auto& thread : callback_threads)
            thread.join();
    }
}

}
