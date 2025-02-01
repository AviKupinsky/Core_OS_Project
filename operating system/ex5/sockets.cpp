#include "string"
#include "iostream"
#include "sys/socket.h"
#include <netdb.h>
#include <netinet/in.h>
#include "unistd.h"
#include <climits>
#include "cstring"
#include <vector>
#include <algorithm>

using std::string;
using std::cerr;
using std::endl;
using std::stoi;
using std::vector;


#define SERVER_ARGC 3
#define CLIENT_ARGC 4
#define SERVER_COMMAND "server"
#define CLIENT_COMMAND "client"
#define SYSTEM_ERROR "system error: "
#define GET_HOSTNAME_ERROR "couldn't get host name."
#define GET_HOST_IP_ERROR "couldn't get host IP address."
#define GET_SOCKET_ERROR "couldn't open a socket."
#define BIND_SOCKET_ERROR "couldn't bind socket to port and IP address."
#define LISTEN_SOCKET_ERROR "couldn't listen to socket."
#define ACCEPT_SOCKET_ERROR "couldn't accept a new socket connection."
#define SELECT_SOCKET_ERROR "couldn't select a client socket."
#define READ_COMMAND_ERROR "couldn't get command from client."
#define CONNECT_SOCKET_ERROR "couldn't connect the client to socket."
#define WRITE_COMMAND_ERROR "couldn't send command to server."
#define FAILURE (-1)
#define SUCCESS 0
#define MAX_CLIENTS 5
#define MAX_BUFFER 256
#define COMMAND_INDEX 1
#define PORT_INDEX 2
#define TERMINAL_COMMAND_INDEX 3
#define ZERO_BYTES 0
#define ONE_BYTE 1

/**
 * Adds one to VALUE.
 */
#define ADD_ONE(VALUE) ((VALUE) + 1)


/**
 * Handles system error. prints to stderr an error message and exits with 1.
 * @param message - the error message ro print.
 */
void error_handler(const char * message){
    cerr << SYSTEM_ERROR << message << endl;
    exit(EXIT_FAILURE);
}


/**
 * Opens a socket with the given port and the IP of the local host using TCP protocol.
 * @param port - the port number.
 * @param sa - a reference to an empty struct of type 'sockaddr_in' to be used by the function. At the end of run the
 * variable will be filled with the new socket information.
 * @return - the created socket fd.
 */
int get_socket(unsigned short port, struct sockaddr_in &sa){
    int s;
    char hostname[ADD_ONE(HOST_NAME_MAX)];
    struct hostent *hp;
    // Get local host name:
    if(gethostname(hostname, HOST_NAME_MAX) == FAILURE){
        error_handler(GET_HOSTNAME_ERROR);
    }
    // Get local host IP:
    hp = gethostbyname(hostname);
    if (hp == nullptr){
        error_handler(GET_HOST_IP_ERROR);
    }
    // Update 'sa' with the correct information:
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = hp->h_addrtype;
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_port = htons(port);
    // Open a socket:
    s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == FAILURE){
        error_handler(GET_SOCKET_ERROR);
    }
    return s;
}


/**
 * Opens a socket to be used by the server application.
 * @param port - the port number for the created socket
 * @return - the created socket fd.
 */
int get_socket_server(unsigned short port){
    struct sockaddr_in sa{};
    // Open a socket:
    int s = get_socket(port, sa);
    // Assign the socket with the port number and local host IP address:
    if (bind(s, (struct sockaddr *)&sa, sizeof(struct sockaddr_in)) == FAILURE){
        close(s);
        error_handler(BIND_SOCKET_ERROR);
    }
    // Start accepting clients (up to MAX_CLIENTS will be queued):
    if (listen(s, MAX_CLIENTS) == FAILURE){
        close(s);
        error_handler(LISTEN_SOCKET_ERROR);
    }
    return s;
}


/**
 * Opens a socket to be used by the client application.
 * @param port - the port number of the server to connect to.
 * @return - the created socket fd.
 */
int get_socket_client(unsigned short port){
    struct sockaddr_in sa{};
    // Open a socket:
    int s = get_socket(port, sa);
    // Connect to server with the port number and local host IP address:
    if (connect(s, (struct sockaddr *)&sa, sizeof(sa)) == FAILURE){
        close(s);
        error_handler(CONNECT_SOCKET_ERROR);
    }
    return s;
}


/**
 * Runs the client application. that is, open a socket, connect to server and send the terminal command.
 * @param port - the port number of the server ro connect to.
 * @param command - the terminal command to send to the server.
 * @param len - the length of the terminal command.
 */
void run_client(unsigned short port, char * command, unsigned long len){
    // Open a socket and connect to the server:
    int s = get_socket_client(port);
    // Send command to the server:
    if (write(s, command, len) == FAILURE){
        close(s);
        error_handler(WRITE_COMMAND_ERROR);
    }
    close(s);
}


/**
 * Reads a command that was sent to the server by a client.
 * @param peer - the socket fd of the conversation with the client.
 * @param buf - a buffer with size of at least 'read_len' and 'MAX_BUFFER' at most to read the command / message into.
 * @param read_len - the size of the command / message to read from the socket.
 * @return - the size of the command / message that was read.
 */
size_t read_from_socket(int peer, char * buf, size_t read_len){
    size_t bytes_read = ZERO_BYTES;
    ssize_t this_read = ZERO_BYTES;
    // While didn't read all the command / message:
    while (read_len > bytes_read){
        // Read to buffer:
        this_read = read(peer, reinterpret_cast<char *>(buf) + bytes_read, read_len - bytes_read);
        if (this_read == FAILURE){
            return FAILURE;
        }
        // Reached EOF:
        if (this_read == ZERO_BYTES){
            return bytes_read;
        }
        // Read 'this_read' bytes:
        if (this_read >= ONE_BYTE){
            bytes_read += this_read;
        }
    }
    return bytes_read;
}


/**
 * Connect the server to a new client.
 * @param server_socket - the 'welcome' socket fd of the server.
 * @param connected - clients that are / was connected to the server.
 * @return - SUCCESS if the client was connected successfully and FAILURE otherwise.
 */
int connect_to_client(int server_socket, fd_set &clients, vector<int> &connected){
    // Accept new client:
    int this_socket = accept(server_socket, nullptr, nullptr);
    if (this_socket == FAILURE){
        return FAILURE;
    }
    // Add the new client to 'connected' and 'clients':
    connected.push_back(this_socket);
    FD_SET(this_socket, &clients);
    return SUCCESS;
}


/**
 * Handles a request from a client. that is, reads the terminal command to execute from the socket with the client and
 * executes it.
 * @param peer - the fd of the socket with the client.
 * @return - SUCCESS if the client's request was executed successfully and FAILURE otherwise.
 */
int handle_request(int peer){
    char buffer[MAX_BUFFER];
    memset(buffer, 0, sizeof(buffer));
    // Read the terminal command:
    if (read_from_socket(peer, buffer, MAX_BUFFER) == (size_t)FAILURE){
        return FAILURE;
    }
    // Executes the terminal command:
    system(buffer);
    return SUCCESS;
}


/**
 * Closes the fd of all open sockets.
 * @param connected - clients that are / was connected to the server.
 * @param main_socket - the 'welcome' socket fd of the server.
 */
void close_files(vector<int> &connected, int main_socket){
    for (auto fd : connected){
        close(fd);
    }
    close(main_socket);
}


/**
 * Runs the server application. that is, opens a socket with queue of 'MAX_CLIENTS', accepts 'MAX_CLIENTS' client
 * requests and executes the received terminal command of etch client.
 * @param port - the port number to assign to the server.
 */
void run_server(unsigned short port){
    int s = get_socket_server(port);
    vector<int> connected;
    fd_set clients;
    fd_set readfds;
    FD_ZERO(&clients);
    FD_SET(s, &clients);
    while (true){
        readfds = clients;
        if(select(ADD_ONE(MAX_CLIENTS), &readfds, NULL, NULL, NULL) == FAILURE){
            close_files(connected, s);
            error_handler(SELECT_SOCKET_ERROR);
        }
        if (FD_ISSET(s, &readfds)){
            if(connect_to_client(s, clients, connected) == FAILURE){
                close_files(connected, s);
                error_handler(ACCEPT_SOCKET_ERROR);
            }
            int fd = connected.back();
            if(handle_request(fd) == FAILURE){
                close_files(connected, s);
                error_handler(READ_COMMAND_ERROR);
            }
            FD_CLR(fd, &clients);
            connected.erase(std::remove(connected.begin(), connected.end(), fd), connected.end());
            close(fd);
        }
    }
}


/**
 * Main function - runs the sockets executable logic. that is, checks command type (server or client - first argument)
 * and runs the correct application with the given port number (second argument) and the given terminal command (third
 * argument) if command type was 'client'.
 * * The program will print an error message and exit with EXIT_FAILURE in case of a system error. * *
 * @param argc - the number of arguments received.
 * @param argv - an array of the given arguments.
 * @return - SUCCESS if given the 'client' command type and the client application was executed successfully.
 * Will not return if given the 'server' command type.
 * FAILURE otherwise.
 */
int main(int argc, char * argv[]){
    string command (argv[COMMAND_INDEX]);
    unsigned short port = stoi(string(argv[PORT_INDEX]));
    if (argc == CLIENT_ARGC) {
        if (command != CLIENT_COMMAND){
            return FAILURE;
        }
        string terminal_command(argv[TERMINAL_COMMAND_INDEX]);
        run_client(port, argv[TERMINAL_COMMAND_INDEX], terminal_command.length());
        return SUCCESS;
    }
    else if (argc == SERVER_ARGC){
        if (command != SERVER_COMMAND){
            return FAILURE;
        }
        run_server(port);
    }
}
