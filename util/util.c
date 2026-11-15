#include "util.h"

extern FILE *g_out_file;

void add_item(CMap **d, char *key, u64 value)
{
    if (NULL == *d) {
        *d = (CMap *)malloc(sizeof(CMap));
        (*d)->count = 0;
    }

    int old_count = (*d)->count;
    (*d)->items = realloc((*d)->items, sizeof(CMapItem) * (old_count + 1));
    (*d)->items[old_count].key   = key;
    (*d)->items[old_count].value = value;

    (*d)->count = old_count + 1;
}

u64 get_value(CMap *d, char *key)
{
    for (int i = 0; i < d->count; i++) {
        if (!memcmp(d->items[i].key, key, strlen(key))) {
            return d->items[i].value;
        }
    }
    return -1;
}

void map_free(CMap **d)
{
    if (NULL != *d) {
        free((*d)->items);
    }
    free(*d);
}

u16 tobig_16(u16 i)
{
    unsigned char i0 = (i & 0x000000FF);
    unsigned char i1 = (i & 0x0000FF00) >> 8;

    return i0 << 8 | i1;
}

u32 tobig_24(u32 i)
{
    unsigned char i0 = (i & 0x000000FF);
    unsigned char i1 = (i & 0x0000FF00) >> 8;
    unsigned char i2 = (i & 0x00FF0000) >> 16;

    // 00 | i0 | i1 | i2
    return i0 << 16 | i1 << 8 | i2;
}

u32 tobig(u32 i)
{
    unsigned char i0 = (i & 0x000000FF);
    unsigned char i1 = (i & 0x0000FF00) >> 8;
    unsigned char i2 = (i & 0x00FF0000) >> 16;
    unsigned char i3 = (i & 0xFF000000) >> 24;

    return i0 << 24 | i1 << 16 | i2 << 8 | i3;
}

u64 tobig_64(u64 l) {
    u64 l0 = (l & 0x00000000000000FF);
    u64 l1 = (l & 0x000000000000FF00) >> 8;
    u64 l2 = (l & 0x0000000000FF0000) >> 16;
    u64 l3 = (l & 0x00000000FF000000) >> 24;
    u64 l4 = (l & 0x000000FF00000000) >> 32;
    u64 l5 = (l & 0x0000FF0000000000) >> 40;
    u64 l6 = (l & 0x00FF000000000000) >> 48;
    u64 l7 = (l & 0xFF00000000000000) >> 56;

    return l0 << 56 | l1 << 48 | l2 << 40 | l3 << 32 |
           l4 << 24 | l5 << 16 | l6 << 8  | l7;
}

u32 uchars2uint(char chars[]) {
    return chars[3] << 24 | chars[2] << 16 | chars[1] << 8 | chars[0];
}

u64 get_utc_time()
{
    time_t t = time(NULL);

    u64 seconds = t + 0x7C25B080; // 1970 based -> 1904 based
    return seconds;
}

void remove_char(char *src, char c) {
    int i = 0, j = 0;
    for (; src[i] != '\0'; i++) {
        if (src[i] != c) {
            src[j++] = src[i];
        }
    }
    src[j] = '\0';
}

void remove_chars(char *src, const char *chars) {
    int i = 0, j = 0;
    for (; src[i] != '\0'; i++) {

        int is_reserve = 1;
        for (int k = 0; chars[k] != '\0'; k++) {
            if (src[i] == chars[k]) {
                is_reserve = 0;
                break;
            }
        }
        if (is_reserve) {
            src[j++] = src[i];
        }
    }
    src[j] = '\0';
}

/**
  char *dst[10] = {0}, **tmp = dst;
  char array[100] = "abc,xyz";
  char *src = array;
  int count = 0;
  split_string(src, ",", &tmp, &count);
 */
void split_string(char *src, const char *delim, char ***dst, int *count) {
    char *token;

    int i = 0;
    while ((token = strsep(&src, delim)) != NULL) {
        (*dst)[i] = token;
        i++;
    }
    *count = i;
}

int file_exist(const char*file_path) {
    int is_exist = 0;

    FILE *pf = fopen(file_path, "rb");
    if (pf) {
        is_exist = 1;
        fclose(pf);
    }
    return is_exist;
}

void fwrite_u16(u16 u) {
    u16 big_endian_u = tobig_16(u);
    fwrite(&big_endian_u, sizeof(u16), 1, g_out_file);
}

void fwrite_u24(u32 u) {
    u32 big_endian_u = tobig_24(u);
    fwrite(&big_endian_u, sizeof(u32) - 1, 1, g_out_file);
}

void fwrite_u32(u32 u) {
    u32 big_endian_u = tobig(u);
    fwrite(&big_endian_u, sizeof(u32), 1, g_out_file);
}

void fwrite_u64(u64 u) {
    u64 big_endian_u = tobig_64(u);
    fwrite(&big_endian_u, sizeof(u64), 1, g_out_file);
}

/**
 * @brief write box header
 * refer to 4.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class Box (unsigned int(32) boxtype, optional unsigned int(8)[16] extended_type) { 
        unsigned int(32) size; 
        unsigned int(32) type = boxtype; 
        if (size==1) { 
            unsigned int(64) largesize; 
        } else if (size==0) { 
            // box extends to end of file 
        } 
        if (boxtype==‘uuid’) { 
            unsigned int(8)[16] usertype = extended_type; 
        } 
    } 
 * @param size          box size, -1 means unknown value, 1 means large size (alreay convert to big endian)
 * @param box_type      box type (alreay convert to big endian)
 * @param extended_type todo
 * @param size_offset   offset of size's field in file
 * @param size_bytes    byte numbers of box size field
 * @return return 0
 */
int write_box(u32 size, u32 box_type, u8 extended_type[16], off_t *size_offset, int *size_bytes) {
    // size
    *size_offset = ftello(g_out_file);
    *size_bytes = 4;
    fwrite(&size, sizeof(u32), 1, g_out_file);

    // type
    fwrite(&box_type, sizeof(u32), 1, g_out_file);

    // large size
    if (tobig(1) == size) {
        *size_offset = ftello(g_out_file);
        *size_bytes = 8;
        u64 large_size = 0;
        fwrite(&large_size, sizeof(u64), 1, g_out_file);
    }

    // extended box
    if (box_type == uchars2uint("uuid")) {
        fwrite(extended_type, sizeof(u8), 16, g_out_file);
    }

    return 0;
}

/**
 * @brief write full box header
 * refer to 4.2 in ISO/IEC 14496-12
 * pseudocode:
    aligned(8) class FullBox(unsigned int(32) boxtype, unsigned int(8) v, bit(24) f) extends Box(boxtype) { 
        unsigned int(8) version = v; 
        bit(24) flags = f; 
    } 
 * @param size           box size, -1 means unknown value, 1 means large size (alreay convert to big endian)
 * @param box_type       box type (alreay convert to big endian)
 * @param extended_type  todo
 * @param version        version
 * @param flags          flags
 * @param size_offset    offset of size's field in file
 * @param size_bytes     byte numbers of box size field
 * @return return 0 
 */
int write_full_box(u32 size, u32 box_type, u8 extended_type[16], u8 version, u32 flags,
                   off_t *size_offset, int *size_bytes) {
    write_box(size, box_type, extended_type, size_offset, size_bytes);

    fwrite(&version, sizeof(u8), 1, g_out_file);
    fwrite_u24(flags);

    return 0;
}

/**
 * @brief update box actual size after all the fields have been written
 * 
 * @param box_size   box actual size 
 * @param size_bytes byte numbers of box size field
 */
void update_box_size(u64 box_size, off_t size_offset, int size_bytes) {
    if (4 == size_bytes) {
        u32 actual_size = tobig(box_size);
        u64 curr_offset  = ftello(g_out_file);

        fseeko(g_out_file, size_offset, SEEK_SET);
        fwrite(&actual_size, sizeof(u32), 1, g_out_file);
        fseeko(g_out_file, curr_offset, SEEK_SET);
    } else if (8 == size_bytes) {
        u64 actual_size = tobig_64(box_size);
        u64 curr_offset  = ftello(g_out_file);

        fseeko(g_out_file, size_offset, SEEK_SET);
        fwrite(&actual_size, sizeof(u64), 1, g_out_file);
        fseeko(g_out_file, curr_offset, SEEK_SET);
    }
}