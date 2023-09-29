/*
 * Source File: client.h
 * Author: Ed Alegrid
 * Copyright (c) 2022 Ed Alegrid <ealegrid@gmail.com>
 * GNU General Public License v3.0
 */
#pragma once
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/fcntl.h>
#include <future>
#include <sys/socket.h>
#include <arpa/inet.h> 
#include <netdb.h>
#include "socketerror.h"

#define BUF_SIZE  512

namespace Tcp {

using namespace std;

class Client
{
    int sockfd, rv; 
    char s[INET6_ADDRSTRLEN];

    void *get_addr(struct sockaddr *sa)
    {
        if (sa->sa_family == AF_INET) {
            return &(((struct sockaddr_in*)sa)->sin_addr);
        }
        return &(((struct sockaddr_in6*)sa)->sin6_addr);
    }

    int initSocket(int Port, string Ip)
    {
        ip = Ip;
        port = Port;
        string prt = to_string(Port);      

        addrinfo hints{};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        addrinfo *servinfo{};

        try{
            if (port <= 0){
                throw SocketError("Invalid port");
            }

            if ((rv = getaddrinfo(Ip.c_str(), prt.c_str(), &hints, &servinfo)) != 0) { 
                cout << "getaddrinfo: " << gai_strerror(rv) << endl;
                throw SocketError("Invalid address");
            }
            sockfd = {socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol)};
            int result{ connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen)};
            if (result < 0)
            {
                 //cout << "connection fail " << Ip << ":" << Port << endl;
                if (fcntl(sockfd, F_SETFL, O_NONBLOCK, O_ASYNC ) <= 0){
                    throw SocketError();
                }
            }
            else
            {
                // details of remote connected endpoint
                inet_ntop(servinfo->ai_family, get_addr((struct sockaddr *)servinfo->ai_addr), s, sizeof s);
                //cout << "Client connected to: " << s << ":" << port << endl; 
            }

            if (servinfo == nullptr) {
                throw SocketError("connect fail ...");
            }

            freeaddrinfo(servinfo);

            return 0;
        }
        catch (SocketError& e)
        {
            //cerr << "socket initialize error: " << e.what() << endl;
            cout << "Connection fail: " << Ip << ":" << Port << " " <<  e.what() << " " << fcntl(sockfd, F_SETFL) << endl;
            closeHandler();
            //exit(1);
            return 1;
        }
        return 0;
    }

    void closeHandler() const
    {
        end();
    }

    public:
    // use with connect() method
    Client() {}
    // immediately initialize the client socket with the port and ip provided
    Client(const int port, const string ip = "127.0.0.1")  {initSocket(port, ip);}
    virtual ~Client() {}

    // client server connection address property
    string ip;
    int port;

    void setNoBlock(){
        int n = 1;
        try{
            if (( n = {fcntl(sockfd, F_SETFL, O_NONBLOCK, O_ASYNC)}) < 0){
                throw SocketError();
            }
            //cout << "setNoBlock n = " << n << endl;
        }
        catch (SocketError& e)
        {
            cerr << "setNoBlock error: " << e.what() << endl;
            closeHandler();
        }  
    }

    virtual void serverConnect(const int port, const string ip = "127.0.0.1")
    {
        initSocket(port, ip);
    }

    virtual const string readSync()
    {
	    char buffer[1024];
        try
        {
            int x = 0;
            ssize_t n = 1;
            bzero(buffer, sizeof(buffer));
            while (( n = {recv(sockfd, buffer, sizeof(buffer), 0)} ) < 0) {  
                bzero(buffer, sizeof(buffer));
                if(x == 10000000){ break; } // wait 2 secs
                x++;
	        }
            //cout << "client read sync n = " << n << " bytes, x = " << x << endl; 
            if (n < 0) {
                cout << "readSync error: no available data\n" << endl;
            }
            if (n == 0){
                cout << "readSync error, socket is closed or disconnected\n";
            }        
        }
        catch (SocketError& e)
        {
            cerr << "readSync error: " << e.what() << endl;
            closeHandler();
        }
        return buffer;
    }

    virtual const string read(int bufsize=1024) 
    {

	    string ad;
        try
        { 
            auto l = [bufsize] (const int fd) 
            {
              string s;
              int x = 0;
              ssize_t n = 1;
              char buffer[BUF_SIZE];

              if(bufsize){
                *buffer = buffer[bufsize];
              } 
              
              bzero(buffer, sizeof(buffer));

              while (( n = {recv(fd, buffer, sizeof(buffer), 0)} ) < 0) {  
		        bzero(buffer, sizeof(buffer));
                //if (n < 0) { break; }
                if(x == 10000000){ break; } // wait 2 secs
                x++;
	          }
              //cout << "client read async n = " << n << " bytes, x = " << x << endl;
              if (n < 0) {
                cout << "read error: no available data\n" << endl;
              }
              if (n == 0){
                cout << "read error, socket is closed or disconnected\n";
              }
              s = &buffer[0];  
              return s; 
            };

            if (fcntl(sockfd, F_GETFL) < 0 && errno == EBADF) {
               throw SocketError();
            }

            ad = async(l, sockfd).get();
        }
        catch (SocketError& e)
        {
            cerr << "read error: " << e.what() << endl;
            closeHandler();
        }
        return ad;
    }

    virtual string sendSync(const string msg) const
    {
	    try
	    {
            int n = send(sockfd, msg.c_str(), strlen(msg.c_str()), 0);
	        if (n < 0) {
	            throw SocketError();
	        }
	    }
	    catch (SocketError& e)
	    {
	        cerr << "sendSync error: " << e.what() << endl;
	        closeHandler();
	    }
	    return msg;
    }

    virtual const string write(const string &msg) const
    {

   	    try
	    {
	        auto l = [] (const int fd, const string &msg)
	        {
	            // cout << "client send async using lambda function." << endl; 
	            ssize_t n{send(fd, msg.c_str(), strlen(msg.c_str()), 0)};
	            //if (n < 0) { throw SocketError(); }
                return n;
	        };

            if (fcntl(sockfd, F_GETFL) < 0 && errno == EBADF) {
                throw SocketError();
            }

            auto n = async(l, sockfd, msg).get();
            if (n < 0) {
	            throw SocketError();
	        }
	    }
	    catch (SocketError& e)
	    {
            cerr << "write error: " << e.what() << endl;
	        closeHandler();
	    }
	    return msg;
    }

    virtual void end() const
    {
        close(sockfd);
    }
};

}

