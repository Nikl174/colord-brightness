#ifndef COLORDHANDLER_H

#define COLORDHANDLER_H

#include <colord.h>
#include <filesystem>
#include <lcms2.h>
#include <optional>

/*! \class ColordHandler
 *  \brief wrapper for setting the brightness colord  and managing the file
 * descriptor of the icc files
 *
 *  Wrappes the colord communication to the display-device and used for
 * setting/getting the brightness and halndling the mem_fd for the temporary icc
 * file
 */
class ColordHandler {
public:
  /*! \brief Constructor, initialises colord_client and memfd
   *  \param path_for_icc path used for creating the mem_fd used for the icc
   * profiles
   *
   *  \throws std::runtime_error if the colord_server is not running
   *  \throws std::runtime_error::system_error if the fd couldn't get created
   */
  ColordHandler(std::filesystem::path path_for_icc) noexcept(false);
  bool setDefaultProfile(uint display_device_id = 0);
  bool setIccFromCmsProfile(cmsHPROFILE profile, uint display_device_id = 0);
  virtual ~ColordHandler();

protected:
  std::optional<CdDevice> getDisplayDevice(uint dev_num);
  bool checkAndSyncCdClient();
  int mem_fd_; /*!< file descriptor for the icc file */
  std::filesystem::path mem_fd_path_;
  CdClient cd_client_;
  GCancellable cancle_request_; /*!< for future cancellation*/
};

#endif /* end of include guard: COLORDHANDLER_H */
