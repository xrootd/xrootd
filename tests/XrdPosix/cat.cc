#undef NDEBUG

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
  for (int i = 1; i < argc; ++i) {
    int fd = open(argv[i], O_RDONLY);

    if (fd == -1) {
      fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
      exit(EXIT_FAILURE);
    }

    struct stat st;

    if (fstat(fd, &st) == -1) {
      fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
      close(fd);
      exit(EXIT_FAILURE);
    }

    char buff[BUFSIZ];
    ssize_t n = 0, tot = 0;
    while((n = read(fd, buff, sizeof(buff))) > 0) {
      fwrite(buff, sizeof(char), n, stdout);
      tot += n;
    }

    if (tot != st.st_size) {
      fprintf(stderr, "%s: %s: %s\n", argv[0], argv[i], strerror(errno));
      close(fd);
      exit(EXIT_FAILURE);
    }

    close(fd);
  }

  exit(EXIT_SUCCESS);
}
