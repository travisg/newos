#define LED_DISPLAY(pos, c) \
    (*(char *)(0x08000100 + (3-pos)*2) = (c))

void _start(void);
void _start(void)
{
    LED_DISPLAY(0, 'n');
    LED_DISPLAY(1, 'u');
    LED_DISPLAY(2, 'o');
    LED_DISPLAY(3, 's');

    for (;;);

    return;
}
