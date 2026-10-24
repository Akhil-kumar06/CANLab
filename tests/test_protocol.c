#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void) {
 Node n={0};struct can_frame f;CHECK(!strcmp(health(&n,0),"WAITING"));
 encode(&f,ENGINE_ID,3000,85,254);
 CHECK(f.data[0]==0xb8 && f.data[1]==0x0b);
 CHECK(protocol_accept(&n,&f,ENGINE_ID,100));CHECK(n.value==3000 && n.temperature==85);
 CHECK(!strcmp(health(&n,600),"OK"));CHECK(!strcmp(health(&n,601),"TIMEOUT"));
 f.data[4]^=1;CHECK(!protocol_accept(&n,&f,ENGINE_ID,700));CHECK(n.last_ms==100);
 encode(&f,ENGINE_ID,8001,85,255);CHECK(!protocol_accept(&n,&f,ENGINE_ID,800));
 encode(&f,ENGINE_ID,8000,150,255);CHECK(protocol_accept(&n,&f,ENGINE_ID,900));
 encode(&f,ENGINE_ID,2000,85,0);CHECK(protocol_accept(&n,&f,ENGINE_ID,1000));CHECK(n.gaps==0);
 CHECK(protocol_accept(&n,&f,ENGINE_ID,1100));CHECK(n.gaps==1);
 encode(&f,ENGINE_ID,2000,85,2);CHECK(protocol_accept(&n,&f,ENGINE_ID,1200));CHECK(n.gaps==2);
 f.can_dlc=4;CHECK(!protocol_accept(&n,&f,ENGINE_ID,1300));
 encode(&f,ENGINE_ID,2000,151,3);CHECK(!protocol_accept(&n,&f,ENGINE_ID,1400));
 encode(&f,ENGINE_ID,2000,85,3);f.can_id|=CAN_RTR_FLAG;CHECK(!protocol_accept(&n,&f,ENGINE_ID,1500));
 encode(&f,ENGINE_ID,2000,85,3);f.can_id|=CAN_EFF_FLAG;CHECK(!protocol_accept(&n,&f,ENGINE_ID,1500));
 Node v={0};encode(&f,VEHICLE_ID,2400,0,0);CHECK(protocol_accept(&v,&f,VEHICLE_ID,0));
 encode(&f,VEHICLE_ID,2401,0,1);CHECK(!protocol_accept(&v,&f,VEHICLE_ID,1));
 encode(&f,VEHICLE_ID,100,1,1);CHECK(!protocol_accept(&v,&f,VEHICLE_ID,2));
 encode(&f,ENGINE_ID,100,0,1);CHECK(!protocol_accept(&v,&f,VEHICLE_ID,3));
 puts("Protocol checks passed: encoding, ranges, format, checksum, timeout, sequence wrap/repeat/gap.");
 return 0;
}
