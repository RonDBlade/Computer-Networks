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

int read_from_server(int conn_fd, char* buffer){//USE THIS TO READ FROM SERVER.OTHERS ARE HELPER FUNCTIONS
    unsigned int message_length;
    char header[MAX_CHARS_EXPECTED] = {0};
    if (read_header(conn_fd, header)){
        perror("Error reading header from server\n");
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
        perror("Error writing header to server\n");
        return 1;
    }
    if (write_message(conn_fd, buffer, message_length)){
        perror("Error writing message\n");
        return 1;
    }
    return 0;
}


int main(int argc, char *argv[]){
    //starting with setting up variables depending on cmd input
	uint16_t port;
    char* buff1=NULL,token;
    char user[16],password[16];
    size_t len=0;
    int nread=0;
    char input[1046]//setting max size for input,using the longest command,which takes 11 to name it,1024 chars for rating text,max size of course is 4,3 for rate number,1 for \0,3 spaces
    char delimiter[5]=" \t\r\n";
	char* hostname;
	
    struct sockaddr_in server_address;
	struct hostent *he;
    int sockfd = -1;
	
    if(argc==1){
		hostname = "localhost";
		port=1337;
    }
    else if(arc==2)
		hostname = argv[2];
        port=1337;
    else if(arc==3){
		port = (unsigned short) strtoul(argv[2], NULL, 10);//might not be exactly what we need,idk what strtoul does exactly
		hostname = argv[2];
    }
    else{//if over 2 cmd args
        printf("Too many command line arguments\n");
        return 1;
    }

    //Creating a TCP socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        perror("Error in socket()\n");
        return 1;
    }
    
    //missing an argument of the IP?
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(port);
    //Converting input to network address
    if (!inet_aton(hostname, &(server_address.sin_addr))){
		if ((he = gethostbyname(hostname)) == NULL){
			perror("Error resolving hostname\n");
			return 1;
		}
		memcpy(&server.sin_addr, he->h_addr_list[0], he->h_length);
    }
    //Connecting to server
    if (connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0){
        perror("Error in connect()\n");
        return 1;
    }

    //NOW WE NEED TO DEAL WITH RECEIVING AND SENDING TO THE SERVER
    //FIRST DEAL WITH USER+PASS
    while(1){//keep going until the user gives us good user and pass.do break; when he does.
    getline(&buff1,&len,stdin);//getline from user.
        token=strtok(buff1,delimiter);
        if(strcmp(token,"User:")){
            printf("Invalid input,please enter username\n");
            continue;
        }
        token=strtok(NULL,delimiter);
        strcpy(user,token);//assume user length is 15 or less
        if(strtok(NULL,delimiter)!=NULL){//more than just "User:" and username
            printf("Invalid input,please enter username\n");
            continue;
        }
        //if we are here,then first line was OK
        getline(&buff1,&len,stdin);
        token=strtok(buff1,delimiter);
        if(strcmp(token,"Password:")){
            printf("Invalid input,please enter password\n");
            memset(user, 0 ,16);
            continue;
        }
        token=strtok(NULL,delimiter);
        strcpy(password,token);//assume user length is 15 or less
        if(strtok(NULL,delimiter)!=NULL){//more than just "Password:" and password
            printf("Invalid input,please enter password\n");
            continue;
        }
    }
    //THEN WHILE LOOP OF USERCOMMANDS TILL USER TYPES QUIT
    //Converting the length to a fixed char* to send to server total chars to read as header
    while(1){//when user inputs "quit",do break;CHANGE WHAT THAT IS INSIDE IT IS OLD WHAT THE FUCK IS GOING ON
	memset(input, 0 ,1046);
      	getline(&buff1,&len,stdin);
	token=strtok(buff1,delimiter);
	//now lets check what command he has entered
	if(strcmp(token,"list_of_courses")){
		if(strtok(NULL,delimiter)!=NULL){
			printf("Invalid input\n");
			continue;
		}
		//else send token to the server
	}
	else if(strcmp(token,"add_course")){
		//append things to input[] array and send to server
	}
	else if(strcmp(token,"rate_course")){
		//append things to input[] array and send to server
	}
	else if(strcmp(token,"get_rate")){
		//append things to input[] array and send to server
	}
	else if(strcmp(token,"quit")){
		if(strtok(NULL,delimiter)!=NULL){
			printf("Invalid input\n");
			continue;
		}
		//if we get here,then just close and clear everything,and return 0.
		close(sockfd);
		return 0;
	}
	else{
		printf("Invalid input\n");
		continue;
	}
    }
	//this is just here to make the program compile,closing will actually happen in the "quit" command
	close(sockfd);
        return 0;
}
