#include <dirent.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

uint8_t buf[4096];

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "usage: %s file0 <file1> <file2> <file...>\n", argv[0]);
    return 1;
  }
  DIR *dir = opendir(".");
  while (1) {
    struct dirent *ent = readdir(dir);
    if (!ent) {
      break;
    }
    fprintf(stderr, "debug: readdir: %s\n", ent->d_name);
  }
  closedir(dir);
  for (int i = 1; i < argc; i++) {
    fprintf(stderr, "debug: processing %s\n", argv[i]);
    FILE *file = fopen(argv[i], "rb");
    if (!file) {
      fprintf(stderr, "%s: %s: error with fopen\n", argv[0], argv[i]);
      return 1;
    }
    while (1) {
      const size_t bytes_read = fread(buf, 1, sizeof(buf), file);
      if (bytes_read == 0) {
        break;
      }
      if (fwrite(buf, 1, bytes_read, stdout) != bytes_read) {
        fprintf(stderr, "%s: %s: error with fwrite\n", argv[0], argv[i]);
        return 1;
      }
    }
    fclose(file);
  }
  return 0;
}
