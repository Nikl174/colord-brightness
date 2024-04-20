#include "ColordHandler.h"
#include <cassert>
#include <easylogging++.h>
#include <filesystem>
#include <lcms2.h>
#include <memory>
#include <optional>
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
      LOG(WARNING) << "Couldn't connect colord client on init! Gerror: "
                   << (**error).message;
    }
  } else {
    // Colord-Server not running, object would be useless
    throw std::runtime_error("Colord-Server is not running!");
  }

  // open memfd, set close on exit (e.g.: closes fd, if process crashes)
  mem_fd_ = memfd_create(path_for_icc.c_str(), MFD_CLOEXEC);
  if (mem_fd_ < 0) {
    // file Couldn't get created, object Couldn't write the profile
    throw std::system_error(errno, std::system_category());
  } else {
    std::stringstream sa;
    sa << "/proc/" << getpid() << "/fd/" << mem_fd_;
    mem_fd_path_ = std::filesystem::path(sa.str());
  }
}

bool ColordHandler::checkAndSyncCdClient() {
  if (!cd_client_get_connected(&cd_client_)) {
    std::unique_ptr<GError *> error;
    if (!cd_client_connect_sync(&cd_client_, &cancle_request_, error.get())) {
      // client not connected, can be fixed
      LOG(WARNING) << "Couldn't connect Colord client! Gerror: "
                   << (**error).message;
      return false;
    }
  }
  return true;
}

std::optional<CdDevice> ColordHandler::getDisplayDevice(uint dev_num) {
  std::unique_ptr<GError *> error;
  GPtrArray *devices = cd_client_get_devices_by_kind_sync(
      &cd_client_, CD_DEVICE_KIND_DISPLAY, &cancle_request_, error.get());
  gpointer dev = devices->pdata[dev_num];
  if (dev)
    return *static_cast<CdDevice *>(dev);
  return std::nullopt;
}

/*! TODO: better error propagation and logging
 *  \todo better error propagation and logging
 */
bool ColordHandler::setIccFromCmsProfile(cmsHPROFILE profile,
                                         uint display_device_id) {
  if (!cmsSaveProfileToFile(profile, mem_fd_path_.c_str()))
    return false;

  CdIcc icc_file = *cd_icc_new();
  {
    std::unique_ptr<GError *> error;
    if (cd_icc_load_fd(&icc_file, mem_fd_, CD_ICC_LOAD_FLAGS_ALL, error.get()))
      return false;
  }
  CdProfile icc_profile = *cd_profile_new();
  {
    std::unique_ptr<GError *> error;
    CdProfile *tmp_profile = cd_client_create_profile_for_icc_sync(
        &cd_client_, &icc_file, CdObjectScope::CD_OBJECT_SCOPE_TEMP,
        &cancle_request_, error.get());
    if (!tmp_profile) {
      return false;
    } else {
      icc_profile = *tmp_profile;
    }
  }

  if (!checkAndSyncCdClient()) {
    return false;
  }

  std::optional<CdDevice> cd_display = getDisplayDevice(display_device_id);
  if (!cd_display.has_value())
    return false;

  {
    std::unique_ptr<GError *> error;
    if (!cd_device_connect_sync(&cd_display.value(), &cancle_request_,
                                error.get()))
      return false;
  }

  {
    std::unique_ptr<GError *> error;
    if (!cd_device_add_profile_sync(&cd_display.value(),
                                    CD_DEVICE_RELATION_SOFT, &icc_profile,
                                    &cancle_request_, error.get()))
      return false;
  }

  std::unique_ptr<GError *> error;
  return cd_device_make_profile_default_sync(&cd_display.value(), &icc_profile,
                                             &cancle_request_, error.get());
}
