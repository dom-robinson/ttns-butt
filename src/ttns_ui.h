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
void ttns_schedule_audio_reopen(void);
void ttns_apply_audio_settings(void);
void ttns_audio_mark_applied(void);
void ttns_audio_settings_changed(void);

/* Pin deck + Remotes + More/info stack and window height. */
void ttns_ui_relayout_shell(void);

/* Failsafe: must tick Confirm next to Mount before Connect/go-live. Always starts off. */
int ttns_ui_mount_is_confirmed(void);
void ttns_ui_clear_mount_confirm(void);
void ttns_ui_update_connect_armed(void);

#endif
