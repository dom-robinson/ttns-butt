#ifndef TTNS_UI_H
#define TTNS_UI_H

class flgui;

void ttns_ui_init(flgui *g);
void ttns_ui_sync_from_cfg(void);
void ttns_cfg_sync_from_ui(void);
void ttns_ui_apply_zone_selection(void);
void ttns_ui_sync_advanced(void);
void ttns_ui_timer_tick(void);
void ttns_ui_trigger_cart(int slot);

#endif
