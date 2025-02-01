#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <iostream>
#include <sched.h>
#include <linux/sched.h>
#include <unistd.h>
#include <cstring>
#include <fstream>


using std::cerr;
using std::endl;
using std::string;
using std::allocator;
using std::fstream;
using std::ofstream;
using std::to_string;
using std::stoi;


#define SYSTEM_ERROR "system error: "
#define ERROR_ALLOCATION "couldn't allocate stack."
#define ERROR_NAME "wasn't able to change the hostname."
#define ERROR_RUNNING_NEW_PROGRAM "wasn't able to run the new program."
#define ERROR_CREATING_NEW_DIRECTORY "wasn't able to create a directory."
#define ERROR_FILE_DIRECTORY "wasn't able to change working directory."
#define ERROR_ROOT_DIRECTORY "wasn't able to change the root directory."
#define ERROR_MOUNT "wasn't able to mount the file."
#define ERROR_UMOUNT "wasn't able to umount the file."
#define ERROR_DELETE "wasn't able to delete the file."
#define ERROR_WAIT "wasn't able to wait for chilled process."
#define PARENT_DIR ".."
#define SYS "sys"
#define FS "fs"
#define CGROUP "cgroup"
#define PIDS "pids"
#define MAX_FILE "pids.max"
#define PROCS_FILE "cgroup.procs"
#define PROC "proc"
#define CONFIGURATION_FILE_WRITE_VAL 1
#define RELEASE_CONTAINER_RESOURCES_FILE "notify_on_release"
#define DIR(DIR_NAME) (string("/").append(DIR_NAME))
#define HOSTNAME_ARG_INDEX 1
#define MAX_NUM_PROCESSES_ARG_INDEX 3
#define PROGRAM_RUN_IN_CONTAINER 4
#define NUM_ARGUMENTS 5
#define STACK_SIZE 8192
#define PREMISSIONS_NUM 0755
#define FAILED (-1)
#define BUFFER_SIZE 256
#define SUCCESS 0
#define ROOT_DIR_ARG_INDEX 2
#define LIMIT_PROCESSES_NUM_DIR_DEPTH 4
#define ADD_ONE(VALUE) ((VALUE) + 1)


/**
 * Prints the relevant error the has occurred and exits with EXIT_FAILURE.
 * @param message - the error message to print.
 */
void error_handler(const string& message){
    cerr << SYSTEM_ERROR << message << endl;
    exit(EXIT_FAILURE);
}


/**
 * Releasing all the directory and files we created.
 * @param container_root_directory - the container's root directory.
 */
void releasing_all_resources(const string& container_root_directory){
    string pids_directory = container_root_directory + DIR(SYS) + DIR(FS) + DIR(CGROUP) + DIR(PIDS);
    char buffer[BUFFER_SIZE];
    strcpy(buffer, pids_directory.c_str());
    if (chdir(pids_directory.c_str()) == FAILED){
        error_handler(ERROR_FILE_DIRECTORY);
    }
    if (remove(MAX_FILE) != SUCCESS){
        error_handler(ERROR_DELETE);
    }
    if (remove(RELEASE_CONTAINER_RESOURCES_FILE) != SUCCESS){
        error_handler(ERROR_DELETE);
    }
    if (remove(PROCS_FILE) != SUCCESS){
        error_handler(ERROR_DELETE);
    }
    if (chdir(PARENT_DIR) != SUCCESS){
        error_handler(ERROR_FILE_DIRECTORY);
    }
    if (remove(PIDS) != SUCCESS){
        error_handler(ERROR_DELETE);
    }
    if (chdir(PARENT_DIR) != SUCCESS){
        error_handler(ERROR_FILE_DIRECTORY);
    }
    if (remove(CGROUP) != SUCCESS){
        error_handler(ERROR_DELETE);
    }
    if (chdir(PARENT_DIR) != SUCCESS){
        error_handler(ERROR_FILE_DIRECTORY);
    }
    if (remove(FS) != SUCCESS){
        error_handler(ERROR_DELETE);
    }
}


/**
 * Limits the number of processes the container can create.
 * @param max_processes - the max number of processes for the container.
 */
void limiting_number_processes(const string& max_processes){
    string sys_directory = DIR(SYS);
    if (chdir(sys_directory.c_str()) == FAILED){
        error_handler(ERROR_FILE_DIRECTORY);
    }
    if (mkdir(FS, PREMISSIONS_NUM) == FAILED){
        error_handler(ERROR_CREATING_NEW_DIRECTORY);
    }
    string fs_directory = sys_directory + DIR(FS);
    if (chdir(fs_directory.c_str()) == FAILED){
        error_handler(ERROR_FILE_DIRECTORY);
    }
    if (mkdir(CGROUP, PREMISSIONS_NUM) == FAILED){
        error_handler(ERROR_CREATING_NEW_DIRECTORY);
    }
    string cgroup_directory = fs_directory + DIR(CGROUP);
    if (chdir(cgroup_directory.c_str()) == FAILED){
        error_handler(ERROR_FILE_DIRECTORY);
    }
    if (mkdir(PIDS, PREMISSIONS_NUM) == FAILED){
        error_handler(ERROR_CREATING_NEW_DIRECTORY);
    }
    string pids_directory = cgroup_directory + DIR(PIDS);
    if (chdir(pids_directory.c_str()) == FAILED){
        error_handler(ERROR_FILE_DIRECTORY);
    }
    std::ofstream procs_file, resources_file, max_file;
    procs_file.open(PROCS_FILE);
    procs_file << getpid();
    string file_dir = pids_directory + DIR(PROCS_FILE);
    chmod(file_dir.c_str(), PREMISSIONS_NUM);
    procs_file.close();
    resources_file.open(RELEASE_CONTAINER_RESOURCES_FILE);
    file_dir = pids_directory + DIR(RELEASE_CONTAINER_RESOURCES_FILE);
    chmod(file_dir.c_str(), PREMISSIONS_NUM);
    resources_file << CONFIGURATION_FILE_WRITE_VAL;
    resources_file.close();
    max_file.open(MAX_FILE);
    file_dir = pids_directory + DIR(MAX_FILE);
    chmod(file_dir.c_str(), PREMISSIONS_NUM);
    max_file << max_processes;
    max_file.close();
}


/**
 * Gets all the arguments for the program and runs it in the container.
 * @param elements - all th argument we have in the command line.
 */
void run_program(char **elements){
    int num_of_args = std::stoi(elements[0]);
    char * execvp_args[ADD_ONE(num_of_args - PROGRAM_RUN_IN_CONTAINER)];
    for (int i = 0; i < ADD_ONE(num_of_args - PROGRAM_RUN_IN_CONTAINER); ++i) {
        if (i == num_of_args - PROGRAM_RUN_IN_CONTAINER){
            execvp_args[i] = (char *) nullptr;
            break;
        }
        execvp_args[i] = elements[PROGRAM_RUN_IN_CONTAINER + i];
    }
    if (execvp(elements[PROGRAM_RUN_IN_CONTAINER], execvp_args) == FAILED){
        error_handler(ERROR_RUNNING_NEW_PROGRAM);
    }
}


/**
 * Creates the container and runs the given program with it's arguments in the container.
 * @param argv - the arguments received in the command line.
 * @return - SUCCESS if container was crated successfully and program was run successfully.
 */
int create_container(void *argv){
    char **arguments = (char **)argv;
    string host_name = arguments[HOSTNAME_ARG_INDEX];
    if (sethostname(arguments[HOSTNAME_ARG_INDEX], host_name.size()) == FAILED){
        error_handler(ERROR_NAME);
    }
    if (chdir(arguments[ROOT_DIR_ARG_INDEX]) == FAILED){
        error_handler(ERROR_FILE_DIRECTORY);
    }
    if (chroot(arguments[ROOT_DIR_ARG_INDEX]) == FAILED){
        error_handler(ERROR_ROOT_DIRECTORY);
    }
    limiting_number_processes(arguments[MAX_NUM_PROCESSES_ARG_INDEX]);
    if (mount(PROC, DIR(PROC).c_str(), PROC, 0 , nullptr)  == FAILED){
        error_handler(ERROR_MOUNT);
    }
    for (int i = 0; i < LIMIT_PROCESSES_NUM_DIR_DEPTH; ++i) {
        if (chdir(PARENT_DIR) != SUCCESS){
            error_handler(ERROR_FILE_DIRECTORY);
        }
    }
    run_program(arguments);
    return SUCCESS;
}


/**
 * Unmounts the container.
 * @param root_directory - the container's root directory.
 */
void umount_container(const string& root_directory){
    string proc_directory = root_directory + DIR(PROC);
    if (umount(proc_directory.c_str()) == FAILED){
        error_handler(ERROR_UMOUNT);
    }
}


/**
 * Main function - runs the container executable logic. that is, checks the number of given arguments, creates a
 * container with the given hostname (first argument), new filesystem at the given root (second argument), its own proc
 * filesystem, limits the number of process that can run to the given limit (third argument) and new process IDs space
 * and then runs the given program (forth argument) with its arguments (the arguments from the fifth argument) in the
 * container.
 * * The program will print an error message and exit with EXIT_FAILURE in case of a system error. * *
 * @param argc - the number of arguments received.
 * @param argv - an array of the given arguments.
 * @return - SUCCESS if the container was created successfully and the given program run was successful and FAILED
 * otherwise.
 */
int main(int argc, char *argv[]){
    if (argc < NUM_ARGUMENTS){
        return FAILED;
    }
    allocator<char> alloc;
    char* stack = alloc.allocate(STACK_SIZE);
    char **clone_args = new char *[argc];
    if (!stack | !clone_args){
        error_handler(ERROR_ALLOCATION);
    }
    string s = to_string(argc);
    char * num_of_args = const_cast<char *>(s.c_str());
    clone_args[0] = num_of_args;
    for (int i = 1; i < argc; ++i) {
        clone_args[i] = argv[i];
    }
    clone(create_container, (void *) (stack + STACK_SIZE),
          CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | SIGCHLD, clone_args);
    if (wait(nullptr) == FAILED){
        error_handler(ERROR_WAIT);
    }
    umount_container(argv[ROOT_DIR_ARG_INDEX]);
    releasing_all_resources(argv[ROOT_DIR_ARG_INDEX]);
    delete stack;
    delete[] clone_args;
    return SUCCESS;
}
