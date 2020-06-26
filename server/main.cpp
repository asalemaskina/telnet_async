#include "telnet.hpp"
#define MAX_LINE 16384

std::vector<std::string> strVec(60);

void readcb(struct bufferevent *bev, void *ctx)
{
    struct evbuffer *input, *output;
    std::string line;
    size_t n;
    input = bufferevent_get_input(bev);
    output = bufferevent_get_output(bev);

    std::string *dir1=static_cast<std::string*>(ctx);
    std::string dir = *dir1;
    int fd=std::atoi(dir.c_str());

    line = evbuffer_readln(input, &n, EVBUFFER_EOL_LF);
   // line.erase(line.length()-1,2);
    if ((line[0]=='c')&&(line[1]=='d')){
        line.erase(0,3);
        ::strVec[fd]=line;
        line="";
    }
    line=cmd(line, ::strVec[fd]);
    evbuffer_add(output, line.c_str(), line.length());
    evbuffer_add(output, "\n", 1);

    if (evbuffer_get_length(input) >= MAX_LINE) {

        while (evbuffer_get_length(input)) {
            int n = evbuffer_remove(input, (void *)line.c_str(), line.length());

            line=cmd(line,dir);
            evbuffer_add(output, line.c_str(), line.length());
        }
        evbuffer_add(output, "\n", 1);
    }
}

void errorcb(struct bufferevent *bev, short error, void *ctx)
{
    if (error & BEV_EVENT_EOF) {
        perror("connection closed");
    } else if (error & BEV_EVENT_ERROR) {
        perror("event error");
    }

    bufferevent_free(bev);
}


void do_accept(evutil_socket_t listener, short event, void *arg)
{
    struct event_base *base = static_cast<struct event_base *>(arg);
    intptr_t fd = ::accept(listener, NULL,NULL);
    if (fd < 0) {
        perror("accept");
    } else if (fd > FD_SETSIZE) {
        ::close(fd);
    } else {
        ::strVec[fd]="/root";
        struct bufferevent *bev;
        evutil_make_socket_nonblocking(fd);
        std::cout<<fd;
        bev = bufferevent_socket_new(base, fd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, readcb, NULL, errorcb,  static_cast<void*>(new std::string(std::to_string(fd))));
        bufferevent_setwatermark(bev, EV_READ, 0, MAX_LINE);
        bufferevent_enable(bev, EV_READ|EV_WRITE);
    }
}

void run(void)
{
    EventBase base;
    Socket sock;
    sock.create();
    sock.setNonBlock();
    sock.bind(3002);
    sock.listen(160);
    Event ev(base.getEventBase(), sock.getFd(), EV_READ|EV_PERSIST, do_accept);
    ev.add(NULL);
    base.dispatch();
}

int main()
{
    run();
    return 0;
}
