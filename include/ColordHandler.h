#ifndef COLORDHANDLER_H

#define COLORDHANDLER_H

#include <colord.h>
#include <filesystem>
#include <lcms2.h>

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
  bool setDefaultProfile();
  bool setIccFromCmsProfile(cmsHPROFILE profile);
  virtual ~ColordHandler();

protected:
  CdDevice getDisplayDevice();
  int mem_fd_; /*!< filedescriptor for the icc file */
  CdClient cd_client_;
  GCancellable cancle_request_;
};

#endif /* end of include guard: COLORDHANDLER_H */
