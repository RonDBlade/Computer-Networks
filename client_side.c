#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* 
    The program connects to the provided server with the provided port, and generates
    the provided amount of random characters from /dev/urandom. Afterwards, a header indicating the
    amount of characters it's going to send to the server is sent, and the characters are being
    transmitted to the server using TCP connection. At the end of the transmission, the program
    recives the amount of printable characters that were in the stream of characters it sent to the server,
    and it prints the total number it recived.

    For printing user details:
        strcat(strcat(strcat(login_message, "Hi "), user_name), ", good to see you.\n");
        strcat(login_message, "Failed to login.\n");

*/

#define MAX_CHARS_EXPECTED 11//An unsigned int can have at most 12 bytes in string representation (12 digits)

//-----Random Number Generator (RNG)

int generate_random_buffer(int randfd, char* rand_buffer, unsigned int buff_size){
    unsigned int bytes_passed = 0;
    int current_pass = 0;
    while (bytes_passed < buff_size){
        current_pass = read(randfd, rand_buffer + bytes_passed, buff_size - bytes_passed);
        if (current_pass <= 0)//Reading from urandom shouldn't return 0
            return 1;
        bytes_passed += current_pass;
    }
    return 0;
}

//-----Buffer Sender-----

int send_chunk(int sockfd, char* rand_buffer, unsigned int buff_size){
    unsigned int bytes_passed = 0;
    int current_pass = 0;
    while (bytes_passed < buff_size){
        current_pass = write(sockfd, rand_buffer + bytes_passed, buff_size - bytes_passed);
        if (current_pass < 0)
            return 1;
        bytes_passed += current_pass;
    }
    return 0;
}

//-----Server Handling-----

int send_to_server(unsigned int length, unsigned int buff_size, int randfd, char * rand_buffer, int sockfd){
    unsigned int total_bytes_passed = 0;
    while (total_bytes_passed < length){
        if (length - total_bytes_passed < buff_size)//If the remaining amount to send is less than the buffer size, update the maximum buffer space to be used
            buff_size = length - total_bytes_passed;
        //Read random numbers to buffer and send from buffer so no data will be lost (read/write not full)
        if (generate_random_buffer(randfd, rand_buffer, buff_size)){
            perror("Error reading from /dev/urandom\n");
            return 1;
        }
        //The random buffer should be filled by buff_size random chars
        //We can now send it through the socket
        if (send_chunk(sockfd, rand_buffer, buff_size)){
            perror("Error writing to socket\n");
            return 1;
        }
        total_bytes_passed += buff_size;
        //By now, a chunk of buff_size was passed to the server for work
    }
    return 0;
}

//-----Header Read/Write-----

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

int read_header(int conn_fd, char *header){
    int bytes_passed = 0;
    int current_pass = 0;
    while (bytes_passed < MAX_CHARS_EXPECTED){ //Read whole 11 digit number of C
        current_pass = read(conn_fd, header + bytes_passed, MAX_CHARS_EXPECTED - bytes_passed);
        if (current_pass <= 0){
            perror("Error reading from socket);
            return 1;
        }
        bytes_passed += current_pass;
    }
    header[bytes_passed] = '\0';
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

int read_from_server(int conn_fd, char* buffer){
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

int write_to_server(int conn_fd, char* buffer){
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

int main(int argc, char *argv[]){
    assert(argc == 4);
    uint16_t port = (unsigned short) strtoul(argv[2], NULL, 10);
    unsigned int length = (unsigned int) strtol(argv[3], NULL, 10);
    unsigned int buff_size = 0;
    unsigned int C = 0;
    struct sockaddr_in server_address;

    char header[MAX_CHARS_EXPECTED+1] = {0};
    int sockfd = -1;
    int randfd = -1;
    if (length < 2<<20)//Bitwise operation! 2<<20=2MB
        buff_size = length;
    else//We assume allocating 2MB is possible, if there is more data, we'll handle it in chunks
        buff_size = 2<<20;
    char* rand_buffer = (char *) malloc(buff_size);
    if (rand_buffer == NULL){
        printf("Error allocating memory\n");
        return 1;
    }
    memset(rand_buffer, 0, buff_size);
    
    //Creating a TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error in socket()\n");
        free(rand_buffer);
        return 1;
    }

    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    //Converting input to network address
    if (!inet_aton(argv[1], &(server_address.sin_addr))){
        perror("Error in inet_aton()\n");
        free(rand_buffer);
        return 1;
    }
    //Connecting to server
    if (connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0){
        perror("Error in connect()\n");
        free(rand_buffer);
        return 1;
    }

    if ((randfd = open("/dev/urandom", O_RDONLY)) < 0){
        perror("Error in open()\n");
        free(rand_buffer);
        close(sockfd);
        return 1;
    }

    //Converting the length to a fixed char* to send to server total chars to read as header
    sprintf(header, "%012u", length);
    if (write_header(sockfd, header)){
        perror("Error writing to socket\n");
        free(rand_buffer);
        close(sockfd);
        close(randfd);
        return 1;
    }
    //By now, header sent to server, and the server knows how much to read
    if (send_to_server(length,  buff_size, randfd, rand_buffer, sockfd)){
        free(rand_buffer);
        close(sockfd);
        close(randfd);
        return 1;
    }
    //By now, length chars were transferred to server, wait for an answer
    free(rand_buffer);
    close(randfd);
    memset(header, 0 ,MAX_CHARS_EXPECTED + 1);//Reset length-to-string buffer so it can be used in a reversed way
    if (read_header(sockfd, header)){
        close(sockfd);
        return 1;
    }
    close(sockfd);
    C = strtoul(header, NULL, 10);
    printf("# of printable characters: %u\n", C);//Print the answer
    return 0;
}
