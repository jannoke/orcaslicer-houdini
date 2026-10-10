#include "PJarczakRuntimeSync.hpp"

#include <boost/log/trivial.hpp>

#include "PJarczakLinuxBridgeConfig.hpp"

#include <fstream>
#include <iterator>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Slic3r::PJarczakLinuxBridge {

namespace fs = std::filesystem;

namespace {

// Shell scripts run by `sh` inside WSL must have LF line endings; a Windows checkout/installer may
// have turned them into CRLF (install_runtime.ps1 normalizes them for the same reason).
bool is_shell_script(const std::string& name)
{
    return name.size() > 3 && name.compare(name.size() - 3, 3, ".sh") == 0;
}

std::string read_lf_normalized(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i)
        if (!(text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n'))
            out += text[i];
    return out;
}

}

int sync_runtime_files(const fs::path& install_dir,
                       const fs::path& plugin_folder,
                       const std::function<bool(const std::string&)>& is_runtime_file)
{
    std::error_code ec;
    if (install_dir.empty() || !fs::is_directory(install_dir, ec) || fs::equivalent(install_dir, plugin_folder, ec))
        return 0;

    int updated = 0;
    for (const fs::directory_entry& entry : fs::directory_iterator(install_dir, ec)) {
        if (!entry.is_regular_file(ec))
            continue;
        const std::string name = entry.path().filename().string();
        if (!is_runtime_file(name))
            continue;

        const fs::path dst = plugin_folder / name;
        const bool exists = fs::exists(dst, ec);
        const bool script = is_shell_script(name);
        std::string script_content;
        if (script)
            script_content = read_lf_normalized(entry.path());
        if (exists) {
            const bool same = script ? read_lf_normalized(dst) == script_content
                                     : fs::file_size(dst, ec) == entry.file_size(ec) &&
                                           fs::last_write_time(dst, ec) == entry.last_write_time(ec);
            if (same)
                continue;
        }
        try {
            fs::create_directories(plugin_folder);
            if (exists) {
                fs::path aside = dst;
                aside += ".old";
                fs::remove(aside, ec);
                fs::rename(dst, aside, ec);
            }
            if (script) {
                std::ofstream(dst, std::ios::binary | std::ios::trunc) << script_content;
            } else {
                fs::copy_file(entry.path(), dst, fs::copy_options::overwrite_existing);
                fs::last_write_time(dst, entry.last_write_time());
            }
            ++updated;
            BOOST_LOG_TRIVIAL(info) << "PJarczakLinuxBridge: refreshed " << name << " from " << install_dir.string();
        } catch (const std::exception& e) {
            BOOST_LOG_TRIVIAL(error) << "PJarczakLinuxBridge: failed to refresh " << name << ": " << e.what();
        }
    }
    return updated;
}

int sync_installed_runtime(const fs::path& plugin_folder)
{
#if defined(_WIN32)
    wchar_t exe[MAX_PATH * 4] = {};
    if (!::GetModuleFileNameW(nullptr, exe, DWORD(sizeof(exe) / sizeof(exe[0]))))
        return 0;
    const std::string manifest = linux_payload_manifest_file_name();
    // The vendor Linux libraries and their manifest belong to the plugin downloader.
    return sync_runtime_files(
        fs::path(exe).parent_path(), plugin_folder,
        [&](const std::string& name) {
            return is_overlay_runtime_filename(name) && name != manifest && !is_linux_payload_filename(name);
        });
#else
    (void) plugin_folder;
    return 0;
#endif
}

}
