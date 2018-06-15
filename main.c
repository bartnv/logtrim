#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#define BUFSIZE 4096
#define USAGE "Usage: %s [-i] [-o] [-w secs] [-e cmd] timestamp filename\n"

int testpattern(char *pattern, char *line) {
  int i;
  for (i = 0; pattern[i]; i++) {
    if (!line[i]) return 0;
    switch (pattern[i]) {
      case 'A':
        if (!isalpha(line[i])) return 0;
        break;
      case '1':
        if (!isdigit(line[i])) return 0;
        break;
      case ':':
        if (!ispunct(line[i])) return 0;
        break;
      default:
        if (line[i] != pattern[i]) return 0;
    }
  }
  return 1;
}

int main(int argc, char **argv) {
  int r, i, len, waitsecs = 0, found = 0, skipped = 0, copied = 0, checkpattern = 0, output = 0, status;
  char *timestamp, *filename, *tmpname, buf[BUFSIZE], *execcmd = NULL, *execargs[4], *pattern;
  struct stat st;
  FILE *ifp, *ofp;

  while ((r = getopt(argc, argv, "w:e:io")) != -1) {
    switch (r) {
      case 'w':
        waitsecs = atoi(optarg);
        if (!waitsecs) {
          fprintf(stderr, USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        break;
      case 'e':
        execcmd = optarg;
        if (!execcmd) {
          fprintf(stderr, USAGE, argv[0]);
          exit(EXIT_FAILURE);
        }
        break;
      case 'i':
        checkpattern = 1;
        break;
      case 'o':
        output = 1;
        break;
      default:
        fprintf(stderr, USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }
  }

  if (optind+1 >= argc) {
    fprintf(stderr, USAGE, argv[0]);
    exit(EXIT_FAILURE);
  }
  timestamp = argv[optind];
  filename = argv[optind+1];

  if (checkpattern) {
    pattern = (char *)malloc(strlen(timestamp+1));
    for (i = 0; timestamp[i]; i++) {
      if (isalpha(timestamp[i])) pattern[i] = 'A';
      else if (isdigit(timestamp[i])) pattern[i] = '1';
      else if (ispunct(timestamp[i])) pattern[i] = ':';
      else pattern[i] = timestamp[i];
    }
    pattern[i] = '\0';
  }

  if (!(ifp = fopen(filename, "re"))) { // Open mode 'e' requests O_CLOEXEC
    fprintf(stderr, "Error opening original logfile");
    exit(-1);
  }

  tmpname = (char *)malloc(strlen(filename+5));
  strcpy(tmpname, filename);
  strcat(tmpname, ".tmp");
  if (!(ofp = fopen(tmpname, "we"))) {
    fprintf(stderr, "Error opening new logfile");
    exit(-1);
  }

  if (fstat(fileno(ifp), &st)) {
    fprintf(stderr, "Error stat()'ing original logfile");
    exit(-1);
  }
  if (fchmod(fileno(ofp), st.st_mode)) {
    fprintf(stderr, "Error setting permissions on new logfile");
    exit(-1);
  }
  if (fchown(fileno(ofp), st.st_uid, st.st_gid)) {
    fprintf(stderr, "Error setting ownership on new logfile");
    exit(-1);
  }

  len = strlen(timestamp);
  while (fgets(buf, BUFSIZE, ifp)) {
    if (!buf[0] || (buf[0] == '\n')) continue;
    if (!found && ((checkpattern && !testpattern(pattern, buf)) || (strncmp(buf, timestamp, len) < 0))) {
      skipped++;
      if (output) puts(buf);
    }
    else {
      found = 1;
      fputs(buf, ofp);
      copied++;
    }
  }
  fflush(ofp);

  if (!output) printf("Skipped %d lines, copied %d\n", skipped, copied);
  if (rename(tmpname, filename)) {
    fprintf(stderr, "Failed to rename %s to %s\n", tmpname, filename);
    exit(EXIT_FAILURE);
  }

  if (execcmd) {
    switch ((i = fork())) {
      case -1:
        fprintf(stderr, "Failed to fork to run -e command\n");
        exit(EXIT_FAILURE);
      case 0: /* CHILD */
        execargs[0] = "/bin/sh";
        execargs[1] = "-c";
        execargs[2] = execcmd;
        execargs[3] = NULL;
        execve(execargs[0], execargs, NULL);
        fprintf(stderr, "Failed to execute -e command\n");
        exit(EXIT_FAILURE);
      default: /* PARENT */
        if (!output) printf("Subcommand launched\n");
    }
  }

  for (i = 1; i <= waitsecs; i++) {
    if (waitpid(-1, &status, WNOHANG) > 0) {
      if (WIFEXITED(status) && !WEXITSTATUS(status)) {
        if (!output) printf("Subcommand completed succesfully\n");
      }
      else fprintf(stderr, "Subcommand failed\n");
    }
    sleep(1);
    copied = 0;
    while (fgets(buf, BUFSIZE, ifp)) {
      if (!buf[0] || (buf[0] == '\n')) continue;
      fseek(ofp, 0, SEEK_END);
      fputs(buf, ofp);
      copied++;
    }
    if (copied) {
      fflush(ofp);
      if (!output) printf("Copied an additional %d lines written %d seconds after file rename\n", copied, i);
    }
  }

  return EXIT_SUCCESS;
}
