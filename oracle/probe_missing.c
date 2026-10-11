/* Diagnostic probe: which of the SDK's 877 API names does the host's
 * GetFunc() return NULL for? Not a plugin — never registers.
 *
 * Build:  g++ -shared -fPIC -Ivendor/reaper-sdk/sdk oracle/probe_missing.c \
 *         -o build/oracle/probe_missing.so
 * Run:    drop into REAPER's UserPlugins, set ORACLE_PROBE_OUT and
 *         ORACLE_SDK_NAMES (default: <cwd>/oracle/sdk_api_names.txt).
 * Output: one missing name per line in $ORACLE_PROBE_OUT, plus
 *         "PROBE total=<n> missing=<m>" as the last line.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "reaper_plugin.h"

// extern "C": REAPER dlsym()s the unmangled name "ReaperPluginEntry";
// a C++-mangled export is silently ignored at plugin load.
extern "C" int ReaperPluginEntry(const char *name, const char *opt, int res, const void *rec)
{
  const reaper_plugin_info_t *info = (const reaper_plugin_info_t *)rec;
  if (!info || !info->GetFunc)
    return 0;

  const char *names_path = getenv("ORACLE_SDK_NAMES");
  if (!names_path || !*names_path)
    names_path = "oracle/sdk_api_names.txt";
  const char *out_path = getenv("ORACLE_PROBE_OUT");
  if (!out_path || !*out_path)
    out_path = "probe_missing.txt";

  FILE *names = fopen(names_path, "r");
  if (!names)
    return 0;
  FILE *out = fopen(out_path, "w");
  if (!out) {
    fclose(names);
    return 0;
  }

  long total = 0, missing = 0;
  char buf[256];
  while (fgets(buf, sizeof buf, names)) {
    char *s = buf;
    while (isspace((unsigned char)*s))
      s++;
    if (!*s)
      continue;
    char *nl = strchr(s, '\n');
    if (nl)
      *nl = 0;
    if (*s == '#')
      continue;
    total++;
    if (info->GetFunc(s) == NULL) {
      missing++;
      fprintf(out, "%s\n", s);
    }
  }
  fclose(names);
  fprintf(out, "PROBE total=%ld missing=%ld\n", total, missing);
  fclose(out);
  return 0; /* diagnostic only: do not register */
}
