#include "FileWatcher.h"
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdatomic.h>
#include <stdexcept>
#include <string>
#include <sys/inotify.h>
#include <unistd.h>

std::string read_from_file(std::filesystem::path file) {}

void FileWatcher::file_watch_thread() {
  char buf[INOTIFY_BUF_SIZE];
  while (watching) {
    // block/wait for occurrence of an event
    int len = read(i_fd, &buf, sizeof(buf));

    // error while reading
    if (len == -1) {
      switch (errno) {
        // Interrupted used to stop the thread
      case EINTR:
        notify_cv->notify_all();
        return;
      default:
        std::cerr << "[ERROR]: While reading from inotify fd, errno: "
                  << strerror(errno) << std::endl;
      }
      // nothing read, should not happen
    } else if (len == 0) {
      std::cout << "[WARNING]: Nothing read from inotify fd" << std::endl;
      break;
      // file changed
    } else {

    }
  }
}

FileWatcher::FileWatcher(std::filesystem::path file)
    : _file_to_watch(file), _watching(false), _cv_mut(), _watching_thread(),
      _notify_waiter(), _watch_fd(-1) {
  _inotify_fd = inotify_init();
  if (_inotify_fd == -1) {
    std::stringstream ss;
    ss << "Inotify file descriptor couldn't get initialised, errno:  "
       << strerror(errno);
    throw std::runtime_error(ss.str().c_str());
  }
}

file_watch_error FileWatcher::start_watching(std::filesystem::path file) {
  if (_watching) {
    return file_watch_error::error_still_watching;
  }
  if (_file_to_watch != file) {
    _file_to_watch = file;
  }

  // add watcher for modification on current file
  int wd = inotify_add_watch(_inotify_fd, _file_to_watch.c_str(), IN_MODIFY);

  if (wd == -1) {
    switch (errno) {
    case EACCES | EEXIST | ENAMETOOLONG | ENOENT | ENOSPC:
      return static_cast<file_watch_error>(errno);
      break;
    default:
      std::cerr << "[ERROR]: Unexpected errno on add_watch with errno: "
                << strerror(errno) << std::endl;
      return file_watch_error::error_unknown;
    }
  } else {
    _watch_fd = wd;
  }
}
