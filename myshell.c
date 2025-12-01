#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAXBUF 256
#define MAXARGS 32

int getcmd(char *buf, int nbuf) {
  fprintf(2, ">>> ");
  if (gets(buf, nbuf) == 0)
    return -1;
  return 0;
}

// Basic split by spaces
int parse(char *buf, char *argv[]) {
  int i = 0;
  while (*buf) {
    while (*buf == ' ' || *buf == '\t')
      buf++;
    if (*buf == 0)
      break;
    argv[i++] = buf;
    while (*buf && *buf != ' ' && *buf != '\t')
      buf++;
    if (*buf) {
      *buf = 0;
      buf++;
    }
  }
  argv[i] = 0;
  return i;
}

// Handle < and >, but very fragile
void redirection(char *argv[], int argc) {
  for (int i = 0; i < argc; i++) {
    if (argv[i] == 0) continue;

    if (strcmp(argv[i], ">") == 0 && i + 1 < argc) {
      int fd = open(argv[i+1], O_WRONLY | O_CREATE | O_TRUNC);
      if (fd >= 0) {
        close(1);
        dup(fd);
        close(fd);
      }
      argv[i] = 0;
      break;
    }

    if (strcmp(argv[i], "<") == 0 && i + 1 < argc) {
      int fd = open(argv[i+1], O_RDONLY);
      if (fd >= 0) {
        close(0);
        dup(fd);
        close(fd);
      }
      argv[i] = 0;
      break;
    }
  }
}

// Single pipe only
int split_pipe(char *cmd, char *left, char *right) {
  for (char *p = cmd; *p; p++) {
    if (*p == '|') {
      *p = 0;
      strcpy(left, cmd);
      strcpy(right, p+1);
      return 1;
    }
  }
  return 0;
}

void run_simple(char *cmd) {
  char *argv[MAXARGS];
  int argc = parse(cmd, argv);
  if (argc == 0) return;

  // cd
  if (strcmp(argv[0], "cd") == 0) {
    if (argc < 2) fprintf(2, "cd: missing arg\n");
    else if (chdir(argv[1]) < 0)
      fprintf(2, "cd: cannot cd to %s\n", argv[1]);
    return;
  }

  int pid = fork();
  if (pid == 0) {
    redirection(argv, argc);
    exec(argv[0], argv);
    fprintf(2, "exec failed\n");
    exit(1);
  }
  wait(0);
}

void run_pipe(char *cmd1, char *cmd2) {
  int fds[2];
  pipe(fds);

  int pid1 = fork();
  if (pid1 == 0) {
    close(1);
    dup(fds[1]);
    close(fds[0]);
    close(fds[1]);
    run_simple(cmd1);
    exit(0);
  }

  int pid2 = fork();
  if (pid2 == 0) {
    close(0);
    dup(fds[0]);
    close(fds[0]);
    close(fds[1]);
    run_simple(cmd2);
    exit(0);
  }

  close(fds[0]);
  close(fds[1]);
  wait(0);
  wait(0);
}

void run_command(char *cmd) {
  char left[MAXBUF], right[MAXBUF];

  // support only ONE pipe
  if (split_pipe(cmd, left, right)) {
    run_pipe(left, right);
  } else {
    run_simple(cmd);
  }
}

int main() {
  static char buf[MAXBUF];

  while (getcmd(buf, sizeof(buf)) >= 0) {
    // no semicolon support
    run_command(buf);
  }
  exit(0);
}
