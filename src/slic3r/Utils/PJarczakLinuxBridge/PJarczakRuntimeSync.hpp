#pragma once

#include <filesystem>
#include <functional>
#include <string>

namespace Slic3r::PJarczakLinuxBridge {

// Copy the bridge runtime files found in `install_dir` into `plugin_folder` when they are
// missing or differ (size / mtime). `is_runtime_file` selects which file names take part.
// A loaded DLL is renamed aside to "<name>.old" so it can be replaced.
// Returns the number of files written.
int sync_runtime_files(const std::filesystem::path& install_dir,
                       const std::filesystem::path& plugin_folder,
                       const std::function<bool(const std::string&)>& is_runtime_file);

// Windows: refresh `plugin_folder` from the runtime installed next to the executable, so a new
// release's forwarder DLL / host binaries / scripts replace the previous release's copies in
// %APPDATA%. No-op on other platforms.
int sync_installed_runtime(const std::filesystem::path& plugin_folder);

}
