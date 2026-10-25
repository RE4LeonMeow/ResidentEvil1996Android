#pragma once
// OptionsMenu.h - Options/configuration menu (key bindings, display, joystick config)
// Decompiled from original at 0x004761b0

void options_menu(void);

// Sub-menu entry points (called as task sub-functions via function pointer table)
void options_menu_exit(void);           // 0x00476b00 - initiate options exit fade
void options_menu_render(void);         // 0x00476b40 - render all options UI
void options_init_keybind_display(void);// 0x00477230 - populate key binding display table
void options_render_cursor(void);       // 0x00477490 - draw selection cursor/highlight
void options_render_entity(int ent);    // 0x004775b0 - render player model joints

// Sub-menu handlers (return 0 to keep running, non-zero to exit)
unsigned int options_key_config_handler(void);    // 0x00452c50
unsigned int options_display_config_handler(void);// 0x00451960
unsigned int options_joystick_config_handler(void);// 0x00453a80

// Sub-menu input handlers
unsigned int options_key_config_input(void);      // 0x00452de0
unsigned int options_display_config_input(void);  // 0x00451b80
unsigned int options_joystick_config_input(void); // 0x00453c10

// Input repeat handler for cursor navigation
unsigned short options_input_repeat(unsigned char* repeatTimer, unsigned short* prevButtons, unsigned char timing, unsigned short mask); // 0x00451900

// VK code / button mapping helpers
void options_map_key_to_print_index(int entry);          // 0x00453950
void options_map_vk_to_controller_symbol(unsigned char* entry); // 0x00453790
void options_map_vk_to_font_index(unsigned char* entry);  // 0x00453610

// Text rendering helpers
void options_print_14x14(short x, short y, unsigned char tint, char mirror); // 0x00456460
void options_print_8x8_glyph(short x, short y, unsigned char tint, char mirror); // 0x00456360

// Keyboard scan (0x00497de0)
unsigned char options_read_keyboard_scancode(void);
