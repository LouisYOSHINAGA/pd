#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Steinberg {
namespace Vst {

// One folder of .vstpreset files, shown as one tab of the preset browser.
struct PresetFolder {
  std::string name;                            // tab title (UTF-8)
  std::filesystem::path directory;
  std::vector<std::filesystem::path> presets;  // sorted by file name
};

// The preset folders of the browser. The root folders are listed in a text
// file in the user's settings folder (rootsFile()), one path per line, '#'
// starting a comment; without the file, the standard VST3 user preset folder
// is used. Each root and each of its direct subfolders that holds .vstpreset
// files becomes one PresetFolder, the root first.
class PresetLibrary {
 public:
  // The scanned folders; scans on first use.
  const std::vector<PresetFolder>& folders();
  void rescan();
  // Appends `root` to the roots file (creating it, with the default root, if
  // missing) and rescans. Returns false if the file could not be written.
  bool addRoot(const std::filesystem::path& root);

  static std::filesystem::path rootsFile();

 private:
  std::vector<PresetFolder> folders_;
  bool scanned_ = false;
};

// The name the browser shows for a preset file: its file name without the
// extension (UTF-8).
std::string presetDisplayName(const std::filesystem::path& file);

// True if both paths name the same file or folder (tolerates differences in
// case or separators, as between a typed folder list and a file dialog).
bool isSamePath(const std::filesystem::path& a, const std::filesystem::path& b);

}  // namespace Vst
}  // namespace Steinberg
