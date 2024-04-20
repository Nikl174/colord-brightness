#include "FileWatcher.h"
#include <atomic>
#include <cerrno>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <easylogging++.h>
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
#include <system_error>
#include <thread>
#include <unistd.h>

// -----------------Helper  functions----------------
void sig_int_handler(int sig) {
  LOG(DEBUG) << "Received sig: " << sig
             << " in thread: " << std::this_thread::get_id();
}

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
    LOG(DEBUG) << "File content updated to: " << ss.str();
    return true;
  } else {
    LOG(WARNING) << "Couldn't update file content because file was not open!";
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
    LOG(DEBUG) << "FileWatcher waiting for events on file " << file_path
               << "...";
    int len = read(inot_fd, &buf, sizeof(buf));

    // error while reading
    if (len == -1) {
      switch (errno) {
        // Interrupted used to stop the thread
      case EINTR:
        cv->notify_all();
        return;
      default:
        LOG(ERROR) << "While reading from inotify fd, errno: "
                   << strerror(errno);
      }
      // nothing read, should not happen
    } else if (len == 0) {
      LOG(WARNING) << "Nothing read from inotify fd";
      break;
    } else {
      // file changed
      LOG(DEBUG) << "FileWatcher read " << len << " bytes";
      *updated = updateFileContent(cv_mut, file_path, file_content);
    }
    cv->notify_one();
  }
}

// -------------------------------------

FileWatcher::FileWatcher(std::filesystem::path file)
    : _file_to_watch(file), _updated(std::make_shared<std::atomic_bool>(false)),
      _watching(std::make_shared<std::atomic_bool>(false)),
      _cv_mut(std::make_shared<std::mutex>()), _watching_thread(),
      _notify_waiter_cv(std::make_shared<std::condition_variable>()),
      _changed_file_content(std::make_shared<std::string>("")), _watch_fd(-1) {

  if (!std::filesystem::exists(file)) {
      throw std::runtime_error("File does not exist!");
  }
  _inotify_fd = inotify_init();
  if (_inotify_fd == -1) {
    std::stringstream ss;
    ss << "Inotify file descriptor couldn't get initialised, errno:  "
       << strerror(errno);
    throw std::system_error(errno, std::generic_category(), ss.str());
  }

  // set a signal handler, std::signal does not work, because we want to mask
  // the SIGINT for the thread stopping (the signal used can be changed though)
  struct sigaction sa = {&sig_int_handler, 0, 0};
  /*! TODO: throw exeception or transfer to startWatching and propagate error?
   *  \todo throw exeception or transfer to startWatching and propagate error?
   */
  LOG_IF(sigaction(SIGINT, &sa, NULL) != 0, WARNING)
      << "Signal Handler for 'SIGINT' couldn't register, errno: "
      << strerror(errno);
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
      LOG(ERROR) << "Unexpected errno on add_watch with errno: "
                 << strerror(errno);
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
    LOG(DEBUG) << "Waiting for event from Filewatcher ...";
    _notify_waiter_cv->wait(lk);
    return *_changed_file_content;
  } else {
    LOG(WARNING) << "Filewatcher is not running!";
    return std::nullopt;
  }
}

template <class Rep, class Period>
std::optional<std::string>
FileWatcher::waitForAndGet(std::chrono::duration<Rep, Period> time) {
  if (*_watching) {
    std::unique_lock<std::mutex> lk(*_cv_mut);
    auto updated = _updated;
    LOG(DEBUG) << "Waiting " << time << " for event from Filewatcher ...";
    std::cv_status changed = _notify_waiter_cv->wait_for(lk, time);
    if (changed == std::cv_status::no_timeout) {
      return *_changed_file_content;
    } else {
      LOG(DEBUG) << "Timeout after waiting " << time << " for a file-change.";
      return std::nullopt;
    }
  } else {
    LOG(WARNING) << "Filewatcher is not running!";
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
      bool updated =
          updateFileContent(_cv_mut, _file_to_watch, _changed_file_content);
      if (updated) {
        return *_changed_file_content;
      } else {
        LOG(WARNING) << "File-content not updated!";
        return std::nullopt;
      }
    }
  }
  return std::nullopt;
}

bool FileWatcher::stopWatching() {
  _watching->store(false);
  if (_watching_thread.joinable()) {
    // send a signal to stop the reading for the fd/waiting for a file
    // modification if needed
    int send_sig = pthread_kill(_watching_thread.native_handle(), SIGINT);
    if (send_sig != 0) {
      LOG(ERROR) << "Couldnt send signal to thread, errno: " << strerror(errno);
      return false;
    }
    _watching_thread.join();
    if (inotify_rm_watch(_inotify_fd, _watch_fd) == -1) {
      LOG(ERROR) << "Couldnt close watch fd, errno: " << strerror(errno);
    }
  }
  _notify_waiter_cv->notify_all();
  LOG(DEBUG) << "Stopped watching" << std::endl;
  return true;
}

FileWatcher::~FileWatcher() {
  if (*_watching) {
    LOG_IF(!stopWatching(), ERROR) << "Watching thread couldn't get stopped!";
  };
  LOG_IF(close(_inotify_fd) != 0, ERROR)
      << "Couldnt close inotify fd, errno: " << strerror(errno);
}
