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
  // bool setDefaultProfile(std::filesystem::path edid_file_path, uint
  // display_device_id = 0);
  bool setIccFromCmsProfile(cmsHPROFILE profile, uint display_device_id = 0);
  bool cancelCurrentAction();
  virtual ~ColordHandler();

protected:
  std::optional<CdDevice *> getDisplayDevice(uint dev_num);
  bool makeProfileFromIccDefault(CdIcc *icc_file, uint display_device_id);
  std::optional<CdIcc> createIccFromEdid(std::filesystem::path edid_file_path);
  bool resetMemFd();

  int _mem_fd; /*!< file descriptor for the icc file */
  std::filesystem::path _mem_fd_path;
  std::filesystem::path _icc_path;
  std::shared_ptr<CdClient> _cd_client;
  std::shared_ptr<GCancellable>
      _cancel_request; /*!< for future cancellation, currently unused*/
};

#endif /* end of include guard: COLORDHANDLER_H */
