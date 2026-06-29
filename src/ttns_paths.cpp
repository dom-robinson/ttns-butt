#include "ttns_paths.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <limits.h>
#include <stdlib.h>
#else
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#endif

#include <sys/stat.h>

static int ttns_file_readable(const char *path)
{
    struct stat st;
    return path && path[0] && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int ttns_get_exe_dir(char *buf, size_t buflen)
{
    char tmp[PATH_MAX];

    if (!buf || buflen == 0)
        return -1;

    buf[0] = '\0';

#ifdef _WIN32
    if (GetModuleFileNameA(NULL, tmp, (DWORD)sizeof(tmp)) == 0)
        return -1;
    {
        char *p = strrchr(tmp, '\\');
        if (!p)
            return -1;
        *p = '\0';
    }
#elif defined(__APPLE__)
    uint32_t sz = (uint32_t)sizeof(tmp);
    if (_NSGetExecutablePath(tmp, &sz) != 0)
        return -1;
    {
        char resolved[PATH_MAX];
        if (realpath(tmp, resolved))
            strncpy(tmp, resolved, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = '\0';
    }
    {
        char *p = strrchr(tmp, '/');
        if (!p)
            return -1;
        *p = '\0';
    }
#else
    ssize_t n = readlink("/proc/self/exe", tmp, sizeof(tmp) - 1);
    if (n > 0)
    {
        tmp[n] = '\0';
        char *p = strrchr(tmp, '/');
        if (p)
            *p = '\0';
    }
    else
    {
        if (!getcwd(tmp, sizeof(tmp)))
            return -1;
    }
#endif

    strncpy(buf, tmp, buflen - 1);
    buf[buflen - 1] = '\0';
    return 0;
}

static int ttns_try_paths(const char *const *paths, char *out, size_t outlen)
{
    int i;

    for (i = 0; paths[i]; i++)
    {
        if (ttns_file_readable(paths[i]))
        {
            strncpy(out, paths[i], outlen - 1);
            out[outlen - 1] = '\0';
            return 0;
        }
    }
    return -1;
}

static int ttns_resolve_bundle_file(const char *subdir, const char *filename,
                                    char *out, size_t outlen)
{
    char exe[PATH_MAX];
    const char *candidates[16];
    int n = 0;
    char b1[PATH_MAX], b2[PATH_MAX], b3[PATH_MAX], b4[PATH_MAX];
    char b5[PATH_MAX], b6[PATH_MAX], b7[PATH_MAX], b8[PATH_MAX];

    snprintf(b1, sizeof(b1), "%s/%s", subdir, filename);
    candidates[n++] = b1;

    snprintf(b2, sizeof(b2), "../%s/%s", subdir, filename);
    candidates[n++] = b2;

    snprintf(b3, sizeof(b3), "../../%s/%s", subdir, filename);
    candidates[n++] = b3;

    if (ttns_get_exe_dir(exe, sizeof(exe)) == 0)
    {
        snprintf(b4, sizeof(b4), "%s/%s/%s", exe, subdir, filename);
        candidates[n++] = b4;

        snprintf(b5, sizeof(b5), "%s/../%s/%s", exe, subdir, filename);
        candidates[n++] = b5;

        snprintf(b6, sizeof(b6), "%s/../Resources/%s/%s", exe, subdir, filename);
        candidates[n++] = b6;

        snprintf(b7, sizeof(b7), "%s/../share/ttns-deck/%s/%s", exe, subdir, filename);
        candidates[n++] = b7;

#ifdef _WIN32
        snprintf(b8, sizeof(b8), "%s\\%s\\%s", exe, subdir, filename);
        candidates[n++] = b8;
#endif
    }

    candidates[n] = NULL;
    return ttns_try_paths(candidates, out, outlen);
}

int ttns_path_data_file(const char *filename, char *out, size_t outlen)
{
    return ttns_resolve_bundle_file("data", filename, out, outlen);
}

int ttns_path_asset_file(const char *filename, char *out, size_t outlen)
{
    return ttns_resolve_bundle_file("assets", filename, out, outlen);
}

static void ttns_mkdir_p(char *path)
{
    char *p;

#ifdef _WIN32
    for (p = path + 1; *p; p++)
    {
        if (*p == '\\' || *p == '/')
        {
            char c = *p;
            *p = '\0';
            _mkdir(path);
            *p = c;
        }
    }
    _mkdir(path);
#else
    for (p = path + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = '\0';
            mkdir(path, 0755);
            *p = '/';
        }
    }
    mkdir(path, 0755);
#endif
}

int ttns_default_log_path(char *out, size_t outlen)
{
    const char *home;

    if (!out || outlen == 0)
        return -1;

#ifdef _WIN32
    home = getenv("LOCALAPPDATA");
    if (!home || !home[0])
        home = getenv("USERPROFILE");
    if (!home || !home[0])
        return -1;
    snprintf(out, outlen, "%s\\TTNS Deck\\ttns-deck.log", home);
#elif defined(__APPLE__)
    home = getenv("HOME");
    if (!home || !home[0])
        return -1;
    snprintf(out, outlen, "%s/Library/Logs/TTNS Deck/ttns-deck.log", home);
#else
    home = getenv("HOME");
    if (!home || !home[0])
        return -1;
    snprintf(out, outlen, "%s/.local/state/ttns-deck/ttns-deck.log", home);
#endif

    return 0;
}

int ttns_ensure_log_file_ready(const char *log_path)
{
    char dir[PATH_MAX];
    char *sep;
    FILE *f;

    if (!log_path || !log_path[0])
        return -1;

    strncpy(dir, log_path, sizeof(dir) - 1);
    dir[sizeof(dir) - 1] = '\0';

#ifdef _WIN32
    sep = strrchr(dir, '\\');
    if (!sep)
        sep = strrchr(dir, '/');
#else
    sep = strrchr(dir, '/');
#endif
    if (!sep)
        return -1;

    *sep = '\0';
    ttns_mkdir_p(dir);
    *sep = '/';

    f = fopen(log_path, "ab");
    if (!f)
        return -1;
    fclose(f);
    return 0;
}
