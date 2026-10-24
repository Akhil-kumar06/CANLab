#include "protocol.h"
#include <string.h>
/* Custom application checksum, NOT the CAN controller's hardware CRC. */
static uint8_t checksum(const struct can_frame *f) {
 uint8_t c=(uint8_t)f->can_id ^ (uint8_t)(f->can_id>>8);
 for(unsigned i=0;i<4;i++) c^=f->data[i];
 return c;
}
void encode(struct can_frame *f,unsigned id,unsigned value,unsigned temp,uint8_t seq) {
 memset(f,0,sizeof *f); f->can_id=id; f->can_dlc=5;
 f->data[0]=(uint8_t)value; f->data[1]=(uint8_t)(value>>8);
 f->data[2]=(uint8_t)temp; f->data[3]=seq; f->data[4]=checksum(f);
}
bool protocol_accept(Node *n,const struct can_frame *f,unsigned id,uint64_t now) {
 unsigned value=(unsigned)f->data[0] | ((unsigned)f->data[1]<<8);
 if(f->can_id!=id || f->can_dlc!=5 || f->data[4]!=checksum(f) ||
    (id==ENGINE_ID && (value>8000 || f->data[2]>150)) ||
    (id==VEHICLE_ID && (value>2400 || f->data[2]!=0))) {
   n->invalid++; return false;
 }
 if(n->seen && (uint8_t)(n->sequence+1)!=f->data[3]) n->gaps++;
 n->seen=true; n->last_ms=now; n->received++; n->sequence=f->data[3];
 n->value=value; n->temperature=f->data[2]; return true;
}
const char *health(const Node *n,uint64_t now) {
 if(!n->seen) return "WAITING";
 return now-n->last_ms>500 ? "TIMEOUT" : "OK";
}
