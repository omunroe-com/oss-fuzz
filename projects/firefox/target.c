#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STRINGLIT(S) #S
#define STRINGIFY(S) STRINGLIT(S)

// Required for oss-fuzz to consider the binary a target.
static const char* magic __attribute__((used)) = "LLVMFuzzerTestOneInput";

int main(int argc, char* argv[]) {
  char path[PATH_MAX] = {0};

  if (**argv != '/') {
    if (!getcwd(path, PATH_MAX)) {
      perror("Couldn't get CWD");
      exit(1);
    }
    strcat(path, "/");
  }

  if (strlen(path) + strlen(*argv) + 20 > PATH_MAX) {
    fprintf(stderr, "Path length would exceed PATH_MAX\n");
    exit(1);
  }

  strcat(path, *argv);

  char* solidus = strrchr(path, '/');
  *solidus = 0; // terminate string before last /

  char ld_path[PATH_MAX] = {0};
  strcpy(ld_path, path);
  strcat(ld_path, "/lib");

  char ff_path[PATH_MAX] = {0};
  strcpy(ff_path, path);
  strcat(ff_path, "/firefox/firefox");

  // Expects LD_LIBRARY_PATH to not also be set by oss-fuzz.
  // If it ever is, this has to be replaced with more complex code.
  if (setenv("LD_LIBRARY_PATH", ld_path, 0)) {
    perror("Error setting LD_LIBRARY_PATH");
    exit(1);
  }

  if (setenv("MOZ_RUN_GTEST", "1", 1) || setenv("LIBFUZZER", "1", 1) ||
      setenv("FUZZER", STRINGIFY(FUZZ_TARGET), 1)) {
    perror("Error setting fuzzing variables");
    exit(1);
  }
  
  if (setenv("HOME", "/tmp", 0)) {
    perror("Error setting HOME");
    exit(1);
  }

  char* options = getenv("ASAN_OPTIONS");
  if (!options) {
    fprintf(stderr, "ASAN_OPTIONS not set ?!\n");
    exit(1);
  }

  // Temporary (or permanent?) work-arounds for fuzzing interface bugs.
  char* new_options = strdup(options);
  char* ptr;
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1477846
  ptr = strstr(new_options, "detect_stack_use_after_return=1");
  if (ptr) ptr[30] = '0';
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1477844
  ptr = strstr(new_options, "detect_leaks=1");
  if (ptr) ptr[13] = '0';

  if (setenv("ASAN_OPTIONS", new_options, 1)) {
    perror("Error setting ASAN_OPTIONS");
    exit(1);
  }
  free(new_options);

  int ret = execv(ff_path, argv);
  if (ret)
    perror("execv");
  return ret;
}
