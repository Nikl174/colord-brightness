#include "FileWatcher.h"
#include <cassert>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char *argv[]) {
  {
    std::ofstream test_file("test");
    FileWatcher fw(std::filesystem::path("test"));
    assert(test_file.is_open());

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
    auto fw_wait_and_get = std::bind(&FileWatcher::waitAndGet, &fw);
    auto file_change_output = std::async(std::launch::async, fw_wait_and_get);

    std::string test_string = "hilarious text";

    test_file << test_string;

    test_file.close();

    std::future_status status = file_change_output.wait_for(
        std::chrono::duration(std::chrono::milliseconds(500)));

    assert(status == std::future_status::ready);

    std::optional<std::basic_string<char>> test_return =
        file_change_output.get();
    assert(test_return.has_value());
    assert(test_return.value() == test_string);
  }
  std::cout << "Success!" << std::endl;
  return 0;
}
