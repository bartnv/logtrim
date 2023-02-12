#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#define BUFSIZE 4096
#define USAGE "Usage: %s [-i] [-o] [-w secs] [-e cmd] timestamp filename...\n"

typedef struct {
  char *name;
  FILE *ifp;
  FILE *ofp;
} input;

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
  int r, i, w, len, waitsecs = 0, found, skipped, copied, checkpattern = 0, output = 0, status;
  char *timestamp, *tmpname, buf[BUFSIZE], *execcmd = NULL, *execargs[4], *pattern, *suffix = NULL, *savename;
  struct stat st;

  while ((r = getopt(argc, argv, "w:e:s:io")) != -1) {
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
      case 's':
        suffix = optarg;
        if (!suffix) {
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

  input *inputs[argc-optind+1];
  for (i = 0; optind+i+1 < argc; i++) {
    inputs[i] = (input *)malloc(sizeof(input));
    inputs[i]->name = argv[optind+i+1];
  }
  inputs[i] = NULL;

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

  for (i = 0; inputs[i]; i++) {
    found = skipped = copied = 0;
    input *input = inputs[i];
    char *filename = input->name;
    FILE *sfp;

    if (!(input->ifp = fopen(filename, "re"))) { // Open mode 'e' requests O_CLOEXEC
      fprintf(stderr, "Error opening original file %s\n", filename);
      exit(EXIT_FAILURE);
    }

    tmpname = (char *)malloc(strlen(filename+5));
    strcpy(tmpname, filename);
    strcat(tmpname, ".tmp");
    if (!(input->ofp = fopen(tmpname, "we"))) {
      fprintf(stderr, "Error opening temporary file %s\n", tmpname);
      exit(EXIT_FAILURE);
    }

    if (fstat(fileno(input->ifp), &st)) {
      fprintf(stderr, "Error stat()'ing original file %s\n", filename);
      exit(EXIT_FAILURE);
    }
    if (fchmod(fileno(input->ofp), st.st_mode)) {
      fprintf(stderr, "Error setting permissions on new file %s\n", tmpname);
      exit(EXIT_FAILURE);
    }
    if (fchown(fileno(input->ofp), st.st_uid, st.st_gid)) {
      fprintf(stderr, "Error setting ownership on new file %s\n", tmpname);
      exit(EXIT_FAILURE);
    }

    if (suffix) {
      savename = (char *)malloc(strlen(filename)+strlen(suffix)+1);
      strcpy(savename, filename);
      strcat(savename, suffix);
      if (!(sfp = fopen(savename, "ae"))) {
        fprintf(stderr, "Error opening save file %s\n", savename);
        exit(EXIT_FAILURE);
      }
    }

    len = strlen(timestamp);
    while (fgets(buf, BUFSIZE, input->ifp)) {
      if (!buf[0] || (buf[0] == '\n')) continue;
      if (!found && ((checkpattern && !testpattern(pattern, buf)) || (strncmp(buf, timestamp, len) < 0))) {
        skipped++;
        if (suffix) fputs(buf, sfp);
        else if (output) puts(buf);
      }
      else {
        found = 1;
        fputs(buf, input->ofp);
        copied++;
      }
    }
    if (suffix) fclose(sfp);
    fflush(input->ofp);

    if (rename(tmpname, filename)) {
      fprintf(stderr, "Failed to rename %s to %s\n", tmpname, filename);
      exit(EXIT_FAILURE);
    }
    if (!output) printf("Processed %s: %d lines %s, %d lines kept\n", filename, skipped, suffix?"saved":"removed", copied);
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

  for (w = 1; w <= waitsecs; w++) {
    if (waitpid(-1, &status, WNOHANG) > 0) {
      if (WIFEXITED(status) && !WEXITSTATUS(status)) {
        if (!output) printf("Subcommand completed succesfully\n");
      }
      else fprintf(stderr, "Subcommand failed\n");
    }
    sleep(1);

    for (i = 0; inputs[i]; i++) {
      input *input = inputs[i];
      copied = 0;
      clearerr(input->ifp);
      while (fgets(buf, BUFSIZE, input->ifp)) {
        if (!buf[0] || (buf[0] == '\n')) continue;
        fseek(input->ofp, 0, SEEK_END);
        fputs(buf, input->ofp);
        copied++;
      }
      if (copied) {
        fflush(input->ofp);
        if (!output) printf("Copied an additional %d lines written to old %s %d seconds after file rename\n", copied, input->name, w);
      }
    }
  }

  return EXIT_SUCCESS;
}
