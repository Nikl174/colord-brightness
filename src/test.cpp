#include <cerrno>
#include <chrono>
#include <colord.h>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <ios>
#include <iostream>
#include <lcms2.h>
#include <linux/limits.h>
#include <sstream>
#include <stdio.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

#define CHECK_NULL(pointer) ((pointer == NULL) ? "NO " : "YES ")

void print_hex(char bytes[128]) {
  for (ushort i = 0; i < 128; i++) {
    std::cout << std::hex << (int)bytes[i] << " ";
    if ((i + 1) % 16 == 0) {
      std::cout << std::endl;
    }
  }
}
void print_color(const CdColorYxy *color) {
  if (color != nullptr) {
    std::cout << " Y: " << color->Y << " x:" << color->x << " y:" << color->y
              << std::endl;
  } else {
    std::cout << "NULLPTR" << std::endl;
  }
}
void reset_and_print_gerror(GError *error) {
  if (error != NULL) {
    std::cout << "Current error: " << error->message << std::endl;
    g_error_free(error);
    error = NULL;
    exit(-1);
  }
}

CdColorYxy &operator*(CdColorYxy &color, gdouble gamma) {
  color.Y *= gamma;
  color.y *= gamma;
  color.x *= gamma;
  return color;
}

// copied from
// https://github.com/udifuchs/icc-brightness/blob/master/icc-brightness-gen.c
cmsHPROFILE create_srgb_profile(double brightness) {
  cmsHPROFILE hsRGB = cmsCreate_sRGBProfile();

  cmsMLU *mlu = cmsMLUalloc(NULL, 1);
  char description[20];
  snprintf(description, 20, "Brightness %.2f", brightness);
  cmsMLUsetASCII(mlu, "en", "US", description);
  cmsWriteTag(hsRGB, cmsSigProfileDescriptionTag, mlu);
  cmsMLUfree(mlu);

  cmsContext context_id = cmsCreateContext(NULL, NULL);
  double curve[] = {1.0, brightness, 0.0}; // gamma, a, b for (a X +b)^gamma
  cmsToneCurve *tone_curve[3] = {
      cmsBuildParametricToneCurve(context_id, 2, curve),
      cmsBuildParametricToneCurve(context_id, 2, curve),
      cmsBuildParametricToneCurve(context_id, 2, curve),
  };
  cmsWriteTag(hsRGB, cmsSigVcgtTag, tone_curve);
  cmsFreeToneCurve(tone_curve[0]);
  cmsFreeToneCurve(tone_curve[1]);
  cmsFreeToneCurve(tone_curve[2]);

  cmsMD5computeID(hsRGB);

  return hsRGB;
}

CdIcc *create_icc_from_edid(double brightness) {
  CdEdid *monitor = cd_edid_new();
  GError *error = NULL;
  std::ifstream edid_file("/sys/class/drm/card1-eDP-1/edid",
                          std::ios::in | std::ios::binary);

  char bytes[128];
  edid_file.read(&bytes[0], 128);
  print_hex(bytes);
  std::cout << "EDID Parse:"
            << cd_edid_parse(monitor, g_bytes_new(bytes, sizeof(bytes)), &error)
            << std::endl;
  const CdColorYxy *m_red = cd_edid_get_red(monitor);
  const CdColorYxy *m_blue = cd_edid_get_blue(monitor);
  const CdColorYxy *m_green = cd_edid_get_green(monitor);
  const CdColorYxy *m_white = cd_edid_get_white(monitor);
  gdouble m_gamma = brightness;

  std::cout << "GAMMA " << m_gamma << std::endl;
  const CdColorYxy red = {m_gamma * 10, m_red->x, m_red->y};
  const CdColorYxy blue = {m_gamma * 10, m_blue->x, m_blue->y};
  const CdColorYxy green = {m_gamma * 10, m_green->x, m_green->y};
  const CdColorYxy white = {m_gamma * 10, m_white->x * m_gamma,
                            m_white->y * m_gamma};
  std::cout << "RED";
  print_color(&red);
  std::cout << "BLUE";
  print_color(&blue);
  std::cout << "GREEN";
  print_color(&green);
  std::cout << "WHITE";
  print_color(&white);

  CdIcc *test_icc = cd_icc_new();
  std::cout << "Create ICC:"
            << cd_icc_create_from_edid(test_icc, m_gamma, &red, &green, &blue,
                                       &white, &error)
            << std::endl;

  reset_and_print_gerror(error);
  GFile *icc_file = g_file_new_for_path("/home/nikl/test.icc");
  bool save = cd_icc_save_file(test_icc, icc_file, CD_ICC_SAVE_FLAGS_NONE, NULL,
                               &error);
  reset_and_print_gerror(error);
  cd_icc_set_filename(test_icc, g_file_get_path(icc_file));
  std::cout << cd_icc_to_string(test_icc) << std::endl;
  const gchar *file_name = cd_icc_get_filename(test_icc);
  std::cout << "Save file: " << save << "; " << file_name << std::endl;
  return test_icc;
}

int main(int argc, char *argv[]) {
  GError *error = NULL;

  CdClient *cd_client = cd_client_new();
  std::cout << "Client connected: "
            << cd_client_connect_sync(cd_client, NULL, NULL) << std::endl;

  /* gboolean created = cd_icc_create_default(test_icc, &error); */
  /* reset_and_print_gerror(error); */
  /* std::cout << "Created default icc: " << created << std::endl; */
  double brightness = 1;
  if (argc > 1) {
    brightness = atof(argv[1]);
  }
  /* CdIcc *test_icc = create_icc_from_edid(brightness); */
  CdIcc *test_icc = cd_icc_new();
  cmsHPROFILE new_profile = create_srgb_profile(brightness);
  std::cout << "Created profile with brightness " << brightness << std::endl;
  if (false) {
    // cms profile in memory, not working(segfault)...
    cmsUInt32Number bytesNeeded;
    cmsBool got_bytes_needed =
        cmsSaveProfileToMem(new_profile, NULL, &bytesNeeded);
    std::cout << "CsmProfile to mem got needed bytes: " << got_bytes_needed
              << ", bytes needed: " << bytesNeeded << std::endl;
    uint8_t profile_data[2 * bytesNeeded];
    cmsBool saved_cms_to_mem =
        cmsSaveProfileToMem(new_profile, &profile_data, &bytesNeeded);
    std::cout << "CsmProfile to mem: " << saved_cms_to_mem
              << ", address: " << &profile_data << std::endl;
    for (uint8_t i : profile_data) {
      std::cout << std::hex << i;
    }
    std::cout << std::endl;
    gboolean loaded_profile =
        cd_icc_load_data(test_icc, (const uint8_t *)&profile_data, bytesNeeded,
                         CD_ICC_LOAD_FLAGS_ALL, &error);
    std::cout << "Loaded ICC in Profile: " << loaded_profile << std::endl;
    reset_and_print_gerror(error);
  }

  if (false) {
    // using a pipe, doesn't work either
    int success = mkfifo("/tmp/icc_color_profile", S_IRUSR | S_IWUSR);
    std::cout << "Created fifo: " << success
              << ", errno: " << (success < 0 ? strerror(errno) : "")
              << std::endl;

    std::thread f_save_icc([new_profile] {
      cmsBool write_to_fifo =
          cmsSaveProfileToFile(new_profile, "/tmp/icc_color_profile");
      std::cout << "CsmProfile to fifo: " << write_to_fifo << std::endl;
    });
    GFile *pipe = g_file_new_for_path("/tmp/icc_color_profile");
    // should block, until someone writes
    gboolean loaded_profile =
        cd_icc_load_file(test_icc, pipe, CD_ICC_LOAD_FLAGS_ALL, NULL, &error);
    f_save_icc.join();
    std::cout << "Loaded ICC in Profile: " << loaded_profile << std::endl;
  }
  std::stringstream sa;

  if (true) {
    int success_fd = memfd_create("/tmp/icc_color_profile.icc", 0);
    std::cout << "Created mem fd: " << success_fd
              << ", errno: " << (success_fd < 0 ? strerror(errno) : "")
              << std::endl;
    char filepath[PATH_MAX];
    sa << "/proc/"<<getpid()<<"/fd/"<<success_fd;
    std::cout << "File Desc: " << sa.str() << std::endl;
    cmsBool write_to_fifo = cmsSaveProfileToFile(new_profile, sa.str().c_str());
    std::cout << "CsmProfile to mem fd: " << write_to_fifo << std::endl;
    /* GFile *pipe = g_file_new_for_path("/tmp/icc_color_profile.icc"); */
    gboolean loaded_profile =
        cd_icc_load_fd(test_icc, success_fd, CD_ICC_LOAD_FLAGS_ALL, &error);
    std::cout << "Loaded ICC in Profile: " << loaded_profile << std::endl;
    cd_icc_set_filename(test_icc, sa.str().c_str());
  }
  std::stringstream sc;
  sc << sa.str() << brightness;
  size_t hash = std::hash<std::string>{}(sc.str());
  cd_icc_add_metadata(test_icc, "ProfileId", std::to_string(hash).c_str());

  std::cout << cd_icc_to_string(test_icc) << std::endl;

  CdProfile *profile = cd_client_create_profile_for_icc_sync(
      cd_client, test_icc, CdObjectScope::CD_OBJECT_SCOPE_TEMP, NULL, &error);
  std::cout << "Create Profile from icc: " << CHECK_NULL(profile) << std::endl;
  reset_and_print_gerror(error);

  auto connected_prof = cd_profile_connect_sync(profile, NULL, &error);
  reset_and_print_gerror(error);
  std::cout << "Connected Profile: " << connected_prof << std::endl;
  std::cout << "Profile Title: " << cd_profile_get_title(profile) << std::endl;
  reset_and_print_gerror(error);
  std::cout << "Hash" << hash << std::endl;
  cd_profile_set_property_sync(profile, "ProfileId",
                               std::to_string(hash).c_str(), NULL, &error);
  std::stringstream ss;
  ss << "/tmp/" << cd_profile_get_id(profile) << "_icc_" << brightness;
  cd_profile_set_object_path(profile, ss.str().c_str());
  std::cout << "Profile String: \n"
            << cd_profile_to_string(profile) << std::endl;
  GPtrArray *devices = cd_client_get_devices_by_kind_sync(
      cd_client, CD_DEVICE_KIND_DISPLAY, NULL, &error);
  reset_and_print_gerror(error);

  /* gboolean installed = */
  /*     cd_profile_install_system_wide_sync(profile, NULL, &error); */
  /* reset_and_print_gerror(error); */
  /* std::cout << "Installed Profile Systemwide:" << installed << std::endl; */
  CdDevice *device = NULL;
  if (devices->pdata[0]) {
    device = static_cast<CdDevice *>(devices->pdata[0]);
  }
  std::cout << "Found " << devices->len << " Devices: " << CHECK_NULL(device)
            << std::endl;
  if (device) {
    gchar *id = cd_device_to_string(device);
    std::cout << "DeviceString: \n" << id;
  };

  gboolean connected = cd_device_connect_sync(device, NULL, &error);
  reset_and_print_gerror(error);
  std::cout << "Connected device: " << connected << std::endl;

  gboolean added_profile = cd_device_add_profile_sync(
      device, CD_DEVICE_RELATION_SOFT, profile, NULL, &error);
  if (error != NULL) {
    std::cout << "Error num:" << error->code << std::endl;
    // profile already added
  }
  reset_and_print_gerror(error);
  std::cout << "Add Profile to device: " << added_profile << std::endl;

  gboolean set_profile =
      cd_device_make_profile_default_sync(device, profile, NULL, &error);
  reset_and_print_gerror(error);
  std::cout << "Make Profile default: " << set_profile << std::endl;
  std::this_thread::sleep_for(std::chrono::seconds(5));
}
