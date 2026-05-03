#pragma once

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed long long   int64_t;

typedef uint64_t           uintptr_t;
typedef int64_t            intptr_t;
typedef uint64_t           size_t;

#define NULL ((void*)0)
#define SERIAL_PORT 0x3F8

static inline uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "d"(port));
    return val;
}
 
static inline uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ volatile("inw %1, %0" : "=a"(val) : "d"(port));
    return val;
}
 
static inline uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ volatile("inl %1, %0" : "=a"(val) : "d"(port));
    return val;
}
 
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "d"(port));
}
 
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile("outw %0, %1" : : "a"(val), "d"(port));
}
 
static inline void outl(uint16_t port, uint32_t val) {
    __asm__ volatile("outl %0, %1" : : "a"(val), "d"(port));
}
 
static inline void _dprint_char(char c) {
    while ((inb(SERIAL_PORT + 5) & 0x20) == 0);
    outb(SERIAL_PORT, c);
}
 
static inline void _dprint_string(const char *s) {
    while (*s) {
        if (*s == '\n') _dprint_char('\r');
        _dprint_char(*s++);
    }
}
 
static inline void _dprint_hex(uint32_t val) {
    _dprint_string("0x");
    for (int i = 28; i >= 0; i -= 4) {
        unsigned char nibble = (val >> i) & 0xF;
        _dprint_char(nibble < 10 ? '0' + nibble : 'A' + nibble - 10);
    }
}
 
#define DPRINT(msg) _dprint_string(msg)
#define DPRINT_HEX(val) _dprint_hex(val)
 
#define DPRINT_INIT() do { \
    outb(SERIAL_PORT + 1, 0x00); \
    outb(SERIAL_PORT + 3, 0x80); \
    outb(SERIAL_PORT + 0, 0x01); \
    outb(SERIAL_PORT + 1, 0x00); \
    outb(SERIAL_PORT + 3, 0x03); \
    outb(SERIAL_PORT + 2, 0xC7); \
    outb(SERIAL_PORT + 4, 0x0B); \
} while(0)
