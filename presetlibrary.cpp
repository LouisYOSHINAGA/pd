#include "presetlibrary.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <system_error>

#if defined(_WIN32)
#include <shlobj.h>
#endif

#include "config.h"

namespace Steinberg {
namespace Vst {

namespace fs = std::filesystem;

namespace {

constexpr const char* kPresetExtension = ".vstpreset";
constexpr const char* kRootsFileName = "preset_folders.txt";

#if defined(_WIN32)
fs::path knownFolder(REFKNOWNFOLDERID id) {
  PWSTR raw = nullptr;
  fs::path result;
  if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &raw))) {
    result = raw;
  }
  CoTaskMemFree(raw);
  return result;
}
#else
fs::path homeFolder() {
  const char* home = std::getenv("HOME");
  return home != nullptr ? fs::path(home) : fs::path();
}
#endif

// Folder of the user's settings, where the roots file lives.
fs::path settingsFolder() {
#if defined(_WIN32)
  fs::path base = knownFolder(FOLDERID_RoamingAppData);
#else
  fs::path base = homeFolder();
  if (!base.empty()) {
    base = base / "Library" / "Application Support";
  }
#endif
  return base.empty() ? base : base / MYVST_VENDOR / MYVST_VSTNAME;
}

// The standard VST3 user preset location of this plug-in.
fs::path defaultRoot() {
#if defined(_WIN32)
  fs::path base = knownFolder(FOLDERID_Documents);
  if (!base.empty()) {
    base = base / "VST3 Presets";
  }
#else
  fs::path base = homeFolder();
  if (!base.empty()) {
    base = base / "Library" / "Audio" / "Presets";
  }
#endif
  return base.empty() ? base : base / MYVST_VENDOR / MYVST_VSTNAME;
}

std::string toLowerAscii(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return text;
}

// Sorts paths by file name, ignoring ASCII case (as Explorer lists them).
void sortByFileName(std::vector<fs::path>& paths) {
  std::sort(paths.begin(), paths.end(), [](const fs::path& a, const fs::path& b) {
    return toLowerAscii(a.filename().u8string()) < toLowerAscii(b.filename().u8string());
  });
}

// Strips surrounding whitespace and one pair of double quotes (as added by
// Explorer's "Copy as path").
std::string trimLine(std::string line) {
  auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
  while (!line.empty() && isSpace(line.back())) {
    line.pop_back();
  }
  size_t begin = 0;
  while (begin < line.size() && isSpace(line[begin])) {
    begin++;
  }
  line.erase(0, begin);
  if (line.size() >= 2 && line.front() == '"' && line.back() == '"') {
    line = line.substr(1, line.size() - 2);
  }
  return line;
}

std::vector<fs::path> readRoots() {
  std::ifstream in(PresetLibrary::rootsFile());
  if (!in) {
    fs::path root = defaultRoot();
    return root.empty() ? std::vector<fs::path>{} : std::vector<fs::path>{root};
  }
  std::vector<fs::path> roots;
  std::string line;
  bool firstLine = true;
  while (std::getline(in, line)) {
    if (firstLine && line.compare(0, 3, "\xEF\xBB\xBF") == 0) {  // UTF-8 BOM (Notepad)
      line.erase(0, 3);
    }
    firstLine = false;
    line = trimLine(line);
    if (!line.empty() && line[0] != '#') {
      roots.push_back(fs::u8path(line));
    }
  }
  return roots;
}

bool endsWithNewline(const fs::path& file) {
  std::ifstream in(file, std::ios::binary | std::ios::ate);
  if (!in || in.tellg() <= 0) {
    return true;
  }
  in.seekg(-1, std::ios::end);
  return in.get() == '\n';
}

bool hasPresetExtension(const fs::path& file) {
  return toLowerAscii(file.extension().u8string()) == kPresetExtension;
}

std::vector<fs::path> subdirectories(const fs::path& directory) {
  std::vector<fs::path> result;
  std::error_code ec;
  for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code typeEc;
    if (it->is_directory(typeEc)) {
      result.push_back(it->path());
    }
  }
  sortByFileName(result);
  return result;
}

// Appends `directory` to `out` if it holds preset files.
void appendFolder(const fs::path& directory, std::vector<PresetFolder>& out) {
  PresetFolder folder;
  folder.directory = directory;
  folder.name = directory.has_filename() ? directory.filename().u8string()
                                         : directory.u8string();
  std::error_code ec;
  for (fs::directory_iterator it(directory, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code typeEc;
    if (it->is_regular_file(typeEc) && hasPresetExtension(it->path())) {
      folder.presets.push_back(it->path());
    }
  }
  if (folder.presets.empty()) {
    return;
  }
  sortByFileName(folder.presets);
  out.push_back(std::move(folder));
}

}  // namespace

const std::vector<PresetFolder>& PresetLibrary::folders() {
  if (!scanned_) {
    rescan();
  }
  return folders_;
}

void PresetLibrary::rescan() {
  folders_.clear();
  for (fs::path root : readRoots()) {
    root = root.lexically_normal();
    if (!root.has_filename() && root.has_relative_path()) {  // "C:\presets\" -> "C:\presets"
      root = root.parent_path();
    }
    appendFolder(root, folders_);
    for (const fs::path& subdirectory : subdirectories(root)) {
      appendFolder(subdirectory, folders_);
    }
  }
  scanned_ = true;
}

bool PresetLibrary::addRoot(const fs::path& root) {
  const fs::path file = rootsFile();
  if (file.empty()) {
    return false;
  }
  const std::vector<fs::path> roots = readRoots();
  bool listed = std::any_of(roots.begin(), roots.end(),
                            [&root](const fs::path& known) { return isSamePath(known, root); });
  if (!listed) {
    std::error_code ec;
    const bool exists = fs::exists(file, ec);
    fs::create_directories(file.parent_path(), ec);
    const bool needsNewline = exists && !endsWithNewline(file);
    std::ofstream out(file, std::ios::app | std::ios::binary);
    if (!out) {
      return false;
    }
    if (needsNewline) {
      out << "\n";
    }
    if (!exists) {
      out << "# PD preset browser: one preset folder per line ('#' starts a comment).\n"
          << "# Each folder and each of its direct subfolders holding .vstpreset files\n"
          << "# becomes one tab.\n";
      for (const fs::path& known : roots) {  // the default root, so it stays listed
        out << known.u8string() << "\n";
      }
    }
    out << root.u8string() << "\n";
    if (!out) {
      return false;
    }
  }
  rescan();
  return true;
}

fs::path PresetLibrary::rootsFile() {
  fs::path folder = settingsFolder();
  return folder.empty() ? folder : folder / kRootsFileName;
}

std::string presetDisplayName(const fs::path& file) {
  return file.stem().u8string();
}

bool isSamePath(const fs::path& a, const fs::path& b) {
  if (a.empty() || b.empty()) {
    return false;
  }
  if (a == b) {
    return true;
  }
  std::error_code ec;
  return fs::equivalent(a, b, ec);
}

}  // namespace Vst
}  // namespace Steinberg
