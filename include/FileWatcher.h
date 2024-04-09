#ifndef FILEWATCHER_H

#define FILEWATCHER_H
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <optional>
#include <string>
#include <sys/inotify.h>
#include <thread>

/*! \class FileWatcher
 *  \brief Asynchronous watcher for file changes
 *
 *  Watches a file for changes and returnes the content if changed.
 *  Uses inotify-events provided by the linux kernel.
 */
class FileWatcher {
public:
  FileWatcher() noexcept(false);
  int start_watching(std::filesystem::path file);
  std::string wait_and_get();

  template <class Rep, class Period>
  std::optional<std::string>
  wait_for_and_get(std::chrono::duration<Rep, Period> time);
  virtual ~FileWatcher();

protected:
  std::filesystem::path _file_to_watch;
  std::shared_ptr<std::condition_variable>
      _notify_waiter; /*!< for notification of the wait_and_get() - caller */
  int _inotify_fd; /*!< file descriptor for the inotify instance used to detect
                      file changes */
  std::thread _watching_thread;
};

#endif /* end of include guard: FILEWATCHER_H */
