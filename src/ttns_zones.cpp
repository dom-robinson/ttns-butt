#include "ttns_zones.h"
#include "ttns_paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include <FL/Fl_Choice.H>

#include "cfg.h"

#define TTNS_ZONES_MAX 4
#define TTNS_SLOTS_MAX 5
#define TTNS_SRV_NAME "TTNS Deck"
#define TTNS_ICY_NAME "TTNS"

typedef struct
{
    char mount[64];
    char password[64];
} ttns_slot_t;

typedef struct
{
    char host[256];
    int port;
    char user[64];
    char description[512];
    char genre[128];
    ttns_slot_t slots[TTNS_ZONES_MAX][TTNS_SLOTS_MAX];
    int loaded;
} ttns_zones_data_t;

static ttns_zones_data_t zones;

static int read_file(const char *path, char **out, size_t *out_len)
{
    FILE *f;
    long sz;
    char *buf;

    f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return -1;
    }
    sz = ftell(f);
    if (sz < 0)
    {
        fclose(f);
        return -1;
    }
    rewind(f);
    buf = (char*)malloc((size_t)sz + 1);
    if (!buf)
    {
        fclose(f);
        return -1;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz)
    {
        free(buf);
        fclose(f);
        return -1;
    }
    buf[sz] = '\0';
    fclose(f);
    *out = buf;
    *out_len = (size_t)sz;
    return 0;
}

static const char *find_mount_block(const char *json, const char *mount)
{
    char needle[128];
    snprintf(needle, sizeof(needle), "\"mount\": \"%s\"", mount);
    return strstr(json, needle);
}

static int parse_slot_password(const char *json, const char *mount, char *pwd, size_t pwd_len)
{
    const char *p = find_mount_block(json, mount);
    const char *q;

    if (!p)
        return -1;
    q = strstr(p, "\"password\"");
    if (!q)
        return -1;
    q = strchr(q, ':');
    if (!q)
        return -1;
    q = strchr(q, '"');
    if (!q)
        return -1;
    q++;
    if (sscanf(q, "%63[^\"]", pwd) != 1)
        return -1;
    return 0;
}

static void parse_string_field(const char *json, const char *key, char *dest, size_t dest_len)
{
    char pattern[64];
    const char *p;
    const char *q;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    p = strstr(json, pattern);
    if (!p)
        return;
    q = strchr(p + strlen(pattern), '"');
    if (!q)
        return;
    q++;
    sscanf(q, "%255[^\"]", dest);
    dest[dest_len - 1] = '\0';
}

static int try_load_path(const char *path)
{
    char *json = NULL;
    size_t len = 0;
    int z, s;
    char mount[64];

    if (read_file(path, &json, &len) != 0)
        return -1;

    memset(&zones, 0, sizeof(zones));
    parse_string_field(json, "host", zones.host, sizeof(zones.host));
    zones.port = 8080;
    {
        const char *p = strstr(json, "\"port\"");
        if (p)
        {
            p = strchr(p, ':');
            if (p)
                zones.port = atoi(p + 1);
        }
    }
    parse_string_field(json, "user", zones.user, sizeof(zones.user));

    {
        const char *meta = strstr(json, "\"stream_metadata\"");
        if (meta)
        {
            parse_string_field(meta, "description", zones.description, sizeof(zones.description));
            parse_string_field(meta, "genre", zones.genre, sizeof(zones.genre));
        }
    }

    for (z = 0; z < TTNS_ZONES_MAX; z++)
    {
        for (s = 0; s < TTNS_SLOTS_MAX; s++)
        {
            snprintf(mount, sizeof(mount), "ttnszone%d_%d", z + 1, s + 1);
            strncpy(zones.slots[z][s].mount, mount, sizeof(zones.slots[z][s].mount) - 1);
            if (parse_slot_password(json, mount, zones.slots[z][s].password,
                                    sizeof(zones.slots[z][s].password)) != 0)
            {
                free(json);
                return -1;
            }
        }
    }

    free(json);
    zones.loaded = 1;
    return 0;
}

int ttns_zones_load(void)
{
    char path[PATH_MAX];

    if (ttns_path_data_file("ttns-zones.json", path, sizeof(path)) == 0)
        return try_load_path(path);
    return -1;
}

static int find_or_add_server(const char *name)
{
    int i;

    for (i = 0; i < cfg.main.num_of_srv; i++)
    {
        if (cfg.srv[i]->name && !strcmp(cfg.srv[i]->name, name))
            return i;
    }

    i = cfg.main.num_of_srv;
    cfg.main.num_of_srv++;
    cfg.srv = (server_t**)realloc(cfg.srv, cfg.main.num_of_srv * sizeof(server_t*));
    cfg.srv[i] = (server_t*)calloc(1, sizeof(server_t));
    cfg.srv[i]->name = strdup(name);
    cfg.srv[i]->type = ICECAST;
    cfg.srv[i]->usr = strdup("source");
    cfg.srv[i]->mount = strdup("ttnszone1_1");
    cfg.srv[i]->addr = strdup("localhost");
    cfg.srv[i]->pwd = strdup("");
    cfg.srv[i]->port = 8000;

    if (i == 0)
    {
        cfg.main.srv = strdup(name);
        cfg.main.srv_ent = strdup(name);
    }
    else
    {
        cfg.main.srv_ent = (char*)realloc(cfg.main.srv_ent,
            strlen(cfg.main.srv_ent) + strlen(name) + 2);
        strcat(cfg.main.srv_ent, ";");
        strcat(cfg.main.srv_ent, name);
    }
    return i;
}

static int find_or_add_icy(const char *name)
{
    int i;

    for (i = 0; i < cfg.main.num_of_icy; i++)
    {
        if (cfg.icy[i]->name && !strcmp(cfg.icy[i]->name, name))
            return i;
    }

    i = cfg.main.num_of_icy;
    cfg.main.num_of_icy++;
    cfg.icy = (icy_t**)realloc(cfg.icy, cfg.main.num_of_icy * sizeof(icy_t*));
    cfg.icy[i] = (icy_t*)calloc(1, sizeof(icy_t));
    cfg.icy[i]->name = strdup(name);
    cfg.icy[i]->pub = strdup("1");
    cfg.icy[i]->desc = strdup("");
    cfg.icy[i]->genre = strdup("eclectic");
    cfg.icy[i]->url = strdup("");
    cfg.icy[i]->irc = strdup("");
    cfg.icy[i]->icq = strdup("");
    cfg.icy[i]->aim = strdup("");

    if (i == 0)
    {
        cfg.main.icy = strdup(name);
        cfg.main.icy_ent = strdup(name);
    }
    else
    {
        cfg.main.icy_ent = (char*)realloc(cfg.main.icy_ent,
            strlen(cfg.main.icy_ent) + strlen(name) + 2);
        strcat(cfg.main.icy_ent, ";");
        strcat(cfg.main.icy_ent, name);
    }
    return i;
}

int ttns_zones_apply(int zone_id, int slot_id)
{
    int zi, si, srv_i, icy_i;
    const ttns_slot_t *slot;

    if (!zones.loaded && ttns_zones_load() != 0)
        return -1;

    if (zone_id < 1 || zone_id > TTNS_ZONES_MAX || slot_id < 1 || slot_id > TTNS_SLOTS_MAX)
        return -1;

    zi = zone_id - 1;
    si = slot_id - 1;
    slot = &zones.slots[zi][si];

    srv_i = find_or_add_server(TTNS_SRV_NAME);
    free(cfg.srv[srv_i]->addr);
    cfg.srv[srv_i]->addr = strdup(zones.host);
    cfg.srv[srv_i]->port = zones.port;
    free(cfg.srv[srv_i]->usr);
    cfg.srv[srv_i]->usr = strdup(zones.user);
    free(cfg.srv[srv_i]->pwd);
    cfg.srv[srv_i]->pwd = strdup(slot->password);
    free(cfg.srv[srv_i]->mount);
    cfg.srv[srv_i]->mount = strdup(slot->mount);
    cfg.srv[srv_i]->type = ICECAST;
    cfg.selected_srv = srv_i;
    cfg.main.srv = strdup(TTNS_SRV_NAME);

    icy_i = find_or_add_icy(TTNS_ICY_NAME);
    free(cfg.icy[icy_i]->desc);
    cfg.icy[icy_i]->desc = strdup(zones.description);
    free(cfg.icy[icy_i]->genre);
    cfg.icy[icy_i]->genre = strdup(zones.genre);
    cfg.selected_icy = icy_i;
    cfg.main.icy = strdup(TTNS_ICY_NAME);

    cfg.ttns.zone = zone_id;
    cfg.ttns.slot = slot_id;
    return 0;
}

int ttns_zones_mount_index(int zone_id, int slot_id)
{
    if (zone_id < 1 || zone_id > TTNS_ZONES_MAX || slot_id < 1 || slot_id > TTNS_SLOTS_MAX)
        return 0;

    return (zone_id - 1) * TTNS_SLOTS_MAX + (slot_id - 1);
}

void ttns_zones_index_to_mount(int index, int *zone_id, int *slot_id)
{
    int z;
    int s;

    if (index < 0)
        index = 0;
    if (index >= TTNS_ZONES_MAX * TTNS_SLOTS_MAX)
        index = TTNS_ZONES_MAX * TTNS_SLOTS_MAX - 1;

    z = index / TTNS_SLOTS_MAX;
    s = index % TTNS_SLOTS_MAX;

    if (zone_id)
        *zone_id = z + 1;
    if (slot_id)
        *slot_id = s + 1;
}

void ttns_zones_fill_mount_choice(Fl_Choice *mount_choice)
{
    int z;
    int s;
    char label[32];

    if (!mount_choice)
        return;

    mount_choice->clear();
    for (z = 1; z <= TTNS_ZONES_MAX; z++)
    {
        for (s = 1; s <= TTNS_SLOTS_MAX; s++)
        {
            snprintf(label, sizeof(label), "ttnszone %d-%d", z, s);
            mount_choice->add(label);
        }
    }

    mount_choice->value(ttns_zones_mount_index(
        cfg.ttns.zone > 0 ? cfg.ttns.zone : 1,
        cfg.ttns.slot > 0 ? cfg.ttns.slot : 1));
}
