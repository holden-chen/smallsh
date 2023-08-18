#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

#define arrlen(a) (sizeof(a) / sizeof *(a))

char *words[MAX_WORDS];
size_t wordsplit(char const *line);
char * expand(char const *word);

char *last_fg_exit_status = "0";
char *last_bg_PID = "";

void
sigint_handler(int sig) {}

int main(int argc, char *argv[])
{
  FILE *input = stdin;
  char *input_fn = "(stdin)";
  if (argc == 2) {
    input_fn = argv[1];
    input = fopen(input_fn, "re");
    if (!input) err(1, "%s", input_fn);
  } else if (argc > 2) {
    errx(1, "too many arguments");
  } else if (argc == 0) {
    errx(1, "No arguments entered");
  }

  /* for signal handling */
  struct sigaction sa_SIGTSTP = {0}, oldsa_SIGTSTP = {0}, sa_SIGINT = {0}, oldsa_SIGINT = {0}; 
  sigemptyset(&sa_SIGTSTP.sa_mask);
  sigemptyset(&sa_SIGINT.sa_mask);
  sa_SIGTSTP.sa_flags = 0;
  sa_SIGINT.sa_flags = 0;

  char *prompt = getenv("PS1");  // for interactive mode

  char *line = NULL;
  size_t n = 0;

  for (;;) {
    int wstatus;
    pid_t terminatedChild;
    clearerr(input);

prompt:;
       /* Manage background processes*/
    while ((terminatedChild = waitpid(-1, &wstatus, WNOHANG | WUNTRACED)) > 0) {
      if (WIFSIGNALED(wstatus)) {
        int signal_num = WTERMSIG(wstatus);  // num of signal that caused the process to terminated
        fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) terminatedChild, signal_num);
      } else if (WIFEXITED(wstatus)) {
        int exit_status = WEXITSTATUS(wstatus);
        fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) terminatedChild, exit_status);
      } else if (WIFSTOPPED(wstatus)) {
        if (kill(terminatedChild, SIGCONT) == -1) {
          fprintf(stderr, "ERROR with kill() system call");
        }
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) terminatedChild);
      }
    }

    /* If in interactive mode */
    if (input == stdin) {
      sa_SIGTSTP.sa_handler = SIG_IGN;
      sa_SIGINT.sa_handler = sigint_handler;

      fprintf(stderr, "%s", prompt);

      /* ignore SIGTSTP */
      if (sigaction(SIGTSTP, &sa_SIGTSTP, &oldsa_SIGTSTP) == -1) {
        fprintf(stderr, "ERROR ignoring SIGTSTP");
        exit(1);
      }

      if (sigaction(SIGINT, &sa_SIGINT, &oldsa_SIGINT) == -1) {
        fprintf(stderr, "ERROR sigaction() for SIGINT");
        exit(1);
      }
    } 

    ssize_t line_len = getline(&line, &n, input);
    if (line_len < 0) {
      if (errno == EINTR) {
        fprintf(stderr, "\n");
      } else {
        exit(0);
      }
    }

    clearerr(input);

    sa_SIGINT.sa_handler = SIG_IGN;
    if (sigaction(SIGINT, &sa_SIGINT, NULL) == -1) {
      fprintf(stderr, "ERROR sigaction() for SIGINT");
      exit(1);
    }

    size_t nwords = wordsplit(line);

    if (nwords <= 0) {
      goto prompt;
    }  

    for (size_t i = 0; i < nwords; ++i) {
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
    }

    // Fill (sanitized) childargs array
    char *childargs[MAX_WORDS] = {NULL};
    size_t j = 0;
    int childargscount = 0;
    for (size_t i = 0; i < nwords; ++i) {
      if (strcmp(words[i], "<") == 0 || strcmp(words[i], ">") == 0 || strcmp(words[i], ">>") == 0 || strcmp(words[i], "&") == 0) {
        ++i;
        if (i >= nwords) break;
        continue;
      }
      childargs[j] = words[i];
      ++j;
      childargscount = childargscount + 1;
    }

    /* exit command */
    if (strcmp(childargs[0], "exit") == 0) {
      if (childargscount > 2) fprintf(stderr, "Error: more than one argument passed to 'exit'.\n");
      
      if (childargscount == 2) {  // if a status arg is passed, exit with the given status
        char *endptr;
        errno = 0;
        long int status = strtol(childargs[1], &endptr, 10);
        if (errno != 0) fprintf(stderr, "Error occurred with strtol()\n");
        exit(status);
      } else {
        exit(0);
      }
    } else if (strcmp(childargs[0], "cd") == 0) {  // cd command
      if (childargscount > 2) fprintf(stderr, "Error: more than one argument passed to 'cd'.\n");
      
      // if no arg passed to cd, use HOME`
      if (childargscount == 1) {
        const char *homedir = getenv("HOME");
        if (chdir(homedir) == 0) continue;
          else fprintf(stderr, "cd: %s\n", strerror(errno));
        continue;
      } 

      if (childargscount == 2) {
        if (chdir(childargs[1]) == 0) continue;
          else fprintf(stderr, "cd: %s\n", strerror(errno));
        continue;
      }  
    } else {
      // Non-built-in commands
	    int childStatus;

	    // Fork a new process
	    pid_t spawnPid = fork();

	    switch(spawnPid){
	    case -1:
		    perror("fork()\n");
		    exit(1);
		    break;
	    case 0:
		    // In the child process

        // Reset signals to original disposition
        if (sigaction(SIGTSTP, &oldsa_SIGTSTP, NULL) == -1) {
          fprintf(stderr, "ERROR resetting SIGTSTP to original disposition");
          exit(1);
        }

        if (sigaction(SIGINT, &oldsa_SIGINT, NULL) == -1) {
          fprintf(stderr, "ERROR resetting SIGINT to original disposition");
          exit(1);
        }

        // REDIRECTION
        for (size_t i = 0; i < nwords; ++i) {
          if (strcmp(words[i], "<") == 0) {
            if (i+1 >= nwords) {
              fprintf(stderr, "Error: missing redirection operand");
              goto prompt;
            }
            // open source file
            close(0);
            int sourceFD = open(words[i+1], O_RDONLY);
            if (sourceFD == -1) {
              perror("source open()");
              exit(1);
            }
            // redirect stdin to source file
            if (sourceFD != 0) {
              dup2(sourceFD, 0);
              close(sourceFD);
            }
          } else if (strcmp(words[i], ">") == 0) {
            if (i+1 >= nwords) {
              fprintf(stderr, "Error: missing redirection operand");
              goto prompt;
            }
            // open target file
            close(1);
            int targetFD = open(words[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
            if (targetFD == -1) {
              perror("target open()");
              exit(1);
            }
            // redirect stdout to target file
            if (targetFD != 1) {
              dup2(targetFD, 1);
              close(targetFD);
            }
          } else if (strcmp(words[i], ">>") == 0) {
            if (i+1 >= nwords) {
              fprintf(stderr, "Error: missing redirection operand");
              goto prompt;
            }
            // open target file
            close(1);
            int targetFD = open(words[i+1], O_WRONLY | O_CREAT | O_APPEND, 0777);
            if (targetFD == -1) {
              perror("target open()");
              exit(1);
            }
            // redirect stdout to target file
            if (targetFD != 1) {
              dup2(targetFD, 1);
              close(targetFD);
            }
          }
        } // end for loop
		 
		    execvp(childargs[0], childargs);  // execute the non-built-in command
                                          
		    // exec only returns if there is an error
		    perror("execvp");
		    exit(2);
		    break;
	    default:
		    // In the parent process
        
        // If background operator is not present,
        // perform blocking wait
        if (strcmp(words[nwords-1], "&") != 0) {
          spawnPid = waitpid(spawnPid, &childStatus, WUNTRACED);

          // check whether command terminated by signal
          if (WIFSIGNALED(childStatus)) {
            int signal_num = WTERMSIG(childStatus);  // num of signal that caused the process to terminate
            int exit_status = signal_num + 128;
            char exit_status_str[20];
            sprintf(exit_status_str, "%d", exit_status);
            last_fg_exit_status = exit_status_str;
          } else if (WIFEXITED(childStatus)) {
            int exit_status = WEXITSTATUS(childStatus);
            // Convert status int to str
            char status_str[25];
            sprintf(status_str, "%d", exit_status);
            last_fg_exit_status = status_str;
          } else if (WIFSTOPPED(childStatus)) {
            if (kill(spawnPid, SIGCONT) == -1) {
              fprintf(stderr, "ERROR with kill() system call (foreground process)");
            }
            // update $!
            char spawnPid_str[25];
            sprintf(spawnPid_str, "%jd", (intmax_t) spawnPid);
            last_bg_PID = spawnPid_str;

            fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) spawnPid);
          } else {
            int exit_status = WEXITSTATUS(childStatus);
            // convert status int to str
            char status_str[20];
            sprintf(status_str, "%d", exit_status);
            last_fg_exit_status = status_str;
          }
        } else {
          char spawnPid_str[25];
          sprintf(spawnPid_str, "%jd", (intmax_t) spawnPid);
          last_bg_PID = spawnPid_str;
          continue;
        } 
        goto prompt;
        break;
	    } 
    }
  }
}

char *words[MAX_WORDS] = {0};

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;

  char const *c = line;
  for (;*c && isspace(*c); ++c); /* discard leading space */

  for (; *c;) {
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
    for (;*c && !isspace(*c); ++c) {
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
      if (!tmp) err(1, "realloc");
      words[wind] = tmp;
      words[wind][wlen++] = *c; 
      words[wind][wlen] = '\0';
    }
    ++wind;
    wlen = 0;
    for (;*c && isspace(*c); ++c);
  }
  return wind;
}


/* Find next instance of a parameter within a word. Sets
 * start and end pointers to the start and end of the parameter
 * token.
 */
char
param_scan(char const *word, char const **start, char const **end)
{
  static char const *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = 0;
  *end = 0;
  for (char const *s = word; *s && !ret; ++s) {
    s = strchr(s, '$');
    if (!s) break;
    switch (s[1]) {
    case '$':
    case '!':
    case '?':
      ret = s[1];
      *start = s;
      *end = s + 2;
      break;
    case '{':;
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = s[1];
        *start = s;
        *end = e + 1;
      }
      break;
    }
  }
  prev = *end;
  return ret;
}

/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
 */
char *
build_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *base = 0;

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.
   * Returns a newly allocated string that the caller must free.
   */
  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  void *tmp = realloc(base, newsize);
  if (!tmp) err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n);
  base_len += n;
  base[base_len] = '\0';

  return base;
}

/* Expands all instances of $! $$ $? and ${param} in a string 
 * Returns a newly allocated string that the caller must free
 */
char *
expand(char const *word)
{
  char const *pos = word;
  char const *start, *end;
  char c = param_scan(pos, &start, &end);
  free(build_str(NULL, NULL));

  while (c) {
    build_str(pos, start);

    if (c == '!') {
      build_str(last_bg_PID, NULL);
    } else if (c == '$') {
      pid_t smallshPID = getpid();
      char str[40];
      sprintf(str, "%d", smallshPID);  // convert int to str
      const char *smallshPID_str = str;
      build_str(smallshPID_str, NULL);
    } else if (c == '?') {
      build_str(last_fg_exit_status, NULL);
    } else if (c == '{') {
      *(char *)(end - 1) = 0;
      char const *name = start + 2;
      if (strlen(name) > 0) {
        char *value = getenv(name);
        if (value == NULL) {
          build_str("", NULL);
        } else {
          //free(build_str(NULL, NULL));
          build_str(value, NULL);
        }
      }
    }
    pos = (char *)end;
    c = param_scan(pos, &start, &end);
  }
  build_str(pos, NULL);
  return build_str(start, NULL);
}
