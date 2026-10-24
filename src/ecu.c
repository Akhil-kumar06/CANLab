#define _POSIX_C_SOURCE 200809L
#include "protocol.h"
#include <errno.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
static volatile sig_atomic_t running=1;
static void stop(int s) { (void)s; running=0; }
static uint64_t now_ms(void) {
 struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
 return (uint64_t)t.tv_sec*1000+(uint64_t)t.tv_nsec/1000000;
}
static int open_can(const char *name,bool receiver) {
 /* This learning project deliberately accepts only virtual interfaces. */
 if(strncmp(name,"vcan",4)!=0) { fprintf(stderr,"Use a vcan interface.\n"); return -1; }
 unsigned index=if_nametoindex(name);
 if(!index) { perror("CAN interface not found"); return -1; }
 int fd=socket(PF_CAN,SOCK_RAW,CAN_RAW);
 if(fd<0) { perror("socket"); return -1; }
 struct can_filter filters[2]={{ENGINE_ID,CAN_SFF_MASK|CAN_EFF_FLAG|CAN_RTR_FLAG},
                              {VEHICLE_ID,CAN_SFF_MASK|CAN_EFF_FLAG|CAN_RTR_FLAG}};
 if(setsockopt(fd,SOL_CAN_RAW,CAN_RAW_FILTER,receiver?filters:NULL,
               receiver?sizeof filters:0)<0) {perror("filters");close(fd);return -1;}
 struct sockaddr_can addr={0}; addr.can_family=AF_CAN; addr.can_ifindex=(int)index;
 if(bind(fd,(struct sockaddr*)&addr,sizeof addr)<0) {perror("bind");close(fd);return -1;}
 return fd;
}
static void report(const Node *e,const Node *v,uint64_t now,uint64_t start) {
 printf("{\"ms\":%llu,\"engine\":\"%s\",\"vehicle\":\"%s\",\"rpm\":%u,\"temp\":%u,\"speed\":%.1f,\"engine_rx\":%u,\"vehicle_rx\":%u,\"invalid\":%u,\"sequence_events\":%u,\"alarm\":%s}\n",
 (unsigned long long)(now-start),health(e,now),health(v,now),e->value,e->temperature,
 v->value/10.0,e->received,v->received,e->invalid+v->invalid,e->gaps+v->gaps,
 e->seen && !strcmp(health(e,now),"OK") && e->temperature>=110 ? "true":"false");
 fflush(stdout);
}
int main(int argc,char **argv) {
 if(argc!=3 || (strcmp(argv[1],"engine") && strcmp(argv[1],"vehicle") && strcmp(argv[1],"monitor"))) {
 fprintf(stderr,"Usage: %s engine|vehicle|monitor vcan0\n",argv[0]); return 2; }
 bool monitor=!strcmp(argv[1],"monitor");
 int fd=open_can(argv[2],monitor); if(fd<0)return 1;
 signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGPIPE,SIG_IGN);
 uint64_t start=now_ms(),next=start; unsigned step=0; uint8_t seq=0;
 Node engine={0},vehicle={0}; bool paused=false,corrupt=false,hot=false;
 bool input_open=true; char cmd[64]; size_t used=0; bool overflow=false;
 while(running) {
 uint64_t now=now_ms(); int wait=next>now?(int)(next-now):0;
 struct pollfd p={monitor?fd:(input_open?STDIN_FILENO:-1),POLLIN,0};
 int result=poll(&p,1,wait);
 if(result<0 && errno!=EINTR) {perror("poll");close(fd);return 1;}
 if(!running)break;
 if(p.revents & (POLLERR|POLLNVAL)) {fprintf(stderr,"Interface/pipe error\n");close(fd);return 1;}
 if(p.revents&POLLIN) {
  if(monitor) {
   struct can_frame f;
   ssize_t size=read(fd,&f,sizeof f);
   if(size==(ssize_t)sizeof f) {
    unsigned id=f.can_id & CAN_SFF_MASK;
    if(id==ENGINE_ID)protocol_accept(&engine,&f,ENGINE_ID,now_ms());
    else if(id==VEHICLE_ID)protocol_accept(&vehicle,&f,VEHICLE_ID,now_ms());
   } else if(size<0 && errno!=EINTR) {perror("read");close(fd);return 1;}
  } else {
   char buf[128]; ssize_t size=read(STDIN_FILENO,buf,sizeof buf);
   if(size==0)input_open=false;
   for(ssize_t i=0;i<size;i++) {
    if(buf[i]=='\n') {
     cmd[used]=0;
     if(!overflow) {
      if(!strcmp(cmd,"pause"))paused=true;
      else if(!strcmp(cmd,"normal")){paused=false;corrupt=false;hot=false;}
      else if(!strcmp(cmd,"corrupt")){paused=false;corrupt=true;}
      else if(!strcmp(cmd,"heat"))hot=true;
      else if(!strcmp(cmd,"quit"))running=0;
     }
     used=0;overflow=false;
    } else if(buf[i]!='\r') {
     if(used<sizeof cmd-1)cmd[used++]=buf[i]; else overflow=true;
    }
   }
  }
 }
 if(p.revents&POLLHUP) {if(monitor)break;input_open=false;}
 now=now_ms();
 if(now>=next) {
  if(monitor)report(&engine,&vehicle,now,start);
  else if(!paused) {
   bool e=!strcmp(argv[1],"engine");struct can_frame f;
   unsigned value=e?1000+(step%50)*60:((step%100)*10);
   encode(&f,e?ENGINE_ID:VEHICLE_ID,value,e?(hot?120:85):0,seq++);
   if(corrupt)f.data[4]^=0xFF;
   if(write(fd,&f,sizeof f)!=(ssize_t)sizeof f) {perror("CAN write");close(fd);return 1;}
   step++;
  }
  next=now+100; /* No catch-up burst after host scheduling delays. */
 }
 }
 close(fd);return 0;
}
