// $Id: cix.cpp,v 1.4 2016-05-09 16:01:56-07 - - $

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
using namespace std;

#include <libgen.h>
#include <sys/types.h>
#include <unistd.h>

#include "protocol.h"
#include "logstream.h"
#include "sockets.h"

logstream log (cout);
struct cix_exit: public exception {};

unordered_map<string,cix_command> command_map {
   {"exit", cix_command::EXIT},
   {"help", cix_command::HELP},
   {"ls"  , cix_command::LS  },
   {"put" , cix_command::PUT },
   {"get" , cix_command::GET },
   {"rm"  , cix_command::RM  }

};

void cix_help() {
   static const vector<string> help = {
      "exit         - Exit the program.  Equivalent to EOF.",
      "get filename - Copy remote file to local host.",
      "help         - Print help summary.",
      "ls           - List names of files on remote server.",
      "put filename - Copy local file to remote host.",
      "rm filename  - Remove file from remote server.",
   };
   for (const auto& line: help) cout << line << endl;
}

void cix_ls (client_socket& server) {
   cix_header header;
   header.command = cix_command::LS;
   log << "sending header " << header << endl;
   send_packet (server, &header, sizeof header);
   recv_packet (server, &header, sizeof header);
   log << "received header " << header << endl;
   if (header.command != cix_command::LSOUT) {
      log << "sent LS, server did not return LSOUT" << endl;
      log << "server returned " << header << endl;
   }else {
      char buffer[header.nbytes + 1];
      recv_packet (server, buffer, header.nbytes);
      log << "received " << header.nbytes << " bytes" << endl;
      buffer[header.nbytes] = '\0';
      cout << buffer;
   }
}
void cix_getfile(client_socket& server, string filename) {
	cix_header header;
	header.command = cix_command::GET;
	memset(header.filename, 0, FILENAME_SIZE);
	strcpy(header.filename, filename.c_str());
	header.filename[filename.length()] = '\0';

	log << "Sending header: " << header << endl;

	send_packet(server, &header, sizeof header);
	
	recv_packet(server, &header, sizeof header);

	if (header.command != cix_command::FILE) {
		log << "Error : file not returned" << endl;
		log << "server returned : " << header << endl;
	} else {
		log << "received header : " << header << endl;
		char buffer[header.nbytes + 1];
		recv_packet(server, buffer, header.nbytes);
		log << "received " << strlen(buffer) << " bytes" << endl;
		buffer[header.nbytes] = '\0';
		ofstream ofile(header.filename, ofstream::binary);
		unsigned int length = strlen(buffer);
		ofile.write(buffer, length);
		ofile.close();
	}
	
	
}
void cix_putfile(client_socket& server, string filename) {

	ifstream sfile(filename, ifstream::binary);

	//Error log if file DNE
	if (not sfile) {
		log << filename << " : " << strerror(errno) << endl;
		return;
	}
	else {
		sfile.seekg(0, sfile.end);
		int length = sfile.tellg();
		sfile.seekg(0, sfile.beg);

		char* buffer = new char[length];

		sfile.read(buffer, length);

		//Send error if file did not load properly into buffer
		if (sfile) {
			log << filename << " : loaded into buffer." << endl;
		} else {
			log << "Error : " << sfile.gcount() << " bytes loaded, when filesize is : " << length
				<< " : " << strerror(errno) << endl;
			return;
		}
		cix_header header;
		header.command = cix_command::PUT;
		memset(header.filename, 0, FILENAME_SIZE);
		strcpy(header.filename, filename.c_str());
		header.filename[filename.length()] = '\0';
		header.nbytes = length;

		log << "Sending header: " << header << endl;

		send_packet(server, &header, sizeof header);
		send_packet(server, buffer, length);

		log << "sent " << length << " bytes" << endl;

		recv_packet(server, &header, sizeof header);
		log << "received header : " << header << endl;
		if (header.command == cix_command::ACK) {
			log << "File tranfer complete." << endl;
		} else if (header.command == cix_command::NAK) {
			log << header.filename << " : " << strerror(errno) << endl;
		} else {
			log << "Incorrect header returned : " << header << endl;
		}
		delete[] buffer;
	}
}
void cix_removefile(client_socket& server, string filename) {
	cix_header header;
	header.command = cix_command::RM;
	memset(header.filename, 0, FILENAME_SIZE);
	strcpy(header.filename, filename.c_str());
	header.filename[filename.length()] = '\0';

	log << "Sending header: " << header << endl;

	send_packet(server, &header, sizeof header);
	recv_packet(server, &header, sizeof header);
	log << "Received header : " << header << endl;

	if (header.command == cix_command::ACK) {
		log << header.filename << " was removed successfully." << endl;
	} else if(header.command == cix_command::NAK){
		log << header.filename << " : " << strerror(errno) << endl;
	} else {
		log << "Incorrect header returned : " << header << endl;
	}
}


void usage() {
   cerr << "Usage: " << log.execname() << " [host] [port]" << endl;
   throw cix_exit();
}

int main (int argc, char** argv) {
   log.execname (basename (argv[0]));
   log << "starting" << endl;
   vector<string> args (&argv[1], &argv[argc]);
   if (args.size() > 2) usage();
   string host = get_cix_server_host (args, 0);
   in_port_t port = get_cix_server_port (args, 1);
   log << to_string (hostinfo()) << endl;
   try {
      log << "connecting to " << host << " port " << port << endl;
      client_socket server (host, port);
      log << "connected to " << to_string (server) << endl;
      for (;;) {
         string line;
         getline (cin, line);
         if (cin.eof()) throw cix_exit();
		 size_t pos = line.find(" ");
		 string command = line.substr(0, pos);
		 string filename = line.substr(pos + 1);

		 //file name error conditions
		 if (filename.find("/") != string::npos) {
			 log << "filename cannot contain '/' : " << filename << endl;
			 command = "";
		 }
		 
		 if (filename.length() >= FILENAME_SIZE) {
			 log << "filename can only be 58 characters : " << filename << " is " << filename.length() << " characters long " << endl;
			 command = "";
		 }

		 log << "command : '" << command << "' filename : '" << filename << "'" << endl;
         const auto& itor = command_map.find (command);
         cix_command cmd = itor == command_map.end()
                         ? cix_command::ERROR : itor->second;
         switch (cmd) {
            case cix_command::EXIT:
               throw cix_exit();
               break;
            case cix_command::HELP:
               cix_help();
               break;
            case cix_command::LS:
               cix_ls (server);
               break;
			case cix_command::GET:			//get file from server
				cix_getfile(server, filename);
			   break;
			case cix_command::RM:			//remove file from server
				cix_removefile(server, filename);
			   break;
			case cix_command::PUT:			//put file on server
				cix_putfile(server, filename);
			   break;
            default:
               log << line << ": invalid command" << endl;
               break;
         }
      }
   }catch (socket_error& error) {
      log << error.what() << endl;
   }catch (cix_exit& error) {
      log << "caught cix_exit" << endl;
   }
   log << "finishing" << endl;
   return 0;
}

