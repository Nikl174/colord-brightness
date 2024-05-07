#ifndef FILEWATCHER_H

#define FILEWATCHER_H
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <sys/inotify.h>
#include <thread>

#define INOTIFY_BUF_SIZE 4096

/*! \enum file_watch_error
 *
 *  errors on starting the filewatcher, wrapping errno-values
 */
enum class file_watch_error {
  error_unknown,
  error_still_watching,
  error_read_access = EACCES,
  error_already_watched = EEXIST,
  error_path_to_long = ENAMETOOLONG,
  error_cant_enter_dir = ENOENT,
  error_to_many_watchers = ENOSPC,
  success = 0
};

/*! \class FileWatcher
 *  \brief Asynchronous watcher for file changes
 *
 *  Watches a single file for changes and returnes the content if changed.
 *  Uses inotify-events provided by the linux kernel.
 */
class FileWatcher {
public:
  /*! \brief Constructor initialises the inotify-file-descriptor and signal
   * handling
   *
   *  initialises shared_ptr and inotify-context and adds a signal handler for
   * SIGINT used to stop the thread, when its started later with
   * FileWatcher::startWatching()
   *
   *  \param file file to watch for changes
   *  \throws std::system_error if the inotify-file-descriptor couldn't get
   * initialsied
   *  \throws std::runtime_error if the path given does not exist
   */
  FileWatcher(std::filesystem::path file) noexcept(false);
  file_watch_error startWatching();
  file_watch_error startWatching(std::filesystem::path file);

  // blocking
  std::optional<std::string> waitAndGet();

  // not blocking, optional
  std::optional<std::string> getWhenChanged();

  // optional or needed?
  template <class Rep, class Period>
  std::optional<std::string>
  waitForAndGet(std::chrono::duration<Rep, Period> time);

  std::optional<std::string> readFile();

  bool stopWatching();

  virtual ~FileWatcher();

protected:
  std::filesystem::path _file_to_watch;
  std::shared_ptr<std::atomic_bool> _watching;
  std::shared_ptr<std::atomic_bool> _updated;
  std::shared_ptr<std::string> _changed_file_content;
  std::shared_ptr<std::mutex>
      _cv_mut; /*<! mutex for the lock in the condition_variable */
  std::shared_ptr<std::condition_variable>
      _notify_waiter_cv; /*!< for notification of the wait_and_get() - caller */
  int _inotify_fd; /*!< file descriptor for the inotify instance used to detect
                      file changes */
  int _watch_fd;   /*!< file descriptor for the watch instance for
                      _file_to_watch*/
  std::thread _watching_thread;
};

#endif /* end of include guard: FILEWATCHER_H */
