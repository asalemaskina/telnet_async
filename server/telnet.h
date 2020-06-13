#include<unistd.h>
#include<fcntl.h>

#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<netdb.h>
#include<arpa/inet.h>

#include<stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <iostream>
#include <event2/event-config.h>
#include <event2/event.h>
#include <event2/event_struct.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <vector>
#include <sys/types.h>
#include <sys/wait.h>

std::string cmd(std::string arg, std::string dir)
{
    std::string data;
    FILE * stream;
    const int max_buffer = 256;
    char buffer[max_buffer];
    chdir(dir.c_str());
    if (arg.length()){
    stream = popen(arg.c_str(), "r");
    if (stream) {
    while (!feof(stream))
    if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
    pclose(stream);
    }
    }
    return data;
}

class Socket{
public:
    Socket():fd_(-1){
    }
    explicit Socket(int fd):fd_(fd){
    }

    void create(){
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    }

    void bind(short port){
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr =0;
        addr.sin_port = ::htons(port);

        if(::bind(fd_,(struct sockaddr*)&addr, sizeof(addr))<0)
        {
                std::cout<<"bind :(\n" << strerror(errno);
                exit(0);
        }
    }

    void listen(int backlog = 2){
        if(::listen(fd_, backlog)<0)
        {
                std::cout<<"listen:(\n";
                exit(0);
        }
    }



    void close(){
        ::close(fd_);
    }

    int getFd() const {
        return fd_;
    }



    void setNonBlock(){
        int flag;
        if ((flag = ::fcntl(fd_, F_GETFL)) == -1){
            std::cout<<"bad\n";
        }
        flag |= O_NONBLOCK;
        if (::fcntl(fd_, F_SETFL, flag) == -1){
            std::cout<<"bad\n";
        }
    }


private:
    Socket(const Socket&);
    Socket& operator=(const Socket&);
private:
    int fd_;
};


typedef void(*EventCallback)(int, short, void*);


class EventBase{
public:

    EventBase(){
        evbase_ = event_base_new();
        if (evbase_ == NULL){
            std::cout<<"bad\n";
        }
    }

    ~EventBase(){
        event_base_free(evbase_);
    }
    inline operator struct event_base*()
       {
           return evbase_;
       }

       inline struct event_base* base()
       {
           return evbase_;
       }

    void loop(int flags=0){
        if (event_base_loop(evbase_, flags) != 0){
            std::cout<<"bad\n";
        }
    }

    void dispatch(){
        if (event_base_dispatch(evbase_) != 0){
            std::cout<<"bad in dispatch\n";
        }
    }



    void loopBreak(){
        if (event_base_loopbreak(evbase_) != 0){
            std::cout<<"bad\n";
        }
    }

    struct event_base* getEventBase() const{
        return evbase_;
    }

private:
    struct event_base* evbase_;
};


class Event{
public:
    Event(): ev_(NULL){

    }
    ~Event()
        {
            free();
        }
    Event(struct event_base* base, int fd,
            short what, EventCallback cb){
        ev_ = event_new(base, fd, what, cb, static_cast<void *>(base));
        if (ev_ == NULL){
            std::cout<<"bad in construct\n";
        }
    }


    void free()
        {
            if (ev_)
            {
                event_free(ev_);
            }
            ev_ = NULL;
        }
    void add(const struct timeval* tv){
        if (event_add(ev_, tv) != 0){
            std::cout<<"bad in add\n";
        }
    }

    struct event* getEvent(){
        return ev_;
    }
protected:
    Event(const EventBase& base, int fd,
                short what, EventCallback cb, void* arg){
            ev_ = event_new(base.getEventBase(), fd, what, cb, arg);
            if (ev_ == NULL){
                std::cout<<"bad\n";
            }
    }
private:
    struct event* ev_;
};


