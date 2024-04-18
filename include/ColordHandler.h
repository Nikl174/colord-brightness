#ifndef COLORDHANDLER_H

#define COLORDHANDLER_H

#include <filesystem>
#include <lcms2.h>

/*! \class BrightnessHandler
 *  \brief wrapper for setting the brightness colord  and managing the file
 * descriptor of the icc files
 *
 *  Wrappes the colord communication and used for setting/getting the brightness
 * and halndling the mem_fd for the temporary icc file
 */
class BrightnessHandler {
public:
  BrightnessHandler(std::filesystem::path path_for_icc);
  bool setDefaultProfile();
  bool setIccFromCmsProfile(cmsHPROFILE profile);
  virtual ~BrightnessHandler();

protected:
  int mem_fd; /*!< filedescriptor for the icc file */
};

#endif /* end of include guard: COLORDHANDLER_H */
