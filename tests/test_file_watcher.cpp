#include "FileWatcher.h"
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char *argv[]) {
  FileWatcher fw(
      std::filesystem::path("/sys/class/backlight/intel_backlight/brightness"));

  file_watch_error started = fw.startWatching();

  if (started == file_watch_error::success) {
    std::cout << "Started successfully" << std::endl;
  } else {
    std::cout << "Error occured!" << std::endl;
    switch (started) {
    case file_watch_error::error_unknown:
      std::cout << "Unknown Error" << std::endl;
      break;
    case file_watch_error::error_still_watching:
      std::cout << "Already watching" << std::endl;
      break;
    default:
      std::cout << "Errno: " << strerror((int)started) << std::endl;
    }
    return -1;
  }

  while (true) {
    std::optional<std::string> st = fw.waitAndGet();
    std::cout << "Got '" << st.value_or("Nothing") << "' from FileWatcher"
              << std::endl;
  }

  return 0;
}
