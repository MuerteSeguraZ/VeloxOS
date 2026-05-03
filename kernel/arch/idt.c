#include "idt.h"
#include "../drivers/keyboard/keyboard.h"

static idt_entry_t   idt[IDT_ENTRIES];
static idt_ptr_t     idt_ptr;
static irq_handler_t irq_handlers[16];

static inline void idt_outb(uint16_t port,uint8_t val){__asm__ volatile("outb %0,%1"::"a"(val),"Nd"(port));}
static inline uint8_t idt_inb(uint16_t port){uint8_t r;__asm__ volatile("inb %1,%0":"=a"(r):"Nd"(port));return r;}
static inline void io_wait(void){idt_outb(0x80,0);}

extern void irq_stub_0(void);  extern void irq_stub_1(void);
extern void irq_stub_2(void);  extern void irq_stub_3(void);
extern void irq_stub_4(void);  extern void irq_stub_5(void);
extern void irq_stub_6(void);  extern void irq_stub_7(void);
extern void irq_stub_8(void);  extern void irq_stub_9(void);
extern void irq_stub_10(void); extern void irq_stub_11(void);
extern void irq_stub_12(void); extern void irq_stub_13(void);
extern void irq_stub_14(void); extern void irq_stub_15(void);

static void idt_set(int n,uint64_t h){
    idt[n].offset_low=h&0xFFFF; idt[n].selector=0x08; idt[n].ist=0;
    idt[n].type_attr=0x8E; idt[n].offset_mid=(h>>16)&0xFFFF;
    idt[n].offset_high=(h>>32)&0xFFFFFFFF; idt[n].zero=0;
}

static void pic_remap(void){
    idt_outb(PIC1_CMD,0x11);io_wait();  idt_outb(PIC2_CMD,0x11);io_wait();
    idt_outb(PIC1_DATA,0x20);io_wait(); idt_outb(PIC2_DATA,0x28);io_wait();
    idt_outb(PIC1_DATA,0x04);io_wait(); idt_outb(PIC2_DATA,0x02);io_wait();
    idt_outb(PIC1_DATA,0x01);io_wait(); idt_outb(PIC2_DATA,0x01);io_wait();
    idt_outb(PIC1_DATA,0xF8);   // IRQ0,1,2 unmasked
    idt_outb(PIC2_DATA,0xEF);   // IRQ12 unmasked
}

static void kbd_irq_handler(void) {
    uint8_t sc = idt_inb(0x60);
    kbd_enqueue_scancode(sc);
}

void idt_init(void){
    idt_set(0x20,(uint64_t)irq_stub_0);  idt_set(0x21,(uint64_t)irq_stub_1);
    idt_set(0x22,(uint64_t)irq_stub_2);  idt_set(0x23,(uint64_t)irq_stub_3);
    idt_set(0x24,(uint64_t)irq_stub_4);  idt_set(0x25,(uint64_t)irq_stub_5);
    idt_set(0x26,(uint64_t)irq_stub_6);  idt_set(0x27,(uint64_t)irq_stub_7);
    idt_set(0x28,(uint64_t)irq_stub_8);  idt_set(0x29,(uint64_t)irq_stub_9);
    idt_set(0x2A,(uint64_t)irq_stub_10); idt_set(0x2B,(uint64_t)irq_stub_11);
    idt_set(0x2C,(uint64_t)irq_stub_12); idt_set(0x2D,(uint64_t)irq_stub_13);
    idt_set(0x2E,(uint64_t)irq_stub_14); idt_set(0x2F,(uint64_t)irq_stub_15);
    idt_ptr.limit=sizeof(idt)-1; idt_ptr.base=(uint64_t)&idt;
    pic_remap();
    irq_register(1, kbd_irq_handler);
    __asm__ volatile("lidt %0"::"m"(idt_ptr));
    __asm__ volatile("sti");
}

void irq_register(int irq,irq_handler_t handler){
    if(irq>=0&&irq<16) irq_handlers[irq]=handler;
}

void irq_dispatch(int irq){
    if(irq_handlers[irq]) irq_handlers[irq]();
    if(irq>=8) idt_outb(PIC2_CMD,0x20);
    idt_outb(PIC1_CMD,0x20);
}