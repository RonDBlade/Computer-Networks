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

int read_from_server(int conn_fd, char* buffer){//USE THIS TO READ FROM SERVER.OTHERS ARE HELPER FUNCTIONS
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

int write_to_server(int conn_fd, char* buffer){//USE THIS TO WRITE TO SERVER.OTHERS ARE HELPER FUNCTIONS
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

int moreThanOne(char* buffer,int len,int start){//checks if we have more than one space in the input
    int i;
    for(i=start+1;i<len;i++){
        if(buffer[i]==' ')
           return 1;
    }
    return 0;
}

int main(int argc, char *argv[]){
    //starting with setting up variables depending on cmd input
    uint16_t port;
    char* buff1=NULL;
    char tmparr[16],user[16],password[16];//
    struct sockaddr_in server_address;
    size_t len=0;
    int nread=0;
    if(argc==1){
       
       port=1337;
    }
    else if(arc==2)
        port=1337;
    else if(arc==3){
    uint16_t port = (unsigned short) strtoul(argv[2], NULL, 10);//might not be exactly what we need,idk what strtoul does exactly
    //line reserved for the hostname
    }
    else{//if over 2 cmd args
        printf("Too many command line arguments\n");
        return 1;
    }
    //which of these are still needed?
    unsigned int length = (unsigned int) strtol(argv[3], NULL, 10);
    unsigned int buff_size = 0;
    unsigned int C = 0;
    
    //DO WE STILL NEED THIS?
    char header[MAX_CHARS_EXPECTED+1] = {0};
    int sockfd = -1;
    int randfd = -1;
    
    //Creating a TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error in socket()\n");
        free(rand_buffer);
        return 1;
    }
    
    //missing an argument of the IP?
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
    
    
    //NOW WE NEED TO DEAL WITH RECEIVING AND SENDING TO THE SERVER
    //FIRST DEAL WITH USER+PASS
    while(1){//keep going until the user gives us good user and pass.do break; when he does.
    getline(&buff1,&len,stdin);//getline from user.
    strncpy(tmparr,buff1,5);
    if(strcmp(*tmparr,"User:")){
        printf("Please enter your username\n");
        continue;
    }
    if(buff1[5]!=' ' || moreThanOne(buff1,len,5)){
        printf("Invalid Input\n");
        continue;
    }
    strncpy(user,buff1+6,len-6);
    
    //THEN WHILE LOOP OF USERCOMMANDS TILL USER TYPES QUIT
    //Converting the length to a fixed char* to send to server total chars to read as header
    while(1){//when user inputs "quit",do break;CHANGE WHAT THAT IS INSIDE IT IS OLD WHAT THE FUCK IS GOING ON
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
}
