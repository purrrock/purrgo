/*
 * Тестовая ФС в памяти с внедрением сбоев (замена платформенного fs_hal).
 */
#include "purrgo/fs_hal.h"
#include "fake_fs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct purrgo_file_s { int idx; uint32_t pos; };
struct purrgo_dir_s  { int unused; };

typedef struct { char name[320]; uint8_t* data; uint32_t size; int used; uint8_t read_ahead; } ffile_t;
static ffile_t files[32];
static struct purrgo_file_s handles[8];
static int handle_used[8];

fake_fs_faults_t g_fault;
static int open_calls;
static uint32_t read_calls;
static uint32_t max_read_end;
static int sync_calls;
int fake_fs_open_calls(void) { return open_calls; }

void fake_fs_reset(void) {
    for (int i = 0; i < 32; i++) { free(files[i].data); }
    memset(files, 0, sizeof(files));
    memset(handle_used, 0, sizeof(handle_used));
    memset(&g_fault, 0, sizeof(g_fault));
    open_calls = 0;
    sync_calls = 0;
    read_calls = 0;
    max_read_end = 0;
}

uint32_t fake_fs_read_calls(void) { return read_calls; }
uint32_t fake_fs_max_read_end(void) { return max_read_end; }
void fake_fs_reset_read_stats(void) { read_calls = 0; max_read_end = 0; }

static int find(const char* n) {
    for (int i = 0; i < 32; i++) if (files[i].used && !strcmp(files[i].name, n)) return i;
    return -1;
}

const uint8_t* fake_fs_data(const char* n, uint32_t* size) {
    int i = find(n); if (i < 0) return NULL; *size = files[i].size; return files[i].data;
}
int fake_fs_open_handles(void) { int c = 0; for (int i = 0; i < 8; i++) c += handle_used[i]; return c; }

void fake_fs_put(const char* n, const char* text) {
    int i = find(n);
    if (i < 0) { for (i = 0; i < 32 && files[i].used; i++); }
    free(files[i].data);
    snprintf(files[i].name, sizeof(files[i].name), "%s", n);
    files[i].size = (uint32_t)strlen(text);
    files[i].data = malloc(files[i].size + 1);
    memcpy(files[i].data, text, files[i].size + 1);
    files[i].used = 1; files[i].read_ahead = 0;
}

void fake_fs_put_bin(const char* n, const uint8_t* d, uint32_t size) {
    int i = find(n);
    if (i < 0) { for (i = 0; i < 32 && files[i].used; i++); }
    free(files[i].data);
    snprintf(files[i].name, sizeof(files[i].name), "%s", n);
    files[i].size = size;
    files[i].data = malloc(size + 1u);
    if (size) { memcpy(files[i].data, d, size); }
    files[i].data[size] = 0;
    files[i].used = 1; files[i].read_ahead = 0;
}

purrgo_file_t* purrgo_fs_open(const char* p, fs_mode_t m) {
    open_calls++;
    if (g_fault.fail_open_append && m == FS_WRITE_APPEND) return NULL;
    if (g_fault.fail_open_all) return NULL;
    int i = find(p);
    if (m == FS_READ && i < 0) return NULL;
    if (i < 0 || m == FS_WRITE_CREATE) {
        if (i < 0) { for (i = 0; i < 32 && files[i].used; i++); }
        free(files[i].data);
        snprintf(files[i].name, sizeof(files[i].name), "%s", p);
        files[i].data = malloc(1); files[i].size = 0; files[i].used = 1;
    }
    for (int h = 0; h < 8; h++) if (!handle_used[h]) {
        handle_used[h] = 1; handles[h].idx = i;
        handles[h].pos = (m == FS_WRITE_APPEND) ? files[i].size : 0;
        return &handles[h];
    }
    return NULL;
}
uint32_t purrgo_fs_write(purrgo_file_t* f, const uint8_t* d, uint32_t n) {
    ffile_t* ff = &files[f->idx];
    if (g_fault.fail_write) return 0;
    if (g_fault.partial_write_bytes) { if (n > g_fault.partial_write_bytes) n = g_fault.partial_write_bytes; g_fault.partial_write_bytes = 0; g_fault.fail_write = 1; }
    ff->data = realloc(ff->data, f->pos + n + 1);
    memcpy(ff->data + f->pos, d, n); f->pos += n;
    if (f->pos > ff->size) ff->size = f->pos;
    return n;
}
uint32_t purrgo_fs_read(purrgo_file_t* f, uint8_t* b, uint32_t n) {
    ffile_t* ff = &files[f->idx];
    read_calls++;
    if (f->pos >= ff->size) return 0;
    if (n > ff->size - f->pos) n = ff->size - f->pos;
    memcpy(b, ff->data + f->pos, n); f->pos += n;
    if (f->pos > max_read_end) max_read_end = f->pos;
    return n;
}
bool purrgo_fs_seek(purrgo_file_t* f, uint32_t o) { f->pos = o; return true; }
void purrgo_fs_set_read_ahead(purrgo_file_t* f, uint8_t sectors) { files[f->idx].read_ahead = sectors; }
uint8_t fake_fs_read_ahead(const char* name) { int i = find(name); return i < 0 ? 0 : files[i].read_ahead; }
bool purrgo_fs_sync(purrgo_file_t* f) { (void)f; sync_calls++; return !g_fault.fail_sync; }
int fake_fs_sync_calls(void) { return sync_calls; }
bool purrgo_fs_close(purrgo_file_t* f) { handle_used[f - handles] = 0; return !g_fault.fail_close; }
bool purrgo_fs_replace(const char* s, const char* d) {
    if (g_fault.fail_replace) return false;
    int i = find(s); if (i < 0) return false;
    int j = find(d);
    if (j >= 0) { free(files[j].data); files[j].used = 0; files[j].data = NULL; }
    snprintf(files[i].name, sizeof(files[i].name), "%s", d);
    return true;
}
purrgo_dir_t* purrgo_fs_opendir(const char* p) { (void)p; return NULL; }
bool purrgo_fs_readdir(purrgo_dir_t* d, purrgo_fs_dirent_t* e) { (void)d; (void)e; return false; }
void purrgo_fs_closedir(purrgo_dir_t* d) { (void)d; }
const char* purrgo_fs_get_config_path(void) { return "PURRGO.CFG"; }
const char* purrgo_fs_get_maps_path(void) { return "maps"; }
const char* purrgo_fs_get_tracks_path(void) { return "TRACKS"; }
