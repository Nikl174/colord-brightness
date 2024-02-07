#include <colord.h>
#include <stdio.h>

int main(int argc, char *argv[]) { 
  CdIcc *test = cd_icc_new(); 
  cd_icc_create_from_edid(test, 0.5, const CdColorYxy *red, const CdColorYxy *green, const CdColorYxy *blue, const CdColorYxy *white, GError **error)
}
