#include "ColordHandler.h"
#include <easylogging++.h>
#include <filesystem>
#include <lcms2.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <sys/mman.h>
#include <system_error>

ColordHandler::ColordHandler(std::filesystem::path path_for_icc)
    : _cancle_request(*g_cancellable_new()), _cd_client(*cd_client_new()) {

  // connect client
  if (cd_client_get_has_server(&_cd_client)) {
    std::unique_ptr<GError *> error;
    if (!cd_client_connect_sync(&_cd_client, &_cancle_request, error.get())) {
      // client not connected, can be fixed
      LOG(WARNING) << "Couldn't connect colord client on init! Gerror: "
                   << (**error).message;
    }
  } else {
    // Colord-Server not running, object would be useless
    throw std::runtime_error("Colord-Server is not running!");
  }

  // open memfd, set close on exit (e.g.: closes fd, if process crashes)
  _mem_fd = memfd_create(path_for_icc.c_str(), MFD_CLOEXEC);
  if (_mem_fd < 0) {
    // file Couldn't get created, object Couldn't write the profile
    throw std::system_error(errno, std::system_category());
  } else {
    std::stringstream sa;
    sa << "/proc/" << getpid() << "/fd/" << _mem_fd;
    _mem_fd_path = std::filesystem::path(sa.str());
  }
}

std::optional<CdDevice> ColordHandler::getDisplayDevice(uint dev_num) {
  std::unique_ptr<GError *> error;
  GPtrArray *devices = cd_client_get_devices_by_kind_sync(
      &_cd_client, CD_DEVICE_KIND_DISPLAY, &_cancle_request, error.get());
  gpointer dev = devices->pdata[dev_num];
  if (dev)
    return *static_cast<CdDevice *>(dev);
  LOG(ERROR) << "No Display device found! Gerror: " << (**error).message;
  return std::nullopt;
}

/*! TODO: better error propagation ??
 *  \todo better error propagation ??
 */
bool ColordHandler::setIccFromCmsProfile(cmsHPROFILE profile,
                                         uint display_device_id) {
  if (!cmsMD5computeID(profile))
    LOG(WARNING) << "Couldn't recompute hash for lcms2 color profile!";

  if (!cmsSaveProfileToFile(profile, _mem_fd_path.c_str())) {
    LOG(ERROR) << "Lcms2-profile Couldn't get saved into mem_fd!";
    return false;
  }

  CdIcc icc_file = *cd_icc_new();
  {
    std::unique_ptr<GError *> error;
    if (!cd_icc_load_fd(&icc_file, _mem_fd, CD_ICC_LOAD_FLAGS_ALL,
                        error.get())) {
      LOG(ERROR) << "CdIcc profile couldn't get loaded from mem_fd! Gerror: "
                 << (**error).message;
      return false;
    }
  }

  return makeProfileFromIccDefault(icc_file, display_device_id);
}

bool ColordHandler::makeProfileFromIccDefault(CdIcc icc_file,
                                              uint display_device_id) {
  CdProfile icc_profile = *cd_profile_new();
  {
    std::unique_ptr<GError *> error;
    CdProfile *tmp_profile = cd_client_create_profile_for_icc_sync(
        &_cd_client, &icc_file, CdObjectScope::CD_OBJECT_SCOPE_TEMP,
        &_cancle_request, error.get());
    if (!tmp_profile) {
      LOG(ERROR) << "CdClient couldn't create a Profile from icc file! Gerror: "
                 << (**error).message;
      return false;
    } else {
      icc_profile = *tmp_profile;
    }
  }

  if (!cd_client_get_connected(&_cd_client)) {
    std::unique_ptr<GError *> error;
    if (!cd_client_connect_sync(&_cd_client, &_cancle_request, error.get())) {
      // client not connected
      LOG(ERROR)
          << "Couldn't connect Colord client on setting a profile! Gerror: "
          << (**error).message;
      return false;
    }
  }

  std::optional<CdDevice> cd_display = getDisplayDevice(display_device_id);
  if (!cd_display.has_value())

    return false;

  {
    std::unique_ptr<GError *> error;
    if (!cd_device_connect_sync(&cd_display.value(), &_cancle_request,
                                error.get())) {
      LOG(ERROR) << "Couldn't connect to CdDevice! Gerror: "
                 << (**error).message;
      return false;
    }
  }

  {
    std::unique_ptr<GError *> error;
    if (!cd_device_add_profile_sync(&cd_display.value(),
                                    CD_DEVICE_RELATION_SOFT, &icc_profile,
                                    &_cancle_request, error.get())) {
      LOG(ERROR) << "Couldn't add Profile to device! Gerror: "
                 << (**error).message;
      return false;
    }
  }

  std::unique_ptr<GError *> error;
  auto set_profile = cd_device_make_profile_default_sync(
      &cd_display.value(), &icc_profile, &_cancle_request, error.get());
  LOG_IF(!set_profile, ERROR)
      << "Couldn't make profile default for device! Gerror: "
      << (**error).message;
  return set_profile;
}

std::stringstream print_color(const CdColorYxy *color) {
  std::stringstream ss;
  if (color != nullptr) {
    ss << " Y: " << color->Y << " x:" << color->x << " y:" << color->y;
  } else {
    ss << "NULLPTR";
  }
  return ss;
}

std::optional<CdIcc>
ColordHandler::createIccFromEdid(std::filesystem::path edid_file_path) {
  CdEdid *monitor = cd_edid_new();
  std::ifstream edid_file("/sys/class/drm/card1-eDP-1/edid",
                          std::ios::in | std::ios::binary);

  char bytes[128];
  edid_file.read(&bytes[0], 128);
  {
    std::unique_ptr<GError *> error;
    gboolean parsed =
        cd_edid_parse(monitor, g_bytes_new(bytes, sizeof(bytes)), error.get());
    if (!parsed) {
      LOG(ERROR) << "Edid couldn't ger parsed! Gerror: " << (**error).message;
      return std::nullopt;
    }
  }
  const CdColorYxy *m_red = cd_edid_get_red(monitor);
  const CdColorYxy *m_blue = cd_edid_get_blue(monitor);
  const CdColorYxy *m_green = cd_edid_get_green(monitor);
  const CdColorYxy *m_white = cd_edid_get_white(monitor);
  gdouble m_gamma = cd_edid_get_gamma(monitor);

  LOG(INFO) << "GAMMA: " << m_gamma;
  LOG(INFO) << "RED: " << print_color(m_red);
  LOG(INFO) << "BLUE: " << print_color(m_blue);
  LOG(INFO) << "GREEN: " << print_color(m_green);
  LOG(INFO) << "WHITE: " << print_color(m_white);

  CdIcc icc = *cd_icc_new();
  {
    std::unique_ptr<GError *> error;
    gboolean created = cd_icc_create_from_edid(&icc, m_gamma, m_red, m_green,
                                               m_blue, m_white, error.get());
    if (!created) {
      LOG(ERROR) << "Couldn't create icc file form edid values! Gerror: "
                 << (**error).message;
    }
  }

  return icc;
}

