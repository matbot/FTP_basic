#!/usr/bin/python3
# -*- coding: utf-8 -*-
"""
Mathew McDade
CS 372.400
Summer 2019
Last Updated: 8/12/2019
Project 2: FTP
ftclient: ftclient is the client side program for a simple FTP program meant to pair with ftserver running on a remote host. ftclient is written in
Python3, with occasional Python2isms, and represents my own creative work, except where otherwise noted. This program deviates from the example
execution, but not the program requirements, in a very significant, but I believe positive, way by providing a structured UI interface for all FTP
activities, while hiding some of the sausage making apparent in a program where everything is passed from the shell prompt. These advanced features 
include a user login with different permission levels, a simple help library, remote directory change ability, UI command entry with server side 
command validation, and structured output for various commands. I've attempted to comment gratuitously where I think anything might be unclear.
FTP structure based on: https://en.wikipedia.org/wiki/File_Transfer_Protocol
I hope you enjoy it! 
"""

import socket
from sys import argv
from os import path		#to check for file existence before writing a file received from the server.
from time import sleep	#for some UI smoothness in one spot in serverLogin that's probably not needed.

"""
validateArgs: simple function to validate the command line arguments in argv and exits status 1 if they aren't usable.
Also validates that the port number passed to the program falls in the non-reserved range--with the caveat that the lower end of the range is 1025
instead of 1024. The reason for this is that later the data socket will connect on a port that is the control port - 1, like real FTP!
"""
def validateArgs(argv):
	if (len(argv)!= 3):
		print("USAGE: ftclient.py hostname portnumber\n")
		exit(1);
	if (int(argv[2])<1025 or int(argv[2])>65535):
		print("ERROR: portnumber must be in the range 1025 to 65535, inclusive.\n")
		exit(1);
	return;

"""
intializeControlSocket: takes the command line arguments in argv and uses then to build and connect the control socket to the ftserver port.
Returns the controlSocket file descriptor for use by other functions.
"""
def initializeControlSocket(argv):
	#Convert args to ints.
	serverHost = argv[1]
	serverPort = int(argv[2])
	#Build control socket.
	controlSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	#Set socket options, namely socket reuse, so important here.
	controlSocket.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
	#Connect to server on control socket.
	controlSocket.connect((serverHost,serverPort))
	return controlSocket;

"""
serverLogin: login function to establish the client's connection to the server. Spells out the valid login options for username and password just to
make things simple for program assessment. The username and password are sent to the server for validation and the server replies with a string
describing one of three baisc user types. The user type is returned for use in the command prompt. I've also included comments in the function that
describe some of the UI element choices I made.
"""
def serverLogin(controlSocket):
	#You'll see me clear the screen using the below escape sequence quite a few times. I think it give a nice UI effect with the lined dividers.
	print("\033[H\033[J")
	print("___________________________FTP_SERVER_LOGIN______________________________\n")
	print("For Admin privleges:\nUsername: admin\nPassword: admin")
	print("_________________________________________\n")
	print("For User privleges:\nUsername: user\nPassword: user")
	print("_________________________________________\n")
	print("For Anonymous privleges:\nUsername: [anything]\nPassword: [anything]")
	print("_________________________________________\n")
	#Get username and password.
	username = input("Username: ")
	password = input("Password: ")
	print("\n")
	#Send username and password.
	#Note that all sends have a newline appended. This is used on the server side as a recv stop character.
	controlSocket.send((username+'\n').encode())
	controlSocket.recv(2)
	controlSocket.send((password+'\n').encode())
	controlSocket.recv(2)
	#Receive user type.
	usertype = controlSocket.recv(6).decode()
	controlSocket.send(".\n".encode())		#You'll see this quite a bit. It's just a back and forth ping with the server.
	sleep(0.5)						#It's probably a waste of an import, but I like the slight pause before the logged in message.
	print("Logged in as {}.".format(usertype))
	#I like using a user return key to continue the application as opposed to an arbitrary timer, and it also prevents input overflow.
	input("\n\n\n\n\nPress Enter to continue...")
	return usertype;

"""
getUserCmnd: simply gets the user's command--using the usertype argument as a prompt--including any arguments, and returns it/them.
"""
def getUserCmnd(usertype):
	print("\033[H\033[J")
	print("_________________________FTP_SERVER_COMMANDER____________________________\n")
	print("Enter a command:")
	print("_________________________________________\n")
	#A list of available commands for each permission level.
	print("Admin: ls, get [filename], cd [directoryname], help, quit")
	print("_________________________________________\n")
	print("User: ls, get [filename], help, quit")
	print("_________________________________________\n")
	print("Anon: ls, help, quit")
	print("_________________________________________\n")
	#A clever little application of list-map to handle multiple cmnd-arg scenarios.
	commandString = list(map(str, input("{}: ".format(usertype)).split()))
	if len(commandString) > 1:
		cmnd = commandString[0]
		arg = commandString[1]
	else:
		cmnd = commandString[0]
		arg = "NULL"
	print("\n")
	return cmnd, arg;

"""
sendCmnd: takes the previously acquired cmnd and arg and sends them to the server. Receives a reply from the server indicating that the command-arg
is valid or invalid. If invalid, the client receives a message from the server indicating what was wrong with the command-arg. The function returns a
1 or a 0, indicating that the command is valid or invalid, respectively.
"""
def sendCmnd(cmnd,arg):
	controlSocket.send((cmnd+'\n').encode())
	controlSocket.recv(2)
	controlSocket.send((arg+'\n').encode())
	controlSocket.recv(2)
	confirmation = controlSocket.recv(8).decode()
	controlSocket.send(".\n".encode())
	if (confirmation=="invalid"):
		invalidMessage = controlSocket.recv(50).decode()
		controlSocket.send(".\n".encode())
		print(invalidMessage)
		input("\n\nPress Enter to continue...")
		return 0;
	return 1;

"""
initializeDataSocket: builds and returns a listening socket for data transfer. Sends the listening port number to the server.
Note that this socket is set up like a server socket, just like real FTP!
"""
def initializeDataSocket(controlSocket,controlPort):
	listenSocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	listenSocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR,1)
	dataPort = controlPort-1
	listenSocket.bind(('',dataPort))
	#Send the listening port to the server for server-side socket construction.
	controlSocket.send((str(dataPort)+'\n').encode())
	controlSocket.recv(2)
	listenSocket.listen(1)
	#Send a message to the server to indicate that the data socket is built and listening, so try to connect.
	controlSocket.send("ready\n".encode())
	controlSocket.recv(2)
	return listenSocket;

"""
processCmnd: Takes the user's command, the control socket(for messaging), and the built and listening data socket. Connects to the data socket and
process the command, primarily by receiving the appropriate data from the server and displaying it to the client user in a nice format.
"""
def processCmnd(cmnd,controlSocket,listenSocket):
	dataSocket = listenSocket.accept()
	dataSocket = dataSocket[0]
	if (cmnd=="ls"):
		print("____________________FOLDER_CONTENTS___________________\n")
		#First, get the number of file contents the server is going to send.
		fileCount = dataSocket.recv(5).decode()
		fileCount = int(fileCount)
		dataSocket.send(".\n".encode())
		for x in range(fileCount):
			fileName = dataSocket.recv(25).decode()
			dataSocket.send(".\n".encode())
			print(fileName)
		print("_____________________END_OF_FOLDER____________________\n")
	#This section is structured a bit redundantly, but I think it helps a bit for clarity overall for a complex section.	
	elif (cmnd=="get"):
		print("__________________...Getting File...__________________\n")
		#Use path function from os module to check if the file already exists.
		if (path.isfile(arg)):
			#Give the user the option to overwrite the file or not.
			overwrite = input("Do you want to overwrite the existing file? ").lower()
			if (overwrite=="yes" or overwrite=="y"):
				file = open(arg,'w')
				print("\nOverwriting file...\n")
				buffer = ""
				while(True):
					buffer = dataSocket.recv(1024).decode()
					if ("!:!" in buffer):
						file.write(buffer[:-3])
						file.close()
						dataSocket.send(".\n".encode())
						break;
					else:
						file.write(buffer)
			else:
				#TODO: I could definitely modify this so that I don't receive the file just to waste it.
				print("File not overwritten.\n")
				buffer = ""
				while("!:!" not in buffer):
					buffer = dataSocket.recv(1024).decode()
				dataSocket.send(".\n".encode())
		else:
			file = open(arg,'w')
			buffer = ""
			while(True):
				buffer = dataSocket.recv(1024).decode()
				if ("!:!" in buffer):
					file.write(buffer[:-3])
					file.close()
					dataSocket.send(".\n".encode())
					break;
				else:
					file.write(buffer)
		print("____________________...File Got...____________________\n")
	
	elif (cmnd=="cd"):
		#The server changes the directory, then sends back that directory's path, which I display to the user.
		print("_______________...Changing Directory..._______________\n")
		newDirectory = dataSocket.recv(50).decode()
		dataSocket.send(".".encode())
		print(newDirectory+"\n")
		print("_______________...Directory Changed...________________\n")
	#Close up those sockets.
	listenSocket.close()
	dataSocket.close()
	input("Press Enter to continue...")
	return;

"""
printHelp: simply prints out a nice little help menu that briefly describes the permission levels and what each command does.
"""
def printHelp():
	print("\033[H\033[J")
	print("___________________________FTP_SERVER_HELP______________________________\n")
	print("All users authorized:")
	print("help: here you are!")
	print("quit: disconnect from the server and exit.")
	print("ls: ls will return the contents of the current working directory from the ftp server.\n")
	print("_________________________________________\n")
	print("Admin and User only:")
	print("get filename: retrieves the file, specified by filename, from the server and saves it locally.") 
	print("	You will be prompted to approve an overwrite if the file already exists locally.")
	print("_________________________________________\n")
	print("Admin only:")
	print("cd directoryname: will change the remote working directory to the one specified by directoryname.")
	print("	Will change to the remote HOME directory if no argument is specified.")
	print("_________________________________________\n")
	input("Press Enter to exit the help menu...")
	return;

#MAIN
"""
main: Not much is done directly in main. It primarily serves to coordinate the essential functions.
"""
if __name__ == "__main__":
	#Validate cmnd line args.
	validateArgs(argv)

	#Initiate and connect to server on control socket.
	controlSocket = initializeControlSocket(argv)

	#Log in to the server.
	userType = serverLogin(controlSocket)
	
	#Begin the command loop.
	while True:
		#Get and Validate user command and argument..
		cmnd, arg = getUserCmnd(userType)

		#Take care of a help call before doing anything else, because we don't need to do any processing for that.
		if (cmnd=="help"):
			printHelp()
			#Set validCmnd to False so we don't waste time with most of the processing.
			validCmnd = False
		else:
			#Send user command to the server.
			validCmnd = sendCmnd(cmnd,arg)
		#Close up shop and clear the screen.
		if (cmnd=="quit"):
			print("Disconnecting from server and quitting.\n")
			controlSocket.close()
			print("\033[H\033[J")
			exit(0);
		#If the command is valid, dig into the socket building and processing functions.
		if (validCmnd):
			listenSocket = initializeDataSocket(controlSocket,int(argv[2]))
			processCmnd(cmnd,controlSocket,listenSocket)

	#Close control socket--probably not needed here as the loop shouldn't break unless it is by exit.
	controlSocket.close()
