// Submission by Loic Dallaire
// Student ID: 002311806

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>

struct shell_state
{
    int VSIZE;
    int HSIZE;
    char* HOST;
    unsigned short PORT;
    int KEEP_ALIVE;
    int SOCKET;
};

int connect_by_port_id(const char* host, const unsigned short port) 
{
    struct hostent* info;
    struct sockaddr_in sin;
    const int type = SOCK_STREAM;
    int sd;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    printf("HOST: %s\n", host);
    info = gethostbyname("osiris.ubishops.ca");
    if (info == NULL) {
        herror("bad info_struct");
        return -1;
    }
    memcpy(&sin.sin_addr, info->h_addr, info->h_length);

    sin.sin_port = htons(port);

    sd = socket(PF_INET, type, 0);
    if (sd < 0) {
        printf("invalid sd");
        return -1;
    }

    int rc = connect(sd, (struct sockaddr*)&sin, sizeof(sin));
    if (rc < 0) {
        printf("bad connnect");
        close(sd);
        return -1;
    }

    return sd;
}

// TODO refoactor later
int send_to_server(char** args, struct shell_state* shell)
{
    printf("%s\n", args[0]);

    int socket;
    if (!shell->KEEP_ALIVE) {
        socket = connect_by_port_id(shell->HOST, shell->PORT);
    } else {
        socket = shell->SOCKET;
    }

    if (socket == -1) {
        perror("bad socket");
        return 0;
    }

     

    int pid = -1;
    if (strcmp(args[0], "&") == 0) {
        args++;
        pid = fork();
    }

    char* req = "TRACE HTTP/1.0 Host: http://osiris.ubishops.ca\n"; // TODO turn args to a string

    if (pid <= 0) {
        printf("Socket: %d\n", socket);
        printf("Sending request");
        send(socket, req, strlen(req), 0);
        const int length = 1024;
        char ans[1024];
        int n = recv(socket, &ans, 1024, 0);
        printf("%d \n", n);
        printf("%s \n", ans);
        if (!shell->KEEP_ALIVE)
            close(socket);
    }

    if (pid == 0) {
        printf("Background task done!\n");
        exit(EXIT_SUCCESS);
    }

    return 1;
}

char** split(char* line)
{
    int bufsize = 64;
    int position = 0;
    char** tokarr = malloc(sizeof(char*) * bufsize);
    char* token;
    char* delimeters = " \t\r\n\a";

    if (!tokarr) {
        perror("[ERROR] (malloc)");
        exit(EXIT_FAILURE);
    }

    // if (line[0] == '&') {
    //     tokarr[0] = "&";
    //     line += 2;
    //     tokarr[1] = line;
    //     return tokarr;
    // } else if (line[0] != '!') {
    //     tokarr[0] = line;
    //     return tokarr;
    // }

    token = strtok(line, delimeters);
    while(token != NULL) {
        tokarr[position++] = token;
        token = strtok(NULL, delimeters);
    }

    tokarr[position] = NULL;
    return tokarr;
}

int setup(struct shell_state* shell)
{
    int file = open("shconfig", O_RDWR|O_CREAT|O_APPEND, 0666);
    if (file == -1) {
        perror("[ERROR] (file open)");
        return EXIT_FAILURE;
    }

    off_t buffsize = lseek(file, 0, SEEK_END);
    if (buffsize == -1) {
        perror("[ERROR] (file seek)");
        return EXIT_FAILURE;
    }
    
    if (lseek(file, 0, SEEK_SET) == -1) {
        perror("[ERROR] (file seek)");
        return EXIT_FAILURE;
    }

    char* buffer = malloc(sizeof(char) * buffsize);
    if (!buffer) {
        perror("[ERROR] (malloc)");
        exit(EXIT_FAILURE);
    }

    int count = read(file, buffer, buffsize);

    char** tokens = split(buffer);
    int i = 0;
    while (tokens[i] != NULL) {
        if (strcmp(tokens[i], "VSIZE") == 0) {
            shell->VSIZE = atoi(tokens[i+2]);
        } else if (strcmp(tokens[i], "HSIZE") == 0) {
            shell->HSIZE = atoi(tokens[i+2]);
        } else if (strcmp(tokens[i], "HOST") == 0) {
            shell->HOST = tokens[i+2];
        } else if (strcmp(tokens[i], "PORT") == 0) {
            shell->PORT = atoi(tokens[i+2]);
        }

        i++;
    }

    if (shell->HSIZE == -1) {
        char buffer[] = "HSIZE = 75\n";
        if (write(file, buffer, sizeof(buffer) - 1) == -1) {
            perror("[ERROR] (writing to file)");
            return EXIT_FAILURE;
        }
        shell->HSIZE = 75;
    }

    if (shell->VSIZE == -1) {
        char buffer[] = "VSIZE = 40\n";
        if (write(file, buffer, sizeof(buffer) - 1) == -1) {
            perror("[ERROR] (writing to file)");
            return EXIT_FAILURE;
        }
        shell->VSIZE = 40;
    }

    shell->KEEP_ALIVE = 0;

    free(buffer);
    free(tokens);
    close(file);

    return 0;
}

char* get_user_input()
{
    int bufsize = 1024;
    int position = 0;
    char* buffer = malloc(sizeof(char) * bufsize);
    if (!buffer) {
        perror("[ERROR] (malloc)");
        return NULL;
    }

    char c;

    while (1) {
        c = getchar();

        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        } else {
            buffer[position++] =  c;
        }
    }
}

void run_external(char** args)
{
    // Create default search paths
    char path1[128] = "/bin/";
    char path2[128] = "/usr/bin/";
    strcat(path1, args[0]);
    strcat(path2, args[0]);

    // Try every path, including an absolute path if default does not work
    if (execve(path1, args, NULL) == 0) {
    } else if (execve(path2, args, NULL) == 0) {
    } else if (execve(args[0], args, NULL) == 0) {
    } else { // if all paths fail
        perror("[ERROR] (Could not excute command)");
        exit(EXIT_FAILURE);
    }
}

int more_cmd(char* path, int v, int h)
{
    int file = open(path, O_RDONLY);
    if (file == -1) {
        perror("[ERROR] (file open)");
        return EXIT_FAILURE;
    }

    off_t buffsize = lseek(file, 0, SEEK_END);
    if (buffsize == -1) {
        perror("[ERROR] (file seek)");
        return EXIT_FAILURE;
    }
    
    if (lseek(file, 0, SEEK_SET) == -1) {
        perror("[ERROR] (file seek)");
        return EXIT_FAILURE;
    }

    char* buffer = malloc(sizeof(char) * buffsize);
    if (!buffer) {
        perror("[ERROR] (malloc)");
        return EXIT_FAILURE;
    }

    int count = read(file, buffer, buffsize);
    if (count == -1) {
        perror("[ERROR] (read file)");
        return EXIT_FAILURE;
    }

    int i = 0;
    while (1) {
        for (int j = 0; j < v; j++) {
            for (int k = 0; k < h; k++) {
                // if line has been read or the whole file has been read
                if (buffer[i] == '\n' || i >= count) {
                    break;
                }

                printf("%c", buffer[i]);
                i++;
            }
            
            // If we read the whole file.
            if (i >= count)
                break;

            // Flush remainder of the line if needed
            while (buffer[i++] != '\n');


            if (j < v - 1)
                printf("%c", '\n');
        }
        
        // char c = getchar(); TODO: this line gives a segfault for some reason
        // Solution to the bug by Dr. Brudda:
        char c[128];
        fgets(c, 127, stdin);
        if (c[0] != '\n' || i >= count) {
            close(file);
            free(buffer);
            break;
        }
    }

    return 1;
}

int run_cmd(char** args, struct shell_state* shell)
{
    if (strcmp(args[0], "!") != 0) {
        // run on server
        send_to_server(args, shell);
        return 0;
    }

    args++;

    int wait = 1;
    if (strcmp(args[0], "exit") == 0) {
        return 0;
    }

    if (strcmp(args[0], "more") == 0) {
        return more_cmd(args[1], shell->VSIZE, shell->HSIZE);
    }

    if (strcmp(args[0], "keepalive") == 0) {
        shell->KEEP_ALIVE = 1;
        shell->SOCKET = connect_by_port_id(shell->HOST, shell->PORT);
    }

    if (strcmp(args[0], "close") == 0) {
        shell->KEEP_ALIVE = 0;
        close(shell->SOCKET);
    }

    int pid = fork();
    int status;

    // If command starts with "&" fork
    if (strcmp(args[0], "&") == 0 && pid == 0) {
        args++;
        pid = fork();
        if (pid != 0) {
            waitpid(pid, &status, 0);
            printf("[ASYNC CMD DONE]\n");
            exit(EXIT_SUCCESS);
        }
    } else if (strcmp(args[0], "&") == 0) {
        wait = 0;
    }    

    if (pid == 0) {
        run_external(args);
    } else if (wait) {
        waitpid(pid, &status, 0);
    }

    return 1;
}

void shell_loop(struct shell_state* shell)
{
    char* command;
    char** args;
    int return_code = 1;

    do {
        printf("$> ");

        // Get line from user
        char* command = get_user_input();
        if (strcmp(command, "") == 0)
            continue;
        
        // Split line into arguments
        char** tokens = split(command);

        // Run the command
        return_code = run_cmd(tokens, shell);

        // Free stuff
        free(command);
        free(tokens);
    } while (return_code);
}

int main(int argc, char** argv)
{
    struct shell_state shell = {-1, -1};
    setup(&shell);

    char* msg = "hell world";
    // send_to_server(msg, &shell);
    shell_loop(&shell);

    return EXIT_SUCCESS;
}

