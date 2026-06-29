#ifndef TTNS_ZONES_H
#define TTNS_ZONES_H

int ttns_zones_load(void);
int ttns_zones_apply(int zone_id, int slot_id);
void ttns_zones_fill_mount_choice(class Fl_Choice *mount_choice);
int ttns_zones_mount_index(int zone_id, int slot_id);
void ttns_zones_index_to_mount(int index, int *zone_id, int *slot_id);

#endif
