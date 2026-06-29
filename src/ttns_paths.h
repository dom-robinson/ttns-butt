#ifndef TTNS_PATHS_H
#define TTNS_PATHS_H

#include <stddef.h>

/* Directory containing the running executable (no trailing slash). */
int ttns_get_exe_dir(char *buf, size_t buflen);

/* Resolve data/ttns-zones.json etc. Returns 0 if file exists. */
int ttns_path_data_file(const char *filename, char *out, size_t outlen);
int ttns_path_asset_file(const char *filename, char *out, size_t outlen);

#endif
