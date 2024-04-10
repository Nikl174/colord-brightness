#include "FileWatcher.h"
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <pthread.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/inotify.h>
#include <thread>
#include <unistd.h>

void FileWatcher::fileWatchThread() {
  char buf[INOTIFY_BUF_SIZE];
  while (_watching) {
    // block/wait for occurrence of an event
    int len = read(_inotify_fd, &buf, sizeof(buf));

    // error while reading
    if (len == -1) {
      switch (errno) {
        // Interrupted used to stop the thread
      case EINTR:
        _notify_waiter_cv.notify_all();
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
      this->updateFileContent();
      _notify_waiter_cv.notify_one();
    }
  }
}

FileWatcher::FileWatcher(std::filesystem::path file)
    : _file_to_watch(file), _watching(false), _cv_mut(), _watching_thread(),
      _notify_waiter_cv(), _watch_fd(-1) {
  _inotify_fd = inotify_init();
  if (_inotify_fd == -1) {
    std::stringstream ss;
    ss << "Inotify file descriptor couldn't get initialised, errno:  "
       << strerror(errno);
    throw std::runtime_error(ss.str().c_str());
  }
}

file_watch_error FileWatcher::startWatching(std::filesystem::path file) {
  if (_watching) {
    return file_watch_error::error_still_watching;
  }
  if (_file_to_watch != file) {
    _file_to_watch = file;
  }

  // add watcher for modification on current file
  int wd = inotify_add_watch(_inotify_fd, _file_to_watch.c_str(), IN_MODIFY);

  // handle errors
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

  // start thread
  _watching.store(true);
  auto thread_f = std::bind(&FileWatcher::fileWatchThread, this);
  _watching_thread = std::thread(thread_f);

  return file_watch_error::success;
}

std::optional<std::string> FileWatcher::waitAndGet() {
  if (_watching) {
    std::unique_lock<std::mutex> lk;
    _notify_waiter_cv.wait(lk);
    return _changed_file_content;
  } else {
    return std::nullopt;
  }
}

void FileWatcher::setFileContent(std::string new_content) {
  std::lock_guard<std::mutex> lk(_cv_mut);
  _changed_file_content = new_content;
}

bool FileWatcher::stopWatching() {
  _watching.store(false);
  if (_watching_thread.joinable()) {
    // send a signal to stop the reading for the fd/waiting for a file
    // modification if needed
    int send_sig = pthread_kill(_watching_thread.native_handle(), SIGABRT);
    if (send_sig != 0) {
      return false;
    }
    _watching_thread.join();
  }
  _notify_waiter_cv.notify_all();
  return true;
}

bool FileWatcher::updateFileContent() {
  std::ifstream file(_file_to_watch);
  if (file.is_open()) {
    char c;
    std::stringstream ss;
    while (file.get(c)) {
      ss << c;
    }
    file.close();
    setFileContent(ss.str());
    return true;
  } else {
    return false;
  }
}

FileWatcher::~FileWatcher() {
  if (_watching) {
    stopWatching();
  }
  if (inotify_rm_watch(_inotify_fd, _watch_fd) == -1) {
    std::cerr << "[ERROR]: Couldnt close watch fd, errno: " << strerror(errno)
              << std::endl;
  }
  close(_inotify_fd);
}
