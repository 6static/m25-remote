/*
 * version.h - Firmware version string
 *
 * FW_VERSION is bumped manually on release. FW_GIT_HASH is injected at
 * build time by version.py (short git commit hash, "-dirty" if the
 * working tree has uncommitted changes). Falls back to "unknown" for
 * builds that skip the PlatformIO pre-build scripts (e.g. Arduino IDE).
 */

#ifndef VERSION_H
#define VERSION_H

#define FW_VERSION "v0.2.0"

#ifndef FW_GIT_HASH
#define FW_GIT_HASH "unknown"
#endif

#define FW_VERSION_STRING FW_VERSION "+" FW_GIT_HASH

#endif
