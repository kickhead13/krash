#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>

#define __COMMAND_LENGTH 200
#define __COMMAND_PADDING 20

/*  TODO!  Handle malloc errors
 *         Implement cd
 *         Implement tab
 *         Implement up arrow
 *         Implement $PATH
 * */

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

int check_pipe(char *const command) {
  size_t len = strlen(command);
  for(size_t iter = 0; iter < len; iter++) 
    if(command[iter] == '|') return 1;
  return 0;
}

size_t npipes(char *const command) {
  size_t len = strlen(command);
  size_t count = 0;
  for(size_t iter = 0; iter < len; iter++) 
    if(command[iter] == '|') count++;
  return count + 1;
}

struct command {
  char **argv;
  char *path;
};

struct command * new_command_list(char *const command) {
  size_t nopipes = npipes(command) + 1;
  char **cmds = (char**)malloc(sizeof(char*)*nopipes);
  struct command * commands = (struct command*)malloc(sizeof(struct command) * (nopipes));
  char * cmd = strtok(command, "|");
  size_t iter = 0;
  while(cmd) {
    cmds[iter] = (char*)malloc(sizeof(char)*(strlen(cmd)+1));
    strcpy(cmds[iter], cmd);
    cmd = strtok(NULL, "|");
    iter++;
  }
  for(iter = 0; iter < nopipes - 1; iter++) {
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
  struct command *commands = new_command_list(command);
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
  }
}

void ksystem(char *const command, char **environ) {
  pid_t pid = fork();
  if(pid == 0) {
    if(!check_pipe(command)) { 
      char **argv = argv_gen(command);
      char *path = (char*)malloc(sizeof(char) * (strlen(argv[0]) +(__COMMAND_PADDING)));
      strcat(path, "/usr/bin/");
      strcat(path, argv[0]);
      if(pid == 0) {
        execve(path, argv, environ);
      }
    } else {
      psystem(command, environ);
    }
  }
  wait(NULL);
}

int shell(char **environ) {
  char *command = (char*)malloc(sizeof(char)*(__COMMAND_LENGTH));
  while(strcmp(command, "exit")) {
    size_t bytes_read = 0;
    while((bytes_read = read(1, command + bytes_read, 1) + bytes_read) > 0) {
      if(*(command + bytes_read - 1) == '\n') {
        *(command + bytes_read - 1) = '\0';
        break;
      }
    }
    if(bytes_read > (__COMMAND_LENGTH)-1) return 1;
    //write(1, command, strlen(command));
    ksystem(command, environ);
  }
}

int main(const int _, char **__, char **environ) {
  return shell(environ);
}
