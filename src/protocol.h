#ifndef PROTOCOL_H
#define PROTOCOL_H
#include <stdbool.h>
#include <stdint.h>
#include <linux/can.h>
#define ENGINE_ID 0x100U
#define VEHICLE_ID 0x200U
typedef struct {
 bool seen; uint64_t last_ms; unsigned received, invalid, gaps;
 uint8_t sequence; unsigned value, temperature;
} Node;
void encode(struct can_frame *f, unsigned id, unsigned value, unsigned temp, uint8_t seq);
bool protocol_accept(Node *n, const struct can_frame *f, unsigned id, uint64_t now);
const char *health(const Node *n,uint64_t now);
#endif
