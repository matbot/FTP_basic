Mathew McDade
CS 372.400
Summer 2019
FTP

Extra Credit Notes:
	a. Server login with multiple permission layers.
	b. Admin can change directories. Note: the server's directory resets after each user.
	c. Sweet client UI.

Preparing the programs:
	1. Enable execution of the python FTP client by modifying its permissions from the command line:
		$ chmod +x ftclient.py

	2. Compile the C FTP server, ftserver.c, by simply running the makefile from the command line:
		$ make
		which will output the ftserver executable.
	
	3. Done!

Running the programs:
	1. Start the FTP server first on serverhostname, with the syntax:
		$./ftserver serverportvalue
		where portnumber is an available, non-reserved port number on the host system, 1025 to 65535, inclusive--
		not 1024, because the data port will be opened on the control port - 1.
		The server is now waiting for a client to connect.

	2. Start the FTP client, with the syntax:
		$ftclient.py serverhostname serverportvalue
		where serverhostname and serverhostport are the hostname and port number of the previoulsy started chatserver.

	3. The server and client should now be connected.

	4. Log in to the server using the username and password combo:
		Username: admin
		Password: admin
		for Admin permission.

		Username: user
		Password: user
		for User permission.

		Username: [whatever]
		Password: [whatever]
		for Anonymous permission.

	5. You'll get a login confirmation from the server. Hit Enter to continue.

	6. You're now at the FTP command prompt. You can enter any command, but only certain ones will be allowed depending on your permission level.
		Admin: help, quit, ls, get [filename(required)], cd [directory].
		User: help, quit, ls, get [filename(required)].
		Anon: help, quit, ls.

		Note: You'll be informed if your current permission level isn't allowed to execute a give command with, "Permission denied."

	7. You can now enter a command.
		Note: if you've requested to "get" a file that is already in your current local directory:
			a. You'll be prompted whether you want to overwrite the file or not.
			b. Pick one.
			c. You'll get the new file or not, based on your choice.

	8. The server will respond to your command and the client will display a formatted output.

	9. Check out those sweet results. Hit Enter to continue.

	10. Rinse and Repeat.

	11. Use the "quit" command to close down the client.

	12. Feel free to start the client right back up using the same address and enjoy!

	13. Kill the server with SIGINT, Ctrl^c, to kill the server.
