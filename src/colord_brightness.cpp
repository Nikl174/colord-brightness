#include "ColordHandler.h"
#include "FileWatcher.h"
#include <algorithm>
#include <cassert>
#include <cctype>
#include <climits>
#include <cstddef>
#include <easylogging++.h>
#include <exception>
#include <filesystem>
#include <lcms2.h>
#include <memory>
#include <string>
#include <thread>

INITIALIZE_EASYLOGGINGPP
#define ELPP_LOGGING_FLAGS_FROM_ARGS

#define MAX_BRIGHTNESS_FILE "max_brightness"
#define BRIGHTNESS_FILE "brightness"

// based on color profile creation from:
// https://github.com/udifuchs/icc-brightness/blob/master/icc-brightness-gen.c
cmsHPROFILE create_srgb_profile(double brightness) {
  // cmsHPROFILE hsRGB = cmsCreate_sRGBProfile();
  cmsContext ctx = cmsCreateContext(NULL, NULL);
  cmsHPROFILE hsRGB = cmsCreate_sRGBProfileTHR(ctx);

  cmsMLU *mlu = cmsMLUalloc(NULL, 1);
  char description[20];
  snprintf(description, 20, "Brightness %.2f", brightness);
  cmsMLUsetASCII(mlu, "en", "US", description);
  cmsWriteTag(hsRGB, cmsSigProfileDescriptionTag, mlu);
  cmsMLUfree(mlu);

  cmsContext context_id = cmsCreateContext(NULL, NULL);
  double used_brightness = std::clamp(brightness, 0.1, 1.0);
  double curve[] = {1.0, used_brightness,
                    0.0}; // gamma, a, b for (a X +b)^gamma
  cmsToneCurve *tone_curve[3] = {
      cmsBuildParametricToneCurve(context_id, 2, curve),
      cmsBuildParametricToneCurve(context_id, 2, curve),
      cmsBuildParametricToneCurve(context_id, 2, curve),
  };
  cmsWriteTag(hsRGB, cmsSigVcgtTag, tone_curve);
  cmsFreeToneCurve(tone_curve[0]);
  cmsFreeToneCurve(tone_curve[1]);
  cmsFreeToneCurve(tone_curve[2]);

  return hsRGB;
}

/*! TODO: maybe remove assertions?
 *  \todo maybe remove assertions?
 */
void startWatchAndApplyBrightness(std::shared_ptr<ColordHandler> cd_handle,
                                  std::shared_ptr<FileWatcher> fw,
                                  uint max_abs_brightness) {
  LOG_IF(!fw, FATAL)
      << "FileWatcher coudn't get constructed but no exception was thrown!?!";
  LOG_IF(!cd_handle, FATAL)
      << "ColordHandler coudn't get constructed but no exception was thrown!?!";
  assert(fw);
  assert(cd_handle);

  file_watch_error started = fw->startWatching();
  if (started == file_watch_error::success) {
  } else {
    LOG(ERROR) << "Error occured on starting the filewatcher! ERROR:";
    switch (started) {
    case file_watch_error::error_unknown:
      LOG(ERROR) << "Unknown Error";
      break;
    case file_watch_error::error_still_watching:
      LOG(ERROR) << "Already watching";
      break;
    default:
      LOG(ERROR) << "Errno: " << strerror((int)started);
    }
    return;
  }
  assert(fw->startWatching() == file_watch_error::error_still_watching);
  // fw started

  while (std::optional<std::string> new_brightness = fw->waitAndGet()) {
    if (new_brightness.has_value()) {
      std::string brightness_str = new_brightness.value();
      int end = brightness_str.length() - 1;
      if (!brightness_str.empty() && brightness_str[end] == '\n') {
        brightness_str.erase(end);
      }

      // check, if value of file is integer
      for (auto c : brightness_str) {
        if (!std::isdigit(c)) {
          LOG(WARNING) << "Content of file to watch is no a number! "
                       << new_brightness.value() << " at char: " << c;
          continue;
        }
      }
      // calculate profile and set it do display
      double brightness = std::stod(brightness_str);
      cmsHPROFILE new_profile =
          create_srgb_profile(brightness / max_abs_brightness);
      LOG_IF(!cd_handle->setIccFromCmsProfile(new_profile), WARNING)
          << "Icc Profile not updated!";
    } else {
      LOG(WARNING) << "Error retreiving brightness value from filewatcher!";
    }
  }
}

int main(int argc, char *argv[]) {
  START_EASYLOGGINGPP(argc, argv);
  el::Configurations default_conf;
  default_conf.setToDefault();
  default_conf.set(el::Level::Warning, el::ConfigurationType::Enabled, "false");
  default_conf.setGlobally(el::ConfigurationType::Format,
                           "%datetime %level %msg");
  el::Loggers::reconfigureAllLoggers(default_conf);

  struct ColordBrightnessConfig {
    std::filesystem::path icc_file;
    std::filesystem::path brightness_driver_dir;
  } conf = {"colord_brightness_profile.icc",
            "/sys/class/backlight/intel_backlight/"};

  std::shared_ptr<ColordHandler> cd_handle;
  std::shared_ptr<FileWatcher> fw;
  /*! TODO: improve error handling
   *  \todo improve error handling
   */
  assert(!conf.brightness_driver_dir.has_filename());
  assert(conf.icc_file.has_filename());
  try {
    fw = std::make_shared<FileWatcher>(conf.brightness_driver_dir /
                                       BRIGHTNESS_FILE);
  } catch (std::exception &e) {
    LOG(ERROR) << "Exception in creation of FileWatcher! Exception:"
               << e.what();
    return -1;
  }
  try {
    cd_handle = std::make_shared<ColordHandler>(conf.icc_file);
  } catch (std::exception &e) {
    LOG(ERROR) << "Exception in creation of ColordHandler! Exception:"
               << e.what();
    return -1;
  }
  std::ifstream max_brightness_file(conf.brightness_driver_dir /
                                    MAX_BRIGHTNESS_FILE);
  if (!max_brightness_file.is_open()) {
    LOG(ERROR) << "Couldn't open file with max brightness! Path: "
               << (conf.brightness_driver_dir / MAX_BRIGHTNESS_FILE);
    return -1;
  }
  char max_brightness[sizeof(UINT_MAX)];
  max_brightness_file.read(max_brightness, sizeof(max_brightness));

  startWatchAndApplyBrightness(cd_handle, fw, std::stod(max_brightness));
  // std::thread t(&startWatchAndApplyBrightness, cd_handle, fw,
  //               std::stod(max_brightness));
  // t.join();
}
