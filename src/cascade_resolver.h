#ifndef FOCUS_MONITOR_CASCADE_RESOLVER_H_
#define FOCUS_MONITOR_CASCADE_RESOLVER_H_

#include <array>
#include <filesystem>
#include <string>
#include <vector>

// Standard install locations for haarcascade_frontalface_default.xml,
// searched in order when no explicit path is given. "data/..." assumes
// the binary is run from the project root (see README); the rest are
// where various Linux package managers install OpenCV's own cascade
// data.
//
// Pulled out of main.cpp (which used to define this, plus
// ResolveCascadePath below, inline) so this logic -- previously untested,
// unlike almost everything else in src/ -- can be exercised on its own.
// It originally used cv::utils::fs::exists(); switched to
// std::filesystem::exists() here, which checks the same thing without
// pulling OpenCV into a file that otherwise has no need for it.
inline const std::array<std::string, 4>& DefaultCascadeSearchPaths() {
  static const std::array<std::string, 4> paths = {
      "data/haarcascades/haarcascade_frontalface_default.xml",
      "/usr/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
      "/usr/local/share/opencv4/haarcascades/haarcascade_frontalface_default.xml",
      "/usr/share/opencv/haarcascades/haarcascade_frontalface_default.xml",
  };
  return paths;
}

// Returns explicit_path unchanged if it's non-empty (an explicit request
// always wins, even if that path doesn't actually exist -- the caller,
// cv::CascadeClassifier::load(), reports that failure itself with a
// clearer message than this function could). Otherwise returns the
// first of search_paths that exists on disk, in order; if none do,
// returns the first entry anyway (an empty string if search_paths
// itself is empty), so the caller gets a real path to name in its own
// "failed to load" error instead of silently failing on an empty one.
inline std::string ResolveCascadePath(const std::string& explicit_path,
                                       const std::vector<std::string>& search_paths) {
  if (!explicit_path.empty()) {
    return explicit_path;
  }
  for (const auto& path : search_paths) {
    if (std::filesystem::exists(path)) {
      return path;
    }
  }
  return search_paths.empty() ? std::string() : search_paths.front();
}

#endif  // FOCUS_MONITOR_CASCADE_RESOLVER_H_
