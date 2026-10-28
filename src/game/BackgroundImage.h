// Port-owned room-background upload. Only campaign replacement TIMs opt into
// header-based stride; retail GOG headers can misdescribe bordered pixel rows.
#pragma once
void display_room_background(int slot,void* tim,int width,int height,bool replacement);
