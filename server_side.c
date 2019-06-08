#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>


#define MAX_CHARS_EXPECTED 11 // Used to transfer how many characters to read in header. An unsigned int can have at most 10 bytes + '\0' in string representation (10 digits)
#define MAX_CONNECTIONS 25
#define MAX_USER_INPUT 16
#define MAX_COURSE_NUMBER 5
#define MAX_COURSE_NAME 100
#define MAX_RATE_TEXT 1024
#define MAX_CMD_LENGTH 16
#define MAX_BROADCAST_MESSAGE 200


//-----Header Read/Write-----

int read_header(int conn_fd, char *header){
    int bytes_passed = 0;
    int current_pass = 0;
    while (bytes_passed < MAX_CHARS_EXPECTED){
        current_pass = read(conn_fd, header + bytes_passed, MAX_CHARS_EXPECTED - bytes_passed);
        if (current_pass <= 0){
            if (current_pass == 0){
                close(conn_fd);
            }
            return 1;
        }
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
        if (current_pass < 0){
            if (current_pass == 0){
                close(conn_fd);
            }
            return 1;
        }
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
    unsigned int bytes_passed = 0;
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
    snprintf(header, MAX_CHARS_EXPECTED, "%010u", message_length);
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
        // Sending -1 to client will indicate no file exists
        if (write_to_client(conn_fd, "-1")){
            return 1;
        }
	    return 0;
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
    // Sending 0 to client will indicate end of file
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

int process_login(int conn_fd, char* users_path, char* user_name, int* user_logged_in){
	char* line = 0;
	size_t len = 0;
	ssize_t read;
    char input_name[MAX_USER_INPUT] = {0}, input_password[MAX_USER_INPUT] = {0};
    char file_user[MAX_USER_INPUT] = {0}, file_password[MAX_USER_INPUT] = {0};
	char* login_status = "1";
    FILE* user_details;
    if (!(user_details = fopen(users_path, "r"))){
        perror("Error reading user file\n");
        return 1;
    }
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
                *user_logged_in = 1;
                for(unsigned int i = 0; i < strlen(input_name); i++){
                    user_name[i] = input_name[i];
                }
            }
        }
    }
    write_to_client(conn_fd, login_status); // Will tell client side if login was successfull or not
    free(line);
    fclose(user_details);
    return 0;
}

int print_courses(int conn_fd, char* course_list_path){
    return print_file_to_client(conn_fd, course_list_path);
}

int check_course_exists(char* input_number, char* course_list_path){
    char *file_line = NULL;
    char course_number[MAX_COURSE_NUMBER] = {0};
    size_t len = 0;
    ssize_t read;
    FILE* course_list;
    if (!(course_list = fopen(course_list_path, "r"))){
        // Course file hasn't been made - no course exists
        return 0;
    }
    while ((read = getline(&file_line, &len, course_list)) != -1){
        if (sscanf(file_line, "%[^:]", course_number) != -1){ // Regex that stops at the first : from the beggining of the line
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

int add_course(int conn_fd, char *course_list_path, char user_list[MAX_CONNECTIONS][MAX_USER_INPUT], int* users_logged_in, int *socket_list, int client_index){
    char course_number[MAX_COURSE_NUMBER];
    char course_name[MAX_COURSE_NAME];
    char *user_name = user_list[client_index];
    int i;
    FILE* course_list;
    if (read_from_client(conn_fd, course_number)){
        return 1;
    }
    if (read_from_client(conn_fd, course_name)){
        return 1;
    }
    if (check_course_exists(course_number, course_list_path) == 1){
        return write_to_client(conn_fd, "1"); // Client will print that course_number exists
    }
    if (!(course_list = fopen(course_list_path, "a"))){
        perror("Error opening file");
        return 1;
    }
    if (fprintf(course_list, "%s:\t%s\n", course_number, course_name) < 0){
        perror("Error writing to file");
        return 1;
    }
    if (fclose(course_list)){
        perror("Error closing file");
        return 1;
    }
    if (write_to_client(conn_fd, "0")){
        return 1;
    }
    for (i = 0; i < MAX_CONNECTIONS; i++){
        if ((users_logged_in[i] != 0) && (strcmp(user_list[i], user_name))){
            write_to_client(socket_list[i], "2");
        }
    }
    return 0;
}

int rate_course(int conn_fd, char* user_name, char* working_directory, char* course_list_path){
    char course_number[MAX_COURSE_NUMBER] = {0};
    char rating_value[4] = {0};
    char rating_text[MAX_RATE_TEXT] = {0};
    char file_name[2 + MAX_COURSE_NUMBER + strlen(working_directory)];
    memset(file_name, 0, 2 + MAX_COURSE_NUMBER + strlen(working_directory));
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
    if (check_course_exists(course_number, course_list_path) != 1){
        return write_to_client(conn_fd, "1"); // Client will print that course_number doesn't exists in database
    }
    strcat(strcat(file_name, working_directory), course_number);
    if (!(course_file = fopen(file_name, "a"))){
        perror("Error opening file");
        return 1;
    }
    if (fprintf(course_file, "%s:\t%s\t%s\n", user_name, rating_value, rating_text) < 0){
        perror("Error writing to file");
        return 1;
    }
    if (fclose(course_file)){
        perror("Error closing file");
        return 1;
    }
    return write_to_client(conn_fd, "0");
}

int get_rate(int conn_fd, char* working_directory, char* course_list_path){
    char course_number[MAX_COURSE_NUMBER] = {0};
    char file_name[2 + MAX_COURSE_NUMBER + strlen(working_directory)];
    memset(file_name, 0, 2 + MAX_COURSE_NUMBER + strlen(working_directory));
    if (read_from_client(conn_fd, course_number)){
        return 1;
    }
    if (check_course_exists(course_number, course_list_path) != 1){
        return write_to_client(conn_fd, "1"); // Client will print that course_number doesn't exists in database
    }
    if (write_to_client(conn_fd, "0")){
	return 1;
    }
    strcat(strcat(file_name, working_directory), course_number);
    return print_file_to_client(conn_fd, file_name);
}

int broadcast_message(int conn_fd, char user_list[MAX_CONNECTIONS][MAX_USER_INPUT], int* users_logged_in, int* socket_list, int client_index){
    int i;
    char *user_name = user_list[client_index];
    char message[MAX_BROADCAST_MESSAGE] = {0};
    if(read_from_client(conn_fd, message)){
        return 1;
    }
    printf("Broadcasting message: %s\n", message);
    for(i = 0; i < MAX_CONNECTIONS; i++){
        printf("Going through user %d\n", i);
        if ((users_logged_in[i] != 0) && (strcmp(user_list[i], user_name))){
            printf("Sending user %d\n", i);
            write_to_client(socket_list[i], "1");
            write_to_client(socket_list[i], user_name);
            write_to_client(socket_list[i], message);
        }
    }
    return 0;
}

int process_client(int conn_fd, char user_list[MAX_CONNECTIONS][MAX_USER_INPUT], int* users_logged_in, int* socket_list, int client_index, char* course_list_path, char* working_directory){
    char *user_name = user_list[client_index];
    char input_cmd[MAX_CMD_LENGTH] = {0};
    // user_name variable now has the currentley logged in user name
	read_from_client(conn_fd, input_cmd);
    if (!strcmp(input_cmd, "list_of_courses")){
        if (print_courses(conn_fd, course_list_path)){
            return 1;
        }
    }
    else if (!strcmp(input_cmd, "add_course")){
        if (add_course(conn_fd, course_list_path, user_list, users_logged_in, socket_list, client_index)){
            return 1;
        }
    }
    else if (!strcmp(input_cmd, "rate_course")){
        if (rate_course(conn_fd, user_name, working_directory, course_list_path)){
            return 1;
        }
    }
    else if (!strcmp(input_cmd, "get_rate")){
        if (get_rate(conn_fd, working_directory, course_list_path)){
            return 1;
        }
    }
    else if (!strcmp(input_cmd, "broadcast")){
        if (broadcast_message(conn_fd, user_list, users_logged_in, socket_list, client_index)){
            return 1;
        }
    }
	else if(!strcmp(input_cmd, "quit")){
        close(conn_fd);
        socket_list[client_index] = 0;
        users_logged_in[client_index] = 0;
        memset(user_list[client_index], 0, strlen(user_list[client_index]));
	}
    // If none of them is true, the command is illegal and client shouldn't send it
    return 0;
}

int main(int argc, char *argv[]){
    assert(argc >= 3);
    uint16_t port;
    if (argc > 3){
    	port = (unsigned short) strtoul(argv[3], NULL, 10);
    }
    else{
    	port = 1337;
    }
    char working_directory[strlen(argv[2]) + 2];

    int listen_fd = -1;
    int conn_fd = -1;

    int client_socket[MAX_CONNECTIONS] = {0};
    char user_names[MAX_CONNECTIONS][MAX_USER_INPUT];
    int users_logged_in[MAX_CONNECTIONS] = {0};
    memset(user_names, 0, MAX_CONNECTIONS * MAX_USER_INPUT * sizeof(user_names[0][0]));
    int i;
    int max_fd;
    int current_sd;
    fd_set read_fds;

    struct sockaddr_in server_address;
    struct sockaddr_in peer_address;

    socklen_t address_size = sizeof(struct sockaddr_in);
	
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error in socket()\n");        
        return 1;
    }
    memset(&server_address, 0, sizeof(server_address));
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
    memset(working_directory, 0, strlen(argv[2]) + 2);
    if (argv[2][strlen(argv[2]-1)] == '/'){
        strcpy(working_directory, argv[2]);
    }
    else{
        strcat(strcat(working_directory, argv[2]), "/");
    }
    char course_list_path[strlen(working_directory) + 2 + 11];
    memset(course_list_path, 0, strlen(working_directory) + 2 + 11);
    strcat(strcat(course_list_path, working_directory), "course_list");
    while (listen_fd != -1){
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds);
        max_fd = listen_fd;
        for (i = 0; i < MAX_CONNECTIONS; i++){
            current_sd = client_socket[i];
            if (current_sd > 0){
                printf("Registered socket number %d\n", i);
                FD_SET(current_sd, &read_fds);
            }
            if (current_sd > max_fd){
                printf("Changing %d to %d\n", max_fd, current_sd);
                max_fd = current_sd;
            }
        }
        printf("Trying to select, max = %d\n", max_fd);
        if(select(max_fd + 1, &read_fds, NULL, NULL, NULL) < 0){
            perror("Error while using select");
        }
        printf("Select succeed\n");
        if(FD_ISSET(listen_fd, &read_fds)){
            conn_fd = accept(listen_fd, (struct sockaddr*) &peer_address, &address_size);
            if (conn_fd < 0){
                perror("Error connecting client\n");
                continue;
            }
            for (i = 0; i < MAX_CONNECTIONS; i++){
                if (client_socket[i] == 0){
                    client_socket[i] = conn_fd;
                    // Notify client he has connected
                    if (write_to_client(conn_fd, "0")){
                        client_socket[i] = 0;
                    }
                    break;
                }
            }
        }
        else{
            for (i = 0; i < MAX_CONNECTIONS; i++){
                current_sd = client_socket[i];
                if (FD_ISSET(current_sd, &read_fds)){
                    printf("Reading from socket number %d\n", i);
                    if (users_logged_in[i] == 0){
                        printf("Processing login\n");
                        if(process_login(current_sd, argv[1], user_names[i], &(users_logged_in[i]))){
                            // Error processing client
                            close(current_sd);
                            client_socket[i] = 0;
                        }
                    }
                    else{
                        printf("Processing request\n");
                        if (process_client(current_sd, user_names, users_logged_in, client_socket, i, course_list_path, working_directory)){
                            close(current_sd);
                            client_socket[i] = 0;
                            users_logged_in[i] = 0;
                            memset(user_names[i], 0, strlen(user_names[i]));
                        }
                    }
                }
            }
        }
    }
    close(listen_fd);
    return 0;
}
