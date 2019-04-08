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
#define MAX_COURSE_NAME 100
#define MAX_RATE_TEXT 1024
#define WELCOME_MESSAGE "Welcome! Please log in.\n"

//-----Globals-----


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
        perror("Error reading message\n");
        return 1;
    }
    return 0;
}

//-----Client Handling-----

int process_login(int conn_fd, char* users_path){
	char* line = 0;
	size_t len = 0;
	ssize_t read;
	unsigned int message_length;
	char header[MAX_CHARS_EXPECTED] = {0};
    char user_name[MAX_USER_INPUT] = {0}, password[MAX_USER_INPUT] = {0};
    char file_user[MAX_USER_INPUT] = {0}, file_password[MAX_USER_INPUT] = {0};
	char* login_status = "1";
    FILE* user_details;
    if (!(user_details = fopen(users_path, "r"))){
        perror("Error reading user file\n");
        return 1;
    }
	while (strcmp(login_status, "0")){
        if (read_from_client(conn_fd, user_name)){
            return 1;
        }
        if (read_from_client(conn_fd, password)){
            return 1;
        }
        while((read = getline(&line, &len, user_details)) != -1){
            if (sscanf(line, "%s    %s", file_user, file_password) != -1){
                if (!strcmp(file_user, user_name) && !strcmp(file_password, password)){
                    login_status = "0";
                }
            }
	    }
    }
    fclose(user_details);
    write_to_client(conn_fd, login_status);
}

int process_client(int conn_fd, char* users_path, char* dir_path){
    unsigned int C = 0;
    unsigned int total_bytes_passed = 0;
    char header[MAX_CHARS_EXPECTED] = {0};
    unsigned int message_length;
    char *user_name;
    //The header will tell the server/client how much bytes it should read
    unsigned int message_length = strlen(WELCOME_MESSAGE);
    sprintf(header, "%010u", message_length);
    if (write_header(conn_fd, message_length)){
    	perror("Error sending header");
    	return 1;
    }
    if (write_message(conn_fd, WELCOME_MESSAGE)){
    	perror("Error displaying welcome message\n");
    	return 1;
    }
    process_login(conn_fd, users_path);

	length = (unsigned int) strtol(header, NULL, 10);
	if (length < 2<<20)//Bitwise operation! 2<<20=2MB
		buff_size = length;
	else//We assume allocating 2MB is possible, if there is more data, we'll handle it in chunks
		buff_size = 2<<20;
	buffer = (char *) malloc(buff_size + 1);
	if (buffer == NULL){
		fprintf(stderr, "Error allocating memory\n");
		return 1;
	}
	else{
		memset(buffer, 0, buff_size + 1);
		//We can start reading and processing the bytes from the client
		while (total_bytes_passed < length){
			if (length - total_bytes_passed < buff_size)//If the remaining amount to send is less than the buffer size, update the maximum buffer space to be used
				buff_size = length - total_bytes_passed;
			if (read_message(conn_fd, buffer, buff_size)){
				perror("Error reading from client\n");
				return 1;
			}
			//We have a chunk of buff_size to process
			else
				C += find_printable(pcc_total, buffer, buff_size);
			total_bytes_passed += buff_size;
			//By now, a chunk of buff_size was processed by the server
		}
		//By now, all the data from the client has been processed
		memset(header, 0, MAX_CHARS_EXPECTED);
		sprintf(header, "%012u", C);
		write_header(conn_fd, header);
		return 0;
	}
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
    while (listen_fd != -1){
        conn_fd = accept(listen_fd, (struct sockaddr*) &peer_address, &address_size);
        if (conn_fd < 0){
            if ((errno != EWOULDBLOCK) && (errno != EAGAIN)){
                perror("Error connecting client\n");
            }
        }
        else{
            if (process_client(conn_fd, argv[1], argv[2])){
                fprintf(stderr, "Error processing client\n");
            }
			close(conn_fd);
        }
    }
    close(listen_fd);
    return 0;
}
