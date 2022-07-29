#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#define CGROUP_FOLDER "/sys/fs/cgroup/pids/manager/"
#define concat(s1,s2) (s1"" s2)
#define HOSTNAME "manager"

void write_rule(const char* path, const char* value) {
  int fp = open(path, O_WRONLY | O_APPEND );
  write(fp, value, strlen(value));
  close(fp);
}
/**Function to limit the process creation using cgroups*/
void limitProcessCreation() {
  // create a folder
  mkdir( CGROUP_FOLDER, S_IRUSR | S_IWUSR);
  char pid[6];
  int pid_int = getpid();
  sprintf(pid,"%d",pid_int);
  write_rule(concat(CGROUP_FOLDER, "cgroup.procs"), pid);
  write_rule(concat(CGROUP_FOLDER, "notify_on_release"), "1");
  write_rule(concat(CGROUP_FOLDER, "pids.max"), "5");
}

/**Allocate  memory for cloned process. */
char* stack_memory() {
  const int stackSize = 65536;
  char *stack = (char*) malloc(stackSize);

  if (stack == NULL) {
    printf("Cannot allocate memory \n");
    exit(EXIT_FAILURE);
  }

  return stack+stackSize;  /*move the pointer to the end of the array because the stack grows backward. */
}

/**Reset environment variables and reset.*/
int setup_variables() {
  /**Resolve a possibility of information leak from the guest.*/
  if(clearenv()!=0) {
    perror("clearenv");
    return EXIT_FAILURE;
  }
  if(setenv("PATH", "/bin/:/sbin/:usr/bin:/usr/sbin", 0)!=0) {
    perror("setenv");
    return EXIT_FAILURE;
  }
  if(setenv("PS1", "\\u@\\h:\\w\\$", 0)!=0) {
    perror("setenv");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/**Set the new root location to the location provided via cmd line*/
int setup_root(const char* folder) {
  if(chroot(folder)==-1) {
    perror("chroot");
    return EXIT_FAILURE;
  }
  if(chdir("/")==-1) {
    perror("chdir");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

char* getAbsolutePath(void *args) {
  char *params = (char*) args;
  char *rootpath= (char*)malloc(sizeof(char)*200);
  strcpy(rootpath,"./");//"/home/");
  strcat(rootpath, params);
  return rootpath;
}

/**Intention was to restart the init. But there is the problem with bus not available*/
int runInit() {
  char *pgm = "/usr/sbin/init u";
  char *_args[] = {pgm, (char*) 0};
  if(execvp(pgm,_args)==-1) {
      perror("exec");
      return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/**An abstract function which runs an executable program by replacing the calling process. */
int run(void *args) {
  const char* pgm = (const char*) args;
  //char *pgm = "/bin/bash";
  char *_args[] = {(char *)pgm, (char *)0 };
  if(execvp(pgm,_args)==-1) {
      perror("exec");
      return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/**Get the boot log */
int readBootLog(char *file_name) {
  FILE *fp;
  char ch;
  fp = fopen(file_name, "r"); // read mode

   if (fp == NULL)
   {
      perror("Error while opening the file.\n");
      return EXIT_FAILURE;
   }
   while((ch = fgetc(fp)) != EOF)
      printf("%c", ch);
   fclose(fp);
   return EXIT_SUCCESS;
}
/**Function which generates the shell process within a container.*/
int createContainer(char *rootpath) {
  char shell[20];
  strcpy(shell,"/bin/sh");
  pid_t shell_pid;

 /*Setup the environment variables*/
  if(setup_variables()==EXIT_FAILURE) {
    return EXIT_FAILURE;
  }
  /**change a new path as the root location.*/
  if(setup_root(rootpath)==EXIT_FAILURE) {
    return EXIT_FAILURE;
  }
  /**Isolate and mount the proc to define independent */
  if(mount("proc", "/proc", "proc", 0, "")==-1) {
    perror("mount");
    return EXIT_FAILURE;
  }

  /**Get boot log info when called with rootfs as the cmdline value*/
  if((strcmp(rootpath,"rootfs")==0)) {
    if(readBootLog("/var/log/boot.log")==EXIT_FAILURE) {
      return EXIT_FAILURE;
    }
  }

  /**Clone a shell process from the container process.*/
  shell_pid = clone(run,stack_memory(), SIGCHLD,(void*)shell);
  if (shell_pid == -1) {
        perror("clone");
        return EXIT_FAILURE;
    }
    /**Wait until the process terminates.*/
    if(waitpid(shell_pid, NULL, 0)==-1){
      perror("waitpid");
      return EXIT_FAILURE;
    }
  /**Unmount the proc fs*/
  if(umount("/proc")==-1) {
    perror("umount");
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

/**Function which manages the container process*/
int createContainerProcess(void *args) {

  char *rootpath;
  char name[20] = HOSTNAME;
  //limitProcessCreation();
//  runInit();
  /**Set host name for the container.*/
  if(sethostname(name, strlen(name))==-1) {
    return EXIT_FAILURE;
  }

  if(args==NULL) {
    rootpath = "rootfs";
    if(createContainer(rootpath)==EXIT_FAILURE) {
      return EXIT_FAILURE;
    }
  }
  else {
    rootpath = getAbsolutePath(args);
    int ret = createContainer(rootpath);
    free(rootpath);
    if(ret==EXIT_FAILURE) {
      return ret;
    }
  }
  return EXIT_SUCCESS;
}

int main(int argc, char** argv) {

  /**Flags for creating new UTS, Isloate process tree with new processids, new namespaces, new network link*/
  int namespaces = CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWNS | CLONE_NEWNET;
  pid_t container;
  /**Building the container process with a clone.*/
  container = clone(createContainerProcess, stack_memory(), namespaces | SIGCHLD, (void*)argv[1]);
  if (container == -1) {
        perror("clone");
        return EXIT_FAILURE;
  }
  /**Parent process waiting on child porcess which is the container process.*/
  if(waitpid(container, NULL, 0)==-1) {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
