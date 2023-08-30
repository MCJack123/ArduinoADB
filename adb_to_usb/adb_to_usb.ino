#include <string.h>
#include "adb.h"
#include "keymap.h"

static bool has_media_keys = false;
static bool is_iso_layout = false;
uint8_t buf[8] = { 0 };

inline
static void register_key(uint8_t key) //8bit - 1bit release, 7bit keycode
{
    uint8_t tmp[6];
    if (key & 0x80) {   // handles key press     0x80= 10000000  only mat
        switch (key & 0x7F) {                   //0x7F = 01111111  
          case 0x36: // LCTRL
            buf[0] &= ~(1<<0); break;
          case 0x37: // LGUI
            buf[0] &= ~(1<<3); // LGUI
            break;
          case 0x38: // LSHIFT
            buf[0] &= ~(1<<1); break;
          case 0x3A: // LALT
            buf[0] &= ~(1<<2); // LALT
            break;
          case 0x7B: // RSHIFT
            buf[0] &= ~(1<<5); break;
          case 0x7C: // RALT
            buf[0] &= ~(1<<6); break;
          case 0x7D: // RCTRL
            buf[0] &= ~(1<<4); break;
          case 0x7E: // RGUI
            buf[0] &= ~(1<<7); break;
          default:    //Alphanumeric keys
            memset(&tmp, 0, sizeof(tmp));
            
            for (int i = 2, tmpi = 0; i < 8; i++) {
              if (buf[i] != adb_to_usb[key & 0x7F]) tmp[tmpi++] = buf[i];
            }
            for (int i = 0; i < 6; i++) buf[i+2] = tmp[i];
        }
    } else {    // handles key releases
        switch (key) {
          case 0x36: // LCTRL
            buf[0] |= (1<<0); break;
          case 0x37: // LGUI
            buf[0] |= (1<<3); // LGUI
             break;
          case 0x38: // LSHIFT
            buf[0] |= (1<<1); break;
          case 0x3A: // LALT
            buf[0] |= (1<<2); // LALT
            break;
          case 0x7B: // RSHIFT
            buf[0] |= (1<<5); break;
          case 0x7C: // RALT
            buf[0] |= (1<<6); break;
          case 0x7D: // RCTRL
            buf[0] |= (1<<4); break;
          case 0x7E: // RGUI
            buf[0] |= (1<<7); break;
          default:
            for (int i = 2; i < 8; i++) {
              if (buf[i] == 0) {buf[i] = adb_to_usb[key]; break;}
              else if (buf[i] == adb_to_usb[key]) break;
            }
        }
    }
    Serial.write(buf, 8);
}

//do we need this?
static void device_scan(void)
{
    for (uint8_t addr = 0; addr < 16; addr++) {
        uint16_t reg3 = adb_host_talk(addr, ADB_REG_3);
    }
}

void setup() {
  Serial.begin(9600);
  DDRB |= (1<<5); PORTB |= (1<<5);
  adb_host_init();
  delay(1000);
  device_scan();

  // Enable keyboard left/right modifier distinction
    // Listen Register3
    //  upper byte: reserved bits 0000, keyboard address 0010
    //  lower byte: device handler 00000011
    adb_host_listen(ADB_ADDR_KEYBOARD, ADB_REG_3, ADB_ADDR_KEYBOARD, ADB_HANDLER_EXTENDED_KEYBOARD);
    DDRB |= (1<<5); PORTB &= ~(1<<5);
    device_scan();
}

void loop() {
  // put your main code here, to run repeatedly:
/* extra_key is volatile and more convoluted than necessary because gcc refused
    to generate valid code otherwise. Making extra_key uint8_t and constructing codes
    here via codes = extra_key<<8 | 0xFF; would consistently fail to even LOAD
    extra_key from memory, and leave garbage in the high byte of codes. I tried
    dozens of code variations and it kept generating broken assembly output. So
    beware if attempting to make extra_key code more logical and efficient. */
    static volatile uint16_t extra_key = 0xFFFF;
    uint16_t codes;
    uint8_t key0, key1;

    codes = extra_key;
    extra_key = 0xFFFF;

    //get next key
    if ( codes == 0xFFFF ) {
        codes = adb_host_kbd_recv(ADB_ADDR_KEYBOARD);
    }
    key0 = codes>>8;
    key1 = codes&0xFF;

    if (codes == 0 || codes == 0x7F7F || codes == 0xFFFF) {                           // no keys | power key press | power key release
        return;
    } else {
        register_key(key0);
        if (key1 != 0xFF)       // key1 is 0xFF when no second key.
            extra_key = key1<<8 | 0xFF; // process in next loop itr
    }
    delay(ADB_POLLING_DELAY_MS);
}
