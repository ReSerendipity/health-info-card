#include "jpeg_store.h"

#include "esp_partition.h"

#include <string.h>

// imgstore partition is 0x20000 (128 KiB). Layout:
//   0x0000..0x0FFF  header: 4 slot descriptors, 8 bytes each (magic[4]+len[4])
//   0x1000..0x8FFF  slot 0 data (32 KiB)
//   0x9000..0x10FFF slot 1 data (32 KiB)
//   0x11000..0x18FFF slot 2 data (32 KiB)
//   0x19000..0x1FFFF slot 3 data (28 KiB)
// Every slot offset/size is 4 KiB aligned for erase safety.

static const char MAGIC[4] = {'J', 'P', 'G', '1'};
static const uint32_t SLOT_OFFSET[JPEG_STORE_SLOTS] = {
    0x1000u, 0x9000u, 0x11000u, 0x19000u,
};
static const uint32_t SLOT_AREA[JPEG_STORE_SLOTS] = {
    0x8000u, 0x8000u, 0x8000u, 0x7000u,
};

static int s_cur_slot = -1;
static uint32_t s_write_offset;
static esp_partition_mmap_handle_t s_source_map;
static esp_partition_mmap_handle_t s_frame_map;
static bool s_source_mapped;
static bool s_frame_mapped;
static int s_mapped_slot = -1;

static const esp_partition_t *partition_named(const char *name)
{
    return esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                    ESP_PARTITION_SUBTYPE_ANY, name);
}

static bool slot_valid(int slot)
{
    return slot >= 0 && slot < JPEG_STORE_SLOTS;
}

static uint32_t slot_stored_length(const esp_partition_t *p, int slot)
{
    uint8_t hdr[8];
    if (!p || !slot_valid(slot) ||
        esp_partition_read(p, (uint32_t)(slot * 8), hdr, sizeof(hdr)) != ESP_OK ||
        memcmp(hdr, MAGIC, sizeof(MAGIC)) != 0) {
        return 0;
    }
    return (uint32_t)hdr[4] | ((uint32_t)hdr[5] << 8) |
           ((uint32_t)hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
}

int jpeg_store_slot_begin(int slot, uint32_t total_length)
{
    const esp_partition_t *p = partition_named("imgstore");
    if (!p || !slot_valid(slot) || total_length > JPEG_STORE_SLOT_MAX) return -1;
    if (esp_partition_erase_range(p, SLOT_OFFSET[slot], SLOT_AREA[slot]) != ESP_OK) {
        return -2;
    }
    s_cur_slot = slot;
    s_write_offset = 0;
    return 0;
}

int jpeg_store_slot_write(const uint8_t *data, int length)
{
    const esp_partition_t *p = partition_named("imgstore");
    if (!p || !data || length < 0 || s_cur_slot < 0) return -1;
    uint32_t off = SLOT_OFFSET[s_cur_slot] + s_write_offset;
    if (off + (uint32_t)length > SLOT_OFFSET[s_cur_slot] + SLOT_AREA[s_cur_slot]) {
        return -1;
    }
    if (esp_partition_write(p, off, data, (size_t)length) != ESP_OK) return -2;
    s_write_offset += (uint32_t)length;
    return 0;
}

int jpeg_store_slot_end(int slot)
{
    const esp_partition_t *p = partition_named("imgstore");
    if (!p || !slot_valid(slot) || s_cur_slot != slot || s_write_offset == 0) return -1;
    // Read the whole 4 KiB header sector, blank this slot's descriptor into it,
    // erase the sector, then write it back. This preserves other slots.
    static uint8_t hdr_sector[0x1000];
    int rc = -2;
    if (esp_partition_read(p, 0, hdr_sector, sizeof(hdr_sector)) != ESP_OK) goto out;
    memcpy(hdr_sector + slot * 8, MAGIC, 4);
    hdr_sector[slot * 8 + 4] = s_write_offset & 0xFF;
    hdr_sector[slot * 8 + 5] = (s_write_offset >> 8) & 0xFF;
    hdr_sector[slot * 8 + 6] = (s_write_offset >> 16) & 0xFF;
    hdr_sector[slot * 8 + 7] = (s_write_offset >> 24) & 0xFF;
    if (esp_partition_erase_range(p, 0, 0x1000) != ESP_OK) goto out;
    if (esp_partition_write(p, 0, hdr_sector, sizeof(hdr_sector)) == ESP_OK) rc = 0;
out:
    s_cur_slot = -1;
    s_write_offset = 0;
    return rc;
}

bool jpeg_store_slot_has(int slot)
{
    const esp_partition_t *p = partition_named("imgstore");
    uint32_t n = slot_stored_length(p, slot);
    return n > 0 && n <= SLOT_AREA[slot];
}

int jpeg_store_slot_count_valid(void)
{
    const esp_partition_t *p = partition_named("imgstore");
    int count = 0;
    for (int i = 0; i < JPEG_STORE_SLOTS; ++i) {
        if (slot_stored_length(p, i) > 0) ++count;
    }
    return count;
}

int jpeg_store_slot_mmap(int slot, const uint8_t **data, int *length)
{
    const esp_partition_t *p = partition_named("imgstore");
    uint32_t stored = slot_stored_length(p, slot);
    if (!p || !data || !length || !slot_valid(slot) || stored == 0 ||
        stored > SLOT_AREA[slot]) {
        return -1;
    }
    if (s_source_mapped) jpeg_store_unmap();
    const void *mapped = NULL;
    if (esp_partition_mmap(p, SLOT_OFFSET[slot], stored,
                           ESP_PARTITION_MMAP_DATA, &mapped,
                           &s_source_map) != ESP_OK) {
        return -2;
    }
    s_source_mapped = true;
    s_mapped_slot = slot;
    *data = mapped;
    *length = (int)stored;
    return 0;
}

void jpeg_store_unmap(void)
{
    if (!s_source_mapped) return;
    esp_partition_munmap(s_source_map);
    s_source_mapped = false;
    s_mapped_slot = -1;
}

void jpeg_store_clear(void)
{
    const esp_partition_t *p = partition_named("imgstore");
    if (!p) return;
    (void)esp_partition_erase_range(p, 0, 0x20000);
}

void jpeg_store_slot_clear(int slot)
{
    const esp_partition_t *p = partition_named("imgstore");
    if (!p || !slot_valid(slot)) return;
    static uint8_t hdr_sector[0x1000];
    if (esp_partition_read(p, 0, hdr_sector, sizeof(hdr_sector)) != ESP_OK) return;
    memset(hdr_sector + slot * 8, 0xFF, 8);
    if (esp_partition_erase_range(p, 0, 0x1000) != ESP_OK) return;
    (void)esp_partition_write(p, 0, hdr_sector, sizeof(hdr_sector));
    (void)esp_partition_erase_range(p, SLOT_OFFSET[slot], SLOT_AREA[slot]);
}

int jpeg_store_begin(uint32_t total_length) { return jpeg_store_slot_begin(0, total_length); }
int jpeg_store_write(const uint8_t *data, int length) { return jpeg_store_slot_write(data, length); }
int jpeg_store_end(void) { return jpeg_store_slot_end(0); }
bool jpeg_store_has_valid(void) { return jpeg_store_slot_count_valid() > 0; }
int jpeg_store_mmap(const uint8_t **data, int *length) { return jpeg_store_slot_mmap(0, data, length); }

// ---- decode-frame buffer on the separate `imgframe` partition ----

int jpeg_frame_begin(uint32_t byte_count)
{
    const esp_partition_t *p = partition_named("imgframe");
    if (!p || byte_count > p->size) return -1;
    uint32_t erase_length = (byte_count + 0xFFFu) & ~0xFFFu;
    return esp_partition_erase_range(p, 0, erase_length) == ESP_OK ? 0 : -2;
}

int jpeg_frame_write(uint32_t offset, const uint8_t *data, int length)
{
    const esp_partition_t *p = partition_named("imgframe");
    if (!p || !data || length < 0 || offset + (uint32_t)length > p->size) return -1;
    return esp_partition_write(p, offset, data, (size_t)length) == ESP_OK ? 0 : -2;
}

int jpeg_frame_mmap(const uint8_t **data, uint32_t byte_count)
{
    const esp_partition_t *p = partition_named("imgframe");
    if (!p || !data || byte_count > p->size) return -1;
    if (s_frame_mapped) jpeg_frame_unmap();
    const void *mapped = NULL;
    if (esp_partition_mmap(p, 0, byte_count, ESP_PARTITION_MMAP_DATA,
                           &mapped, &s_frame_map) != ESP_OK) return -2;
    s_frame_mapped = true;
    *data = mapped;
    return 0;
}

void jpeg_frame_unmap(void)
{
    if (!s_frame_mapped) return;
    esp_partition_munmap(s_frame_map);
    s_frame_mapped = false;
}

uint32_t jpeg_frame_capacity(void)
{
    const esp_partition_t *p = partition_named("imgframe");
    return p ? p->size : 0;
}
