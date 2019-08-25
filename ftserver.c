/* Mathew McDade
 * CS 372.400
 * Summer 2019
 * Last Updated: 8/12/2019
 * Project 2: FTP
 * ftserver: ftserver is the server side program for a simple FTP program meant to pair with the ftclient running on a remote host. ftserver is
 * written in gnu99 compiled C, utilizing a variety of libraries, and representes my own creative work, except where otherwise noted. This program
 * deviates from the example execution, but not the program requirements in a significant, but I believe positive, way by servicing a continuous
 * UI-centric client. This requires primarily the ability to process multiple commands within a single execution cycle, while handling multiple data
 * socket connect-disconnect cycles and socket I/O as well as mutiple clients. The server also supports client command validation, user permission
 * levels, and the ability for the client to change the server's working directory.
 * FTP structure based on: https://en.wikipedia.org/wiki/File_Transfer_Protocol
 * I hope you enjoy it!
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>		//for handling directories.
#include <fcntl.h>		//for file handling.
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>	//for directory assessment.
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>	//to be able to access the in_addr structure.

//Basic error handling function to print any error status for bug diagnostics.
void error(const char *msg, int status) { perror(msg); exit(status); }

//Validates the command like args.
void validateArgs(int argc, char* argv[]) {
	if (argc != 2) error("USAGE: ftserver portnumber\n",1); // Check usage args.
	if (atoi(argv[1]) < 1025 || atoi(argv[1]) > 65535)
		error("ERROR: portnumber must be in the range 1025 to 65535, inclusive.",1);
	return;
}

//Facilitator function to pass a ping to the client--ping here not technically accurate, more just a back and forth trade with the client.
void sendPing(int socketFD) {
	char ping[2] = ".\0";
	send(socketFD,ping,strlen(ping),0);
}

//Facilitator function to get a ping from the client.
void getPing(int socketFD) {
	char ping[2];
	memset(ping,'\0',sizeof(ping));
	recv(socketFD,ping,sizeof(ping),0);
}

//Sends a message specified by the message argument on the socket specified by socketFD, and get a "ping" back.
void sendMessage(char* message, int socketFD) {
	int charsWritten;
	charsWritten = send(socketFD, message, strlen(message), 0);
	if (charsWritten < 0) error("ERROR writing to socket.\n",1);
	getPing(socketFD);
	return;
}

//Receives a message from the client on the socket specified by socketFD, and stuffs it into message, returned by reference.
int receiveMessage(char* message, int socketFD) {
	int charsRead = 0;
	//Client message is passed into a buffer before being put into the reference argument.
	char buffer[1024];
	memset(buffer,'\0',sizeof(buffer));
	//I really like this little do-while loop, for whatever reason.
	do {
		charsRead = recv(socketFD,buffer,1023,0);
		if (charsRead < 0) error("ERROR reading from socket.\n",1);
		//Check the socket is still connected.
		if (charsRead ==0) return 0;
		strcat(message,buffer);
		memset(buffer,'\0',sizeof(buffer));
	} while(strstr(message,"\n")==NULL);	//newline char is the end of transmit added by the client.
	char* at = strchr(message,'\n');		//now get rid of it.
	*at = '\0';
	sendPing(socketFD);
	return 1;
}

//Simply takes care of the boilerplate of building the server address structure, and returns the serverAddress by reference.
void buildServerAddress(struct sockaddr_in* serverAddress,char* portArg) {
	int portNumber;
	portNumber = atoi(portArg);
	memset((char*)serverAddress,'\0',sizeof(*serverAddress));
	serverAddress->sin_family = AF_INET;
	serverAddress->sin_port = htons(portNumber);
	serverAddress->sin_addr.s_addr = INADDR_ANY;
	return;
}

//Builds and binds the listening socket using the serverAddress argument and returns the listenSocket by reference.
//Notably sets the socket option for socket reuse, so important.
void buildListenSocket(struct sockaddr_in* serverAddress,int* listenSocket) {
	*listenSocket = socket(AF_INET,SOCK_STREAM,0);
	if (*listenSocket <= 0) error("ERROR opening listen socket.", 1);
	//Setting the socket option in C is kind of weird.
	int optval = 1;
	setsockopt(*listenSocket,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));
	if(bind(*listenSocket,(struct sockaddr*)serverAddress,sizeof(*serverAddress)) < 0) error("ERROR binding listen socket.", 1);
	return;
}

//Accept the control socket from the listening socket and pass it and the clientAddress information back by reference.
void acceptConnection(struct sockaddr_in* clientAddress,int* listenSocket,int* controlSocket) {
	socklen_t sizeofClientInfo = sizeof(*clientAddress);
	*controlSocket = accept(*listenSocket,(struct sockaddr*)clientAddress,&sizeofClientInfo);
	if (*controlSocket <= 0) error("ERROR accepting client connection.", 1);
	return;
}

//Validates the username and password received on the controlSocket argument and sends back a user description.
//Returns a integer value to indicate the user permission level on the server.
//Usernames are built in application here for simplicity. A more robust example would have these stored encrypted separately.
int validateUser(int controlSocket) {
	int userType;
	char user[25];
	memset(user,'\0',sizeof(user));
	char password[25];
	memset(password,'\0',sizeof(password));

	receiveMessage(user,controlSocket);
	receiveMessage(password,controlSocket);

	if (strcmp(user,"admin")==0 && strcmp(password,"admin")==0) {
		userType = 3;
		sendMessage("Admin",controlSocket);
	}
	else if (strcmp(user,"user")==0 && strcmp(password,"user")==0) {
		userType = 2;
		sendMessage("User",controlSocket);
	}
	else {
		userType = 1;
		sendMessage("Anon",controlSocket);
	}
	return userType;
}

//Just gets the command and argument from the client.
void getClientCmnd(char* cmnd,char* arg,int controlSocket) {
	receiveMessage(cmnd,controlSocket);
	receiveMessage(arg,controlSocket);
	return;
}

//The file argument for get function is taken, and the function returns 1 if the file exists and 0 if it doesn't.
int validateFile(char* file) {
	struct stat statBuffer;
	int statResult = stat(file,&statBuffer);
	if (statResult == 0 && S_ISREG(statBuffer.st_mode)) {
		return 1;
	}
	else {
		return 0;
	}
}

//The dir argument for get function is taken, and the function returns 1 if the directory exists and 0 if it doesn't.
int validateDir(char* dir) {
	struct stat statBuffer;
	int statResult = stat(dir,&statBuffer);
	if (statResult == 0 && S_ISDIR(statBuffer.st_mode)) {
		return 1;
	}
	//If the cd command had no argument, it will arrive as "NULL", but this is valid for a change to the HOME directory.
	else if (strcmp(dir,"NULL")==0) {
		return 1;
	}
	else {
		return 0;
	}
}

//This is a busy function. It takes the client's command, the command's argument, the user type(from validateUser), and the control socket as
//	arguments. The function then makes sure that the given user type has permission to perform the command and, if so, if the argument is valid for
//	the given command. The function sends a message to the client to tell it if the command-argument is confirmed or invalid, and if it's invalid, 
//	sends an explanation. The function then returns 1 or 0 to indicate the command-argument combo is valid or invalid, respectively.
int validateCmnd(char* cmnd,char* arg,int userType,int controlSocket) {
	if (strcmp(cmnd,"ls")==0) {
		//Everyone can ls, yay!
		sendMessage("confirm",controlSocket);
		return 1;
	}
	else if (strcmp(cmnd,"get")==0) {
		if (userType > 1) {
			int fileExists = validateFile(arg);
			if (fileExists) {
				sendMessage("confirm",controlSocket);
				return 1;
			}
			else {
				//File doesn't exist.
				sendMessage("invalid",controlSocket);
				sendMessage("File does not exist.",controlSocket);
				return 0;
			}
		}
		else {
			//Anonymous users don't have permission to get files.
			sendMessage("invalid",controlSocket);
			sendMessage("Permission denied.",controlSocket);
			return 0;
		}
	}
	else if (strcmp(cmnd,"cd")==0) {
		if (userType > 2) {
			int dirExists = validateDir(arg);
			if (dirExists) {
				sendMessage("confirm",controlSocket);
				return 1;
			}
			else {
				//Directory doesn't exist.
				sendMessage("invalid",controlSocket);
				sendMessage("Directory does not exist.",controlSocket);
				return 0;
			}
		}
		else {
			//Only admins have permission to change directory.
			sendMessage("invalid",controlSocket);
			sendMessage("Permission denied.",controlSocket);
			return 0;
		}
	}
	//Special case to quit.
	else if (strcmp(cmnd,"quit")==0) {
		return -1;
	}
	//If it's not ls, get, cd, or quit, it ain't nothin.
	else {
		sendMessage("invalid",controlSocket);
		sendMessage("Invalid command.",controlSocket);
		return 0;
	}
}

//Build an address structure based on the client's address and the data port value sent from the client, and the client address structure is passed
//back by reference.
void buildClientAddress(struct sockaddr_in* clientAddress,struct sockaddr_in* client,char* dataPort) {
	memset((char*)clientAddress,'\0',sizeof(*clientAddress));
	int portNumber = atoi(dataPort);
	clientAddress->sin_port = htons(portNumber);
	clientAddress->sin_family = AF_INET;
	memcpy((char*)&clientAddress->sin_addr,(char*)&client->sin_addr,sizeof(&client->sin_addr));
}

//Build the data socket and set the port reuse option.
void buildDataSocket(int* dataSocket) {
	*dataSocket = socket(AF_INET,SOCK_STREAM,0);
	if (*dataSocket < 0) error("ERROR opening data socket.\n",1);
	int optval = 1;
	setsockopt(*dataSocket,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval)); //port reuse.
}

//Connect to the data socket.
void connectDataSocket(struct sockaddr_in* clientAddress,int* dataSocket) {
	if (connect(*dataSocket,(struct sockaddr*)clientAddress,sizeof(*clientAddress)) < 0)
		error("ERROR connecting to client data socket.\n",1);
}

//Based on GNU library example: https://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister.html
//and: https://www.gnu.org/software/libc/manual/html_node/Simple-Directory-Lister-Mark-II.html#Simple-Directory-Lister-Mark-II
//to sort the directory before sending it to the client.
static int one(const struct dirent *notUsed) {
	return 1;
}
//Sends the contents of the current directory over the data socket.
void sendCurrentDirectoryContents(int dataSocket) {
	printf("Sending current directory contents...\n");
	struct dirent **dir;
	char fileCountString[10];
	int fileCount = scandir("./",&dir,one,alphasort);		//scandir returns the count of objects in the directory.
	sprintf(fileCountString,"%d",fileCount);
	sendMessage(fileCountString,dataSocket);
	for(int i=0;i<fileCount;i++) {
		sendMessage(dir[i]->d_name,dataSocket);
	}
}

//Send the file specified by the fileName argument over the data socket specified by dataSocket.
void sendFile(char* fileName, int dataSocket) {
	printf("Sending file...\n");
	int ifile = open(fileName,O_RDONLY);
	char fileBuffer[1024];
	memset(fileBuffer,'\0',sizeof(fileBuffer));
	//While there's something to read from the specified file, pack it up and send it to the client.
	//Really neat because it doesn't require loading the whole file into memory.
	while(read(ifile,fileBuffer,sizeof(fileBuffer)-1)) {
		send(dataSocket,fileBuffer,sizeof(fileBuffer),0);
		printf(fileBuffer);
		memset(fileBuffer,'\0',sizeof(fileBuffer));
	}
	close(ifile);
	sendMessage("!:!",dataSocket);	//this is the end of file special character set for the client.
}

//Changes the server's current working directory to the one specified by the targetDirectory argument.
//Sends the current working directory path to the client over the data socket, specified by the dataSocket argument.
void changeDirectory(char* targetDirectory,int dataSocket) {
	printf("Changing directory...\n");
	char swd[50];
	memset(swd,'\0',sizeof(swd));
	if(strcmp(targetDirectory,"NULL")==0) {
		chdir(getenv("HOME"));
	}
	else {
		chdir(targetDirectory);
	}
	getcwd(swd,sizeof(swd));
	sendMessage(swd,dataSocket);
}

//Coordinating function calls the data socket setup functions and connects to the client data socket.
//Routes the valid functions to their appropriate functions.
void processCmnd(char* cmnd,char* arg,struct sockaddr_in client,int controlSocket) {
	//Build data socket and connect to client.
	int dataSocket;
	char dataPort[10];
	memset(dataPort,'\0',sizeof(dataPort));
	struct sockaddr_in clientAddress;

	receiveMessage(dataPort,controlSocket);

	printf("Building data socket...\n");
	buildClientAddress(&clientAddress,&client,dataPort);
	buildDataSocket(&dataSocket);
	//sleep(2);
	char ready[6];
	receiveMessage(ready,controlSocket); //instead of sleep.

	connectDataSocket(&clientAddress,&dataSocket);
	printf("Data socket connected...\n");

	//Command parsing.
	if (strcmp(cmnd,"ls")==0) {
		sendCurrentDirectoryContents(dataSocket);
	}
	else if (strcmp(cmnd,"get")==0) {
		sendFile(arg,dataSocket);
	}
	else if (strcmp(cmnd,"cd")==0) {
		changeDirectory(arg,dataSocket);
	}
	//Gotta close that data socket.
	close(dataSocket);
}

//This main is a lot busier than I usually care for, but I hope it's clearly explained by contained comments.
//TODO: better modularization from main to functions.
int main(int argc, char* argv[])
{
	//Get the original directory position so that it can be reset for each client.
	char ftpDir[50];
	memset(ftpDir,'\0',sizeof(ftpDir));
	getcwd(ftpDir,sizeof(ftpDir));

	//Socket Variables
	int listenSocket, controlSocket;
	struct sockaddr_in serverAddress, clientAddress;

	//Validate command line arguments.
	validateArgs(argc,argv);

	//Build, bind, and listen on connection socket.
	printf("Building server socket...\n");
	buildServerAddress(&serverAddress,argv[1]);
	buildListenSocket(&serverAddress,&listenSocket);
	listen(listenSocket,1); // Listen on the desginated port; queue one client.
	printf("Listening...\n");

	//Begin server listen loop.
	while(1) {
		//Reset the ftp origin directory.
		chdir(ftpDir);
		printf("Waiting for a client connection...\n");
		//Accept connection from a client.
		acceptConnection(&clientAddress,&listenSocket,&controlSocket);
		printf("Client connection accepted...\n");
		printf("Control socket established...\n");

		//Validate a username and password.
		printf("Validating user...\n");
		int userType = validateUser(controlSocket);
		printf("User validated...\n");

		//Begin user command loop.
		while(1) {
			char cmnd[25], arg[25];
			memset(cmnd,'\0',sizeof(cmnd));
			memset(arg,'\0',sizeof(arg));
			//Get client's command, including any arguments.
			printf("Getting client command...\n");
			getClientCmnd(cmnd,arg,controlSocket);

			//Validate client's command, returning -1 if it's an invalid command.
			//TODO: which is dumb, it should return 0...
			printf("Validating client command...\n");
			int validCmnd = validateCmnd(cmnd,arg,userType,controlSocket);

			//Client command execution conditional on command validation.
			//I don't bother building the data port unless the command is valid and executable.
			if (validCmnd == -1) {	//Client quit.
				printf("Invalid command...\n");
				break;
			}
			else if (validCmnd) {
				printf("Command validated...\n");
				//Execute client's command, performing any necessary transmission over the data socket.
				printf("Processing client command...\n");
				processCmnd(cmnd,arg,clientAddress,controlSocket);
			}
		}
		//Disconnect from the client by closing the control socket.
		printf("Closing control socket...\n");
		close(controlSocket);
	}
	//Close the listening socket and shut down the server.
	printf("Closing listening socket...\n");
	close(listenSocket);
	return 0;
}
