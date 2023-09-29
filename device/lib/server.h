/*
 * Source File: server.h
 * Author: Ed Alegrid
 * Copyright (c) 2022 Ed Alegrid <ealegrid@gmail.com>
 * GNU General Public License v3.0
 */
#pragma once
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <future>
#include <sys/socket.h>
#include <stdio.h>
#include "socketerror.h"

#define MAX_CONN        16
#define MAX_EVENTS      32
#define BUF_SIZE 		512

namespace Tcp {

using namespace std;

class Server
{
    int i, n, epfd, nfd;
    int sockfd, newsockfd, rv;
    uint16_t PORT; // or in_port_t PORT where in_port_t is equivalent to the type uint16_t as defined in <inttypes.h> .
    int listenF = false, ServerLoop = false;
    string IP;
    socklen_t clen;
    sockaddr_in server_addr{}, client_addr{}; // structure that specifies a transport address and port for the AF_INET address family
    struct epoll_event ev;
	struct epoll_event events[MAX_EVENTS];

    void epoll_ctl_add(int epfd, int fd, uint32_t events)
    {
	    struct epoll_event ev;
	    ev.events = events;
	    ev.data.fd = fd;
	    if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
		    perror("epoll_ctl()\n");
		    exit(1);
	    }
    }

    int setnonblocking(int sfd)
    {
	    if (fcntl(sfd, F_SETFD, fcntl(sfd, F_GETFD, 0) | O_NONBLOCK) == -1) {
		    return -1;
	    }
	    return 0;
    }
   
    int initSocket(const uint16_t &Port, const string Ip = "127.0.0.1")
    {
        PORT = Port;
        IP = Ip;
        port = Port;
        ip = Ip;

        try
        {
	        if (port <= 0){
	            throw SocketError("Invalid port");
	        }
	        sockfd =  socket(AF_INET, SOCK_STREAM, 0);

	        if (sockfd < 0) {
	            throw SocketError();
	        }

	        server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
	        server_addr.sin_port = htons(port);

	        int reuse = 1; //reuse socket
	        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));
          
	        if(bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
	            throw SocketError();
	        }
	        else
	        {
	            listen(sockfd, MAX_CONN);
                //epfd = epoll_create(1); // alternate api
                epfd = epoll_create1(0);
	            epoll_ctl_add(epfd, sockfd, EPOLLIN | EPOLLOUT | EPOLLET);
	            clen = sizeof(client_addr);
	        }
	        return 0;
        }
        catch (SocketError& e)
        {
	        cerr << "socket initialize error: " << e.what() << endl;
	        closeHandler();
            return 1;
        }
    }

    void closeHandler() const
    {
        end();
    }

    public:
        // use with createServer() method
        Server(){}
        // immediately initialize the server socket with the port provided
        Server(const uint16_t &port, const string ip = "127.0.0.1" ): PORT{port}, IP{ip} { initSocket(port, ip); }
        virtual ~Server() {} // use for polymorphism or class derivation // ok w/ or w/o
        //~Server() {} // basic 

        void createServer(const uint16_t &Port, const string Ip = "127.0.0.1")
        {
            initSocket(Port, Ip);
        }

        // server address property
        string ip;
        uint16_t port;

        //void Listen(int serverloop = false)
        void clientListen(int serverloop = true)
        {
            ServerLoop = serverloop;
            try
            {
                // using anonymous lambda function for async pattern
                auto l = [] (int fd, sockaddr_in client_addr, socklen_t clen)
                //auto l = [] (auto fd, auto client_addr, auto clen) // using auto in args requires -std=c++20 or -fconcepts flag during compilation 
                {
                  auto newfd = accept4(fd, (struct sockaddr *) &client_addr, &clen, SOCK_NONBLOCK);
                  
                  if (newfd < 0){ throw SocketError("Invalid socket descriptor! Listen flag is false! \nMaybe you want to set it to true like Listen(true).");}
                  return newfd;
                };

                newsockfd = async(l, sockfd, client_addr, clen).get();

                if (!listenF){
                  //cout << "Server listening on: " << IP << ":" << PORT << "\n\n";
                  listenF = true;
                }
                
                epoll_ctl_add(epfd, newsockfd, EPOLLIN | EPOLLET | EPOLLRDHUP | EPOLLHUP);
                //cout << "server connection from client " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << "\n\n"; 
            }
            catch (SocketError& e)
            {
                cerr << "listen error: " << e.what() << endl;
                closeHandler();
            }
        }

        void wait(int serverloop = true)  
        {
            clientListen(serverloop);
        }

        virtual const string readSync()
        {
            char buffer[BUF_SIZE];

            if(!listenF){
                throw SocketError("No listening socket!\n Did you forget to start the Listen() method!");
            }

            try
            {
                //nfd = epoll_wait(epfd, events, MAX_EVENTS, -1); // blocks next execution if no available data  
                nfd = epoll_wait(epfd, events, MAX_EVENTS, 1000); // wait 1 sec for next execution

                for (i = 0; i < nfd; i++) {
                    if (events[i].data.fd == newsockfd) {
                        if (events[i].events & EPOLLIN) {
                            ssize_t n = 1;
                            bzero(buffer, sizeof(buffer));
                            n = {recv(events[i].data.fd, buffer, sizeof(buffer), 0)}; 
                            //cout << "server read sync n = " << n << endl;
                            if (n <= 0 || errno == EAGAIN ) {
                                //cout << "no available data, read again\n" << endl;
                                break;
                            }
                            if (n == 0){
                                cout << "readSync error, socket is closed or disconnected\n";
                            }
                         }
                         else {
                            cout << "unexpected readSync error\n";
                         }
                         // check if the connection is closed
                         if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
                            cout << "readSync error, connection is closed!\n";
                            epoll_ctl(epfd, EPOLL_CTL_DEL,
                            events[i].data.fd, NULL);
                            close(events[i].data.fd);
                            continue;
                         }
                    }
                }
                if(nfd < 1){
                    cout << "readSync error: no available data\n" << endl;
                    bzero(buffer, sizeof(buffer));
                }
            }
            catch (SocketError& e)
            {
                cerr << "readSync error: " << e.what() << endl;
                closeHandler();
            }
            return buffer;
        }

        // read data asynchronously, use only after calling the Listen() method
        virtual const string read(const int bufsize=1024) 
        {
            if(!listenF){
                throw SocketError("No listening socket!\n Did you forget to start the Listen() method!");
            }

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
                        if(x == 100000){ break; }
                        x++;
                    }
                    //cout << "server read async n = " << n << " bytes, x = " << x << endl;
                    if (n <= 0 || errno == EAGAIN ){
                        cout << "read error: no available data\n" << endl;
                    }
                    if (n == 0){
                        cout << "read error, socket is closed or disconnected\n";
                    }
                    s = &buffer[0];  
                    return s; 
                };

                //nfd = epoll_wait(epfd, events, MAX_EVENTS, -1); // blocks next execution if no available data  
                nfd = epoll_wait(epfd, events, MAX_EVENTS, 1000); // wait 1 sec for next execution
                for (i = 0; i < nfd; i++) {
                    if (events[i].data.fd == newsockfd) {
                        if (events[i].events & EPOLLIN) {
                            ad = async(l, newsockfd).get();
                        }
                        else {
                            cout << "read unexpected error\n";
                        }
                        // check if the connection is closed
                        if (events[i].events & (EPOLLRDHUP | EPOLLHUP)) {
                            cout << "read error, connection is closed!\n";
                            epoll_ctl(epfd, EPOLL_CTL_DEL,
                            events[i].data.fd, NULL);
                            close(events[i].data.fd);
                            continue;
                        }
                    }
                }
                if(nfd < 1){ 
                    cout << "read error: no available data\n" << endl;
                } 
          }
          catch (SocketError& e)
          {
                cerr << "read error: " << e.what() << endl;
                closeHandler();
          }
          return ad;
        }

        virtual const string sendSync(const string &msg) const
        {
            if(!listenF){
                throw SocketError("No listening socket!\n Did you forget to start the Listen() method!");
            }

            try
            {
                ssize_t n{send(newsockfd, msg.c_str(), strlen(msg.c_str()), 0)};
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
            if(!listenF){
                throw SocketError("No listening socket!\n Did you forget to start the Listen() method!");
            }

            try
            {
                // using lambda expressions
                auto l = [] (const int fd, const string &msg)
                {
            	    ssize_t n{send(fd, msg.c_str(), strlen(msg.c_str()), 0)};
                    //cerr << "write n: " << n << endl;
            	    if (n < 0) {
              	        throw SocketError();
            	    }
                };
                async(l, newsockfd, msg).get();
            }
            catch (SocketError& e)
            {
                cerr << "Server write error: " << e.what() << endl;
                closeHandler();
            }
            return msg;
        }

        virtual void end() const
        {
            if(ServerLoop){
                close(newsockfd);
            }
            else{
                close(newsockfd);
                close(sockfd);
            }
        }
};

}

