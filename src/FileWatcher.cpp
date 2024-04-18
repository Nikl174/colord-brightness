#include "FileWatcher.h"
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <poll.h>
#include <pthread.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <thread>
#include <unistd.h>

bool updateFileContent(std::shared_ptr<std::mutex> mut,
                       std::filesystem::path file_path,
                       std::shared_ptr<std::string> changed_file_content) {
  std::lock_guard<std::mutex> lk(*mut);
  std::ifstream file(file_path);
  if (file.is_open()) {
    char c;
    std::stringstream ss;
    while (file.get(c)) {
      ss << c;
    }
    file.close();
    *changed_file_content = ss.str();
    return true;
  } else {
    return false;
  }
}

void fileWatchThread(int inot_fd, std::filesystem::path file_path,
                     std::shared_ptr<std::atomic_bool> watching,
                     std::shared_ptr<std::atomic_bool> updated,
                     std::shared_ptr<std::condition_variable> cv,
                     std::shared_ptr<std::mutex> cv_mut,
                     std::shared_ptr<std::string> file_content) {
  char buf[INOTIFY_BUF_SIZE];
  while (*watching) {
    // block/wait for occurrence of an event
    std::cout << "[DEBUG]: FileWatcher waiting for events on file " << file_path
              << "..." << std::endl;
    int len = read(inot_fd, &buf, sizeof(buf));

    // error while reading
    if (len == -1) {
      switch (errno) {
        // Interrupted used to stop the thread
      case EINTR:
        cv->notify_all();
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
      std::cout << "[DEBUG]: FileWatcher read " << len << " bytes" << std::endl;
      *updated = updateFileContent(cv_mut, file_path, file_content);
    }
    cv->notify_one();
  }
}

FileWatcher::FileWatcher(std::filesystem::path file)
    : _file_to_watch(file), _updated(std::make_shared<std::atomic_bool>(false)),
      _watching(std::make_shared<std::atomic_bool>(false)),
      _cv_mut(std::make_shared<std::mutex>()), _watching_thread(),
      _notify_waiter_cv(std::make_shared<std::condition_variable>()),
      _changed_file_content(std::make_shared<std::string>("")), _watch_fd(-1) {
  _inotify_fd = inotify_init();
  if (_inotify_fd == -1) {
    std::stringstream ss;
    ss << "Inotify file descriptor couldn't get initialised, errno:  "
       << strerror(errno);
    throw std::runtime_error(ss.str().c_str());
  }
}

file_watch_error FileWatcher::startWatching(std::filesystem::path file) {
  if (*_watching) {
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
  _watching->store(true);
  _watching_thread =
      std::thread(&fileWatchThread, _inotify_fd, _file_to_watch, _watching,
                  _updated, _notify_waiter_cv, _cv_mut, _changed_file_content);

  return file_watch_error::success;
}

file_watch_error FileWatcher::startWatching() {
  return startWatching(_file_to_watch);
}

std::optional<std::string> FileWatcher::waitAndGet() {
  if (*_watching) {
    std::unique_lock<std::mutex> lk(*_cv_mut);
    auto updated = _updated;
    std::cout << "[DEBUG]: Waiting for event from Filewatcher ..." << std::endl;
    _notify_waiter_cv->wait(lk);
    return *_changed_file_content;
  } else {
    return std::nullopt;
  }
}

template <class Rep, class Period>
std::optional<std::string>
FileWatcher::waitForAndGet(std::chrono::duration<Rep, Period> time) {
  if (*_watching) {
    std::unique_lock<std::mutex> lk(*_cv_mut);
    auto updated = _updated;
    std::cout << "[DEBUG]: Waiting for event from Filewatcher ..." << std::endl;
    _notify_waiter_cv->wait_for(lk,time);
    return *_changed_file_content;
  } else {
    return std::nullopt;
  }
}

/*! TODO: Improve overall structure
 *  \todo Improve overall structure
 */
std::optional<std::string> FileWatcher::getWhenChanged() {
  pollfd fds;
  fds.fd = _inotify_fd;
  int ret = poll(&fds, 1, 0);
  if (ret > 0) {
    char buf[INOTIFY_BUF_SIZE];
    int len = read(_inotify_fd, buf, sizeof(buf));
    if (len > 0) {
      updateFileContent(_cv_mut, _file_to_watch, _changed_file_content);
      return *_changed_file_content;
    }
  }
  return std::nullopt;
}

bool FileWatcher::stopWatching() {
  _watching->store(false);
  if (_watching_thread.joinable()) {
    // send a signal to stop the reading for the fd/waiting for a file
    // modification if needed
    int send_sig = pthread_kill(_watching_thread.native_handle(), SIGABRT);
    if (send_sig != 0) {
      return false;
    }
    _watching_thread.join();
    if (inotify_rm_watch(_inotify_fd, _watch_fd) == -1) {
      std::cerr << "[ERROR]: Couldnt close watch fd, errno: " << strerror(errno)
                << std::endl;
    }
  }
  _notify_waiter_cv->notify_all();
  return true;
}

FileWatcher::~FileWatcher() {
  if (*_watching) {
    stopWatching();
  }
  close(_inotify_fd);
}
