#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>


/*    The program operates as a sever, that binds to the provided port and waits for a connection in any address.
    When a connection has been made, the server reads a header of the amount of data the client is about to send,
    and start reading it. When the amount of data is transferred to the server, it calculates the amount of printable
    character it received in the stream, updates a global counter for all the printable characters it saw in all clients,
    and sends the client the amount of printable character it saw in the stream of data. Then, it continues to the next
    client in a serial order (please pay attention for the explanation above if it wasn't supposed to be implemented serialy).
    To stop the server operation, an interrupt signal should be sent, and the server will stop receiving new clients,
    finish handling the accepted clients, and print the counter of each printable character it saw while running, and shut down.
*/

#define MAX_CHARS_EXPECTED 11 // Used to transfer how many characters to read in header. An unsigned int can have at most 10 bytes + '\0' in string representation (10 digits)
#define MAX_CONNECTIONS 25
#define MAX_USER_INPUT 16
#define MAX_COURSE_NUMBER 5
#define MAX_COURSE_NAME 100
#define MAX_RATE_TEXT 1024
#define MAX_CMD_LENGTH 16


//-----Header Read/Write-----

int read_header(int conn_fd, char *header){
    int bytes_passed = 0;
    int current_pass = 0;
    while (bytes_passed < MAX_CHARS_EXPECTED){
        current_pass = read(conn_fd, header + bytes_passed, MAX_CHARS_EXPECTED - bytes_passed);
        if (current_pass <= 0)
            return 1;
        bytes_passed += current_pass;
    }
    header[bytes_passed] = '\0';
    return 0;
}


int write_header(int conn_fd, char* header){
    int bytes_passed = 0;
    int current_pass = 0;
    while (bytes_passed < MAX_CHARS_EXPECTED){
        current_pass = write(conn_fd, header + bytes_passed, MAX_CHARS_EXPECTED - bytes_passed);
        if (current_pass < 0)
            return 1;
        bytes_passed += current_pass;
    }
    return 0;
}

//-----Buffer Filler-----

int read_message(int conn_fd, char* buffer, unsigned int buff_size){
    unsigned int bytes_passed = 0;
    int current_pass = 0;
    while (bytes_passed < buff_size){
        current_pass = read(conn_fd, buffer + bytes_passed, buff_size - bytes_passed);
        if (current_pass <= 0)
            return 1;
        bytes_passed += current_pass;
    }
    buffer[bytes_passed] = '\0';
    return 0;
}

int write_message(int conn_fd, char* buffer, unsigned int buff_size){
    int bytes_passed = 0;
    int current_pass = 0;
    while (bytes_passed < buff_size){
        current_pass = write(conn_fd, buffer + bytes_passed, buff_size - bytes_passed);
        if (current_pass < 0)
            return 1;
        bytes_passed += current_pass;
    }
    return 0;
}

//-----Helper Functions-----

int read_from_client(int conn_fd, char* buffer){
    unsigned int message_length;
    char header[MAX_CHARS_EXPECTED] = {0};
    if (read_header(conn_fd, header)){
        perror("Error reading header from client\n");
        return 1;
    }
    message_length = strtoul(header, NULL, 10);
    if (read_message(conn_fd, buffer, message_length)){
        perror("Error reading message\n");
        return 1;
    }
    return 0;
}

int write_to_client(int conn_fd, char* buffer){
    unsigned int message_length = strlen(buffer);
    char header[MAX_CHARS_EXPECTED] = {0};
    sprintf(header, "%011u", message_length);
    if (write_header(conn_fd, header)){
        perror("Error writing header to client\n");
        return 1;
    }
    if (write_message(conn_fd, buffer, message_length)){
        perror("Error writing message\n");
        return 1;
    }
    return 0;
}

int print_file_to_client(int conn_fd, char* file_name){
    char *file_line = NULL;
    size_t len = 0;
    ssize_t read;
    FILE* output_file;
    if (!(output_file = fopen(file_name, "r"))){
        // Sending 0 to client will indicate end of file
        if (write_to_client(conn_fd, "0")){
            return 1;
        }
    }
    while ((read = getline(&file_line, &len, output_file)) != -1){
        if (write_to_client(conn_fd, file_line)){
            free(file_line);
            fclose(output_file);
            return 1;
        }
    }
    if (ferror(output_file)){
        perror("Error reading courses file");
        free(file_line);
        fclose(output_file);
        return 1;
    }
    if (write_to_client(conn_fd, "0")){
        free(file_line);
        fclose(output_file);
        return 1;
    }
    free(file_line);
    fclose(output_file);
    return 0;
}

//-----Client Handling-----

int process_login(int conn_fd, char* users_path, char* user_name){
	char* line = 0;
	size_t len = 0;
	ssize_t read;
	unsigned int message_length;
	char header[MAX_CHARS_EXPECTED] = {0};
    char input_name[MAX_USER_INPUT] = {0}, input_password[MAX_USER_INPUT] = {0};
    char file_user[MAX_USER_INPUT] = {0}, file_password[MAX_USER_INPUT] = {0};
	char* login_status = "1";
    FILE* user_details;
    if (!(user_details = fopen(users_path, "r"))){
        perror("Error reading user file\n");
        return 1;
    }
    write_to_client(conn_fd, "0");
	while (strcmp(login_status, "0")){
        rewind(user_details);
        if (read_from_client(conn_fd, input_name)){
            return 1;
        }
        if (read_from_client(conn_fd, input_password)){
            return 1;
        }
        while((read = getline(&line, &len, user_details)) != -1){
            if (sscanf(line, "%s\t%s", file_user, file_password) != -1){
                if (!strcmp(file_user, input_name) && !strcmp(file_password, input_password)){
                    login_status = "0";
                    user_name = input_name;
                }
            }
	    }
        write_to_client(conn_fd, login_status); // Will tell client side if login was successfull or not
    }
    fclose(user_details);
}

int print_courses(int conn_fd){
    return print_file_to_client(conn_fd, "./course_list");
}

int check_course_exists(char* input_number){
    char *file_line = NULL;
    char *course_number;
    size_t len = 0;
    ssize_t read;
    FILE* course_list;
    if (!(course_list = fopen("./course_list", "r"))){
        // Course file hasn't been made - no course exists
        return 0;
    }
    while ((read = getline(&file_line, &len, course_list)) != -1){
        if (sscanf(file_line, "%s", course_number) != -1){
            if (!strcmp(input_number, course_number)){ // Found course
                free(file_line);
                fclose(course_list);
                return 1;
            }
        }
    }
    if (ferror(course_list)){
        perror("Error reading courses file");
        free(file_line);
        fclose(course_list);
        return -1;
    }
    free(file_line);
    fclose(course_list);
    return 0;
}

int add_course(int conn_fd){
    char course_number[MAX_COURSE_NUMBER];
    char course_name[MAX_COURSE_NAME];
    FILE* course_list;
    if (read_from_client(conn_fd, course_number)){
        return 1;
    }
    if (read_from_client(conn_fd, course_name)){
        return 1;
    }
    if (check_course_exists(course_number) == 1){
        return write_to_client(conn_fd, "1"); // Client will print that course_number exists
    }
    if (!(course_list = fopen("./course_list", "a"))){
        perror("Error opening file");
        return 1;
    }
    if (fprintf(course_list, "%s:\t%s\n", course_number, course_name) < 0){
        perror("Error writing to file");
        return 1;
    }
    if (close(course_list)){
        perror("Error closing file");
        return 1;
    }
    return 0;
}

int rate_course(int conn_fd, char* user_name){
    char course_number[MAX_COURSE_NUMBER];
    char rating_value[4];
    char rating_text[MAX_RATE_TEXT];
    FILE* course_file;
    if (read_from_client(conn_fd, course_number)){
        return 1;
    }
    if (read_from_client(conn_fd, rating_value)){
        return 1;
    }
    if (read_from_client(conn_fd, rating_text)){
        return 1;
    }
    char file_name[2 + MAX_COURSE_NUMBER];
    strcat(strcat(file_name, "./"), course_number);
    if (!(course_file = fopen(file_name))){
        perror("Error opening file");
        return 1;
    }
    if (fprintf(course_file, "%s:\t%s\t%s", user_name, rating_value, rating_text) < 0){
        perror("Error writing to file");
        return 1;
    }
    if (fclose(course_file){
        perror("Error closing file");
        return 1;
    }
    return 0;
}

int get_rate(int conn_fd){
    char course_number[MAX_COURSE_NUMBER];
    char file_name[2 + MAX_COURSE_NUMBER];
    if (read_from_client(conn_fd, course_number)){
        return 1;
    }
    strcat(strcat(file_name, "./"), course_number);
    return print_file_to_client(conn_fd, file_name);
}

int process_client(int conn_fd, char* users_path){
    char user_name[MAX_USER_INPUT] = {0};
    char input_cmd[MAX_CMD_LENGTH] = {0};
    if (process_login(conn_fd, users_path, user_name)){
        return 1;
    }
    // user_name variable now has the currentley logged in user name
    while (strcmp(read_from_client(conn_fd, input_cmd), "quit")){
        if (strcmp(input_cmd, "list_of_courses")){
            if (print_courses(conn_fd)){
                return 1;
            }
        }
        else if (strcmp(input_cmd, "add_course")){
            if (add_course){
                return 1;
            }
        }
        else if (strcmp(input_cmd, "rate_course")){
            if (rate_course(conn_fd, user_name)){
                return 1;
            }
        }
        else if (strcmp(input_cmd, "get_rate")){
            if (get_rate(conn_fd)){
                return 1;
            }
        }
        // If none of them is true, the command is illegal and client shouldn't send it
    }
    // Connection will be closed in main (where it was opened), any dynamic allocations should be freed here
    return 0;
}

int main(int argc, char *argv[]){
    assert(argc >= 3);
    uint16_t port;
    if (argc > 3){
    	port = (unsigned short) strtoul(argv[1], NULL, 10);
    }
    else{
    	port = 1337;
    }

    int listen_fd = -1;
    int conn_fd = -1;
    int flags;

    struct sockaddr_in server_address;
    struct sockaddr_in peer_address;

    socklen_t address_size = sizeof(struct sockaddr_in);
	
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error in socket()\n");        
        return 1;
    }
    memset(&server_address, 0, sizeof(server_address));

    //Set listener socket to be non-blocking
    if ((flags = fcntl(listen_fd, F_GETFL)) < 0){
        perror("Error getting socket flags\n");
        return 1;
    }

    if ((fcntl(listen_fd, F_SETFL, flags | O_NONBLOCK)) < 0){
        perror("Error setting socket as non-blocking\n");
        return 1;
    }


    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*) &server_address, address_size) != 0){
        perror("Error in bind()\n");
        close(listen_fd);
        return 1;
    }
    if (listen(listen_fd, MAX_CONNECTIONS)){
        perror("Error on listen()\n");
        close(listen_fd);
        return 1;
    }
    if (chdir(argv[2])){ // Change current working directory to provided path to handle files as local
        perror("Error using given directory to server\n");
        close(listen_fd);
        return 1;
    }
    while (listen_fd != -1){
        conn_fd = accept(listen_fd, (struct sockaddr*) &peer_address, &address_size);
        if (conn_fd < 0){
            if ((errno != EWOULDBLOCK) && (errno != EAGAIN)){
                perror("Error connecting client\n");
            }
        }
        else{
            if (process_client(conn_fd, argv[1])){
                fprintf(stderr, "Error processing client\n");
            }
			close(conn_fd);
        }
    }
    close(listen_fd);
    return 0;
}
