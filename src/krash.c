#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h> /*sprintf(..)*/

/*  TODO!  Handle malloc errors
 *         Implement cd
 *         Implement tab
 *         Implement up arrow
 *         Implement $PATH
 *         Implement list of PID's
 * */

#define __COMMAND_LENGTH 50
#define __COMMAND_PADDING 20

struct user {
  char *username;
  char *pwd;
  char *ps1;
};
static struct user kuser;

struct command {
  char **argv;
  char *path;
  char *fcmd;
};

void ps1() {
  for(size_t iter = 0; kuser.ps1[iter]; iter++) {
    if(kuser.ps1[iter] == '\\' && kuser.ps1[iter+1] != '\0') {
      switch(kuser.ps1[iter+1]) {
        case 'u':
          write(1, kuser.username, strlen(kuser.username));
          iter++;
          break;
        case 'W':
          write(1, kuser.pwd, strlen(kuser.pwd));
          iter++;
          break;
        case 'H':
          write(1, "@", 1);
          iter++;
          break;
        case 't':
          time_t now = time(NULL);
          struct tm *tm_struct = localtime(&now);
          char stime[20];
          sprintf(stime, "%d:%d:%d", tm_struct->tm_hour, tm_struct->tm_min, tm_struct->tm_sec);
          write(1, stime, strlen(stime));
          iter++;
          break;
        default:
          write(1, kuser.ps1 + iter + 1, 1);
          iter++;
          break;
      }
    } else {
      write(1, kuser.ps1 + iter, 1);
    }
  }
}

struct user init_user(char **environ) {
  size_t iter = 0;
  for(;environ[iter];iter++) {
    if(strncmp("USER=", environ[iter], 5) == 0) {
      kuser.username = (char*)malloc(sizeof(char)*strlen(environ[iter]));
      strcpy(kuser.username, environ[iter]+5);
    } else if(strncmp("PS1=", environ[iter], 4) == 0) {
      kuser.ps1 = (char*)malloc(sizeof(char)*strlen(environ[iter]));
      strcpy(kuser.ps1, environ[iter]+4);
    } else if(strncmp("PWD=", environ[iter], 4) == 0) {
      kuser.pwd = (char*)malloc(sizeof(char)*strlen(environ[iter]));
      strcpy(kuser.pwd, environ[iter]+4);
    }
  }
}

char **argv_gen(char *const command) {
  size_t len = strlen(command);
  size_t count = 0;
  for(size_t iter = 0; iter < len; iter++) {
    if(command[iter] == ' ') count++;
  }
  char **ret = (char**)malloc(sizeof(char*) * (count + 2));
  char *token = strtok(command, " ");
  size_t iter = 0;
  while(token) {
    *(ret+iter) = (char*)malloc(sizeof(char) * (strlen(token) + 1));
    strcpy(*(ret+iter), token);
    token = strtok(NULL, " ");
    iter++;
  }
  *(ret+iter) = NULL;
  return ret;
}



size_t scounter(char *const restrict command, char const symbol) {
  size_t len = strlen(command);
  size_t count = 0;
  for(size_t iter = 0; iter < len; iter++) 
    if(command[iter] == symbol) count++;
  return count + 1;
}



struct command * new_command_list(char *const command, char *const restrict sep, char const symbol) {
  size_t nocommands = scounter(command, symbol) + 1;
  char **cmds = (char**)malloc(sizeof(char*)*nocommands);
  struct command * commands = (struct command*)malloc(sizeof(struct command) * (nocommands));
  char * cmd = strtok(command, sep);
  size_t iter = 0;
  while(cmd) {
    cmds[iter] = (char*)malloc(sizeof(char)*(strlen(cmd)+1));
    strcpy(cmds[iter], cmd);
    cmd = strtok(NULL, sep);
    iter++;
  }
  for(iter = 0; iter < nocommands - 1 && cmds[iter]; iter++) {
    commands[iter].fcmd = (char*)malloc(sizeof(char) * (strlen(cmds[iter]) + (__COMMAND_PADDING) ));
    strcpy(commands[iter].fcmd, cmds[iter]);
    commands[iter].argv = argv_gen(cmds[iter]);
    commands[iter].path = (char*)malloc(sizeof(char)*(strlen(commands[iter].argv[0]+ (__COMMAND_PADDING))));
    strcat(commands[iter].path, "/usr/bin/");
    strcat(commands[iter].path, commands[iter].argv[0]);
  }
  commands[iter].path = 0;
  return commands;
}

int child(int in, int out, struct command cmd, char **environ) {
  pid_t pid = fork();
  if(pid == 0) {
    if(in != 0) {
      dup2(in, 0);
      close(in);
    }
    if(out != 1) {
      dup2(out, 1);
      close(out);
    }
    return execve(cmd.path, cmd.argv, environ);
  }
  return pid;
}

void psystem(char *const command, char **environ) {
  struct command *commands = new_command_list(command, "|", '|');
  size_t iter = 0;
  int fd[2], in=0;

  if(commands[iter].path == 0) return;
  while(commands[iter+1].path) {
    pipe(fd);
    child(in, fd[1], commands[iter], environ);
    close(fd[1]);
    in = fd[0];
    iter++;
  }
  pid_t pid = fork();
  if(pid == 0) {
    if(in != 0) dup2(in, 0);
    execve(commands[iter].path, commands[iter].argv, environ);
    exit(0);
  }
  wait(NULL);
}

void ksystem(char *const command, char **environ) {
  pid_t pid = fork();
  if(pid == 0) {
    if(scounter(command, '|') <= 1) { 
      char **argv = argv_gen(command);
      char *path = (char*)malloc(sizeof(char) * (strlen(argv[0]) +(__COMMAND_PADDING)));
      strcat(path, "/usr/bin/");
      strcat(path, argv[0]);
      if(pid == 0) {
        execve(path, argv, environ);
        exit(0);
      }
    } else {
      psystem(command, environ);
    }
    exit(0);
  }
  wait(NULL);
}

void ssystem(char *const command, char **environ) {
  if(scounter(command, ';') > 1) {
    struct command * commands = new_command_list(command, ";", ';');
    size_t iter = 0;
    while(commands[iter].path) {
      if(commands[iter].fcmd) ksystem(commands[iter].fcmd, environ);
      iter++;
    }
  }
  ksystem(command, environ);
}

int shell(char **environ) {
  init_user(environ);
  char command[__COMMAND_LENGTH];// = (char*)malloc(sizeof(char)*(__COMMAND_LENGTH));
  while(strcmp(command, "exit")) {
    ps1();
    size_t bytes_read = 0;
    while((bytes_read = read(1, command + bytes_read, 1) + bytes_read) > 0) {
      if(*(command + bytes_read - 1) == '\n') {
        *(command + bytes_read - 1) = '\0';
        break;
      }
    }
    if(bytes_read > (__COMMAND_LENGTH)-1) return 1;
    if(bytes_read > 0) {
      ssystem(command, environ);
    }
  }
}

int main(const int _, char **__, char **environ) {
  return shell(environ);
}
