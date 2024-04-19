#include "ColordHandler.h"
#include <easylogging++.h>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <sys/mman.h>
#include <system_error>

ColordHandler::ColordHandler(std::filesystem::path path_for_icc)
    : cancle_request_(*g_cancellable_new()), cd_client_(*cd_client_new()) {

  // connect client
  if (cd_client_get_has_server(&cd_client_)) {
    std::unique_ptr<GError *> error;
    if (!cd_client_connect_sync(&cd_client_, &cancle_request_, error.get())) {
      // client not connected, can be fixed
      LOG(WARNING) << "Couldn't connect to Colord-Server on init!";
    }
  } else {
    // Colord-Server not running, object would be useless
    throw std::runtime_error("Colord-Server is not running!");
  }

  // open memfd, set close on exit (e.g.: closes fd, if process crashes)
  int fd_or_failed = memfd_create(path_for_icc.c_str(), MFD_CLOEXEC);
  if (fd_or_failed < 0) {
    // file Couldn't get created, object Couldn't write the profile
    throw std::system_error(errno, std::system_category());
    ;
  } else {
    mem_fd_ = fd_or_failed;
  }
}
