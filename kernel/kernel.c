void kernel_main(void) {
    volatile char* vga = (volatile char*)0xB8000;
    
    const char* msg = "WELCOME TO OASIS";
    int i = 0;

    while(msg[i]) {
        vga[i*2] = msg[i];
        vga[i*2+1] = 0x0F;
        i++;
    }

    while(1) {
        __asm__ __volatile__("hlt");
    }
}