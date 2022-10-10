#include "adb.h"
#include "keymap.h"
// #define LOCKING_CAPS // comment this out if not using a keyboard with locking Caps Lock
// #define SWAP_ALT_GUI // uncomment this if you want to swap the alt and super(GUI) keys to emulate behavior/positioning of windows key

static bool has_media_keys = false;
static bool is_iso_layout = false;
uint8_t buf[8] = { 0 };
#ifdef LOCKING_CAPS
bool capsOn = false;
#endif

inline
static void register_key(uint8_t key)
{
    uint8_t tmp[6];
    if (key&0x80) {
        // handles key press
        //matrix[row] &= ~(1<<col);
        switch (key & 0x7F) {
          case 0x36: // LCTRL
            buf[0] &= ~(1<<0); break;
          case 0x37: // LGUI
#ifdef SWAP_ALT_GUI
            buf[0] &= ~(1<<2); // LALT
#else
            buf[0] &= ~(1<<3); // LGUI
#endif
            break;
          case 0x38: // LSHIFT
            buf[0] &= ~(1<<1); break;
          case 0x3A: // LALT
#ifdef SWAP_ALT_GUI
            buf[0] &= ~(1<<3); // LGUI
#else
            buf[0] &= ~(1<<2); // LALT
#endif
            break;
          case 0x7B: // RSHIFT
            buf[0] &= ~(1<<5); break;
          case 0x7C: // RALT
            buf[0] &= ~(1<<6); break;
          case 0x7D: // RCTRL
            buf[0] &= ~(1<<4); break;
          case 0x7E: // RGUI
            buf[0] &= ~(1<<7); break;
#ifdef LOCKING_CAPS
          case 57: // CAPS (locking)
            if (!capsOn) return;
            capsOn = false;
            for (int i = 2; i < 8; i++) {
              if (buf[i] == 0) {buf[i] = 57; break;}
              else if (buf[i] == 57) break;
            }
            Serial.write(buf, 8);
#endif
          default:
            for (int i = 0; i < 6; i++) tmp[i] = 0;
            for (int i = 2, tmpi = 0; i < 8; i++) {
              if (buf[i] != adb_to_usb[key & 0x7F]) tmp[tmpi++] = buf[i];
            }
            for (int i = 0; i < 6; i++) buf[i+2] = tmp[i];
        }
    } else {
        //matrix[row] |=  (1<<col);
#ifdef LOCKING_CAPS
        if (key == 57 && capsOn) return;
#endif
        // handles key releases
        switch (key) {
          case 0x36: // LCTRL
            buf[0] |= (1<<0); break;
          case 0x37: // LGUI
#ifdef SWAP_ALT_GUI
            buf[0] |= (1<<2); // LALT
#else 
            buf[0] |= (1<<3); // LGUI
#endif
             break;
          case 0x38: // LSHIFT
            buf[0] |= (1<<1); break;
          case 0x3A: // LALT
#ifdef SWAP_ALT_GUI
            buf[0] |= (1<<3); // LGUI
#else 
            buf[0] |= (1<<2); // LALT
#endif
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
#ifdef LOCKING_CAPS
        if (key == 57) {
            capsOn = true;
            Serial.write(buf, 8);
            for (int i = 0; i < 6; i++) tmp[i] = 0;
            for (int i = 2, tmpi = 0; i < 8; i++) {
              if (buf[i] != 57) tmp[tmpi++] = buf[i];
            }
            for (int i = 0; i < 6; i++) buf[i+2] = tmp[i];
        }
#endif
    }
    Serial.write(buf, 8);
    /*for (int i = 0; i < 8; i++) {
      Serial.print(buf[i]);
      Serial.print(" ");
    }
    Serial.println("");*/
}

void led_set(uint8_t usb_led)
{
    adb_host_kbd_led(ADB_ADDR_KEYBOARD, ~usb_led);
}

static void device_scan(void)
{
    //xprintf("\nScan:\n");
    for (uint8_t addr = 0; addr < 16; addr++) {
        uint16_t reg3 = adb_host_talk(addr, ADB_REG_3);
        if (reg3) {
            //xprintf(" addr:%d, reg3:%04X\n", addr, reg3);
        }
    }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  //Serial.println("start");
  DDRB |= (1<<5); PORTB |= (1<<5);
  //printstr("Starting...\n");
  adb_host_init();
  delay(1000);
  device_scan();
  //Serial.print("\nKeyboard:\n");
    // Determine ISO keyboard by handler id
    // http://lxr.free-electrons.com/source/drivers/macintosh/adbhid.c?v=4.4#L815
    uint8_t handler_id = (uint8_t) adb_host_talk(ADB_ADDR_KEYBOARD, ADB_REG_3);
    switch (handler_id) {
    case 0x04: case 0x05: case 0x07: case 0x09: case 0x0D:
    case 0x11: case 0x14: case 0x19: case 0x1D: case 0xC1:
    case 0xC4: case 0xC7:
        is_iso_layout = true;
        break;
    default:
        is_iso_layout = false;
        break;
    }
    //Serial.print("handler: ");
    //Serial.print(handler_id, HEX);
    //Serial.print(", ISO: ");
    //Serial.println(is_iso_layout ? "yes" : "no");
  has_media_keys = (0x02 == (adb_host_talk(ADB_ADDR_APPLIANCE, ADB_REG_3) & 0xff));
    if (has_media_keys) {
        //Serial.print("Media keys\n");
    }
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

    /* tick of last polling */
    static uint16_t tick_ms;

    codes = extra_key;
    extra_key = 0xFFFF;
    if (Serial.available()) adb_host_kbd_led(ADB_ADDR_KEYBOARD, ~Serial.read());

    if ( codes == 0xFFFF )
    {
        // polling with 12ms interval
        //if (timer_elapsed(tick_ms) < 12) return 0;
        //tick_ms = timer_read();

        codes = adb_host_kbd_recv(ADB_ADDR_KEYBOARD);

        // Adjustable keybaord media keys
        if (codes == 0 && has_media_keys &&
                (codes = adb_host_kbd_recv(ADB_ADDR_APPLIANCE))) {
            // key1
            switch (codes & 0x7f ) {
            case 0x00:  // Mic
                codes = (codes & ~0x007f) | 0x42;
                break;
            case 0x01:  // Mute
                codes = (codes & ~0x007f) | 0x4a;
                break;
            case 0x02:  // Volume down
                codes = (codes & ~0x007f) | 0x49;
                break;
            case 0x03:  // Volume Up
                codes = (codes & ~0x007f) | 0x48;
                break;
            case 0x7F:  // no code
                break;
            default:
                //xprintf("ERROR: media key1\n");
                return;
            }
            // key0
            switch ((codes >> 8) & 0x7f ) {
            case 0x00:  // Mic
                codes = (codes & ~0x7f00) | (0x42 << 8);
                break;
            case 0x01:  // Mute
                codes = (codes & ~0x7f00) | (0x4a << 8);
                break;
            case 0x02:  // Volume down
                codes = (codes & ~0x7f00) | (0x49 << 8);
                break;
            case 0x03:  // Volume Up
                codes = (codes & ~0x7f00) | (0x48 << 8);
                break;
            default:
                //xprintf("ERROR: media key0\n");
                return;
            }
        }
    }
    key0 = codes>>8;
    key1 = codes&0xFF;

    //if (debug_matrix && codes) {
        //print("adb_host_kbd_recv: "); phex16(codes); print("\n");
    //}

    if (codes == 0) {                           // no keys
        return;
    } else if (codes == 0x7F7F) {   // power key press
        register_key(0x7F);
    } else if (codes == 0xFFFF) {   // power key release
        register_key(0xFF);
    } else {
        // Macally keyboard sends keys inversely against ADB protocol
        // https://deskthority.net/workshop-f7/macally-mk96-t20116.html
        if (key0 == 0xFF) {
            key0 = key1;
            key1 = 0xFF;
        }

        /* Swap codes for ISO keyboard
         * https://github.com/tmk/tmk_keyboard/issues/35
         *
         * ANSI
         * ,-----------    ----------.
         * | *a|  1|  2     =|Backspa|
         * |-----------    ----------|
         * |Tab  |  Q|     |  ]|   *c|
         * |-----------    ----------|
         * |CapsLo|  A|    '|Return  |
         * |-----------    ----------|
         * |Shift   |      Shift     |
         * `-----------    ----------'
         *
         * ISO
         * ,-----------    ----------.
         * | *a|  1|  2     =|Backspa|
         * |-----------    ----------|
         * |Tab  |  Q|     |  ]|Retur|
         * |-----------    -----`    |
         * |CapsLo|  A|    '| *c|    |
         * |-----------    ----------|
         * |Shif| *b|      Shift     |
         * `-----------    ----------'
         *
         *         ADB scan code   USB usage
         *         -------------   ---------
         * Key     ANSI    ISO     ANSI    ISO
         * ---------------------------------------------
         * *a      0x32    0x0A    0x35    0x35
         * *b      ----    0x32    ----    0x64
         * *c      0x2A    0x2A    0x31    0x31(or 0x32)
         */
        if (is_iso_layout) {
            if ((key0 & 0x7F) == 0x32) {
                key0 = (key0 & 0x80) | 0x0A;
            } else if ((key0 & 0x7F) == 0x0A) {
                key0 = (key0 & 0x80) | 0x32;
            }
        }
        register_key(key0);
        if (key1 != 0xFF)       // key1 is 0xFF when no second key.
            extra_key = key1<<8 | 0xFF; // process in a separate call
    }
    delay(12);
}
