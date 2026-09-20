#pragma once

#include <stdbool.h>
#include <stdint.h>

// Multi-slot QR image store on the `imgstore` data partition.
// Partition layout (0x20000 = 128 KiB):
//   header area 0x1000: 4 slot descriptors, 8 bytes each (magic[4] + length[4])
//   data area: 4 equal slots, each SLOT_SIZE bytes
// Slot k data starts at 0x1000 + k * SLOT_SIZE.

#define JPEG_STORE_SLOTS 4
#define JPEG_STORE_SLOT_SIZE (0x7C00u)  // 31744 bytes per slot (~31 KiB)
#define JPEG_STORE_SLOT_MAX  (0x7000u)  // 28672 bytes (~28 KiB) upload cap per slot

// Slot-scoped write/verify API.
int jpeg_store_slot_begin(int slot, uint32_t total_length);
int jpeg_store_slot_write(const uint8_t *data, int length);
int jpeg_store_slot_end(int slot);
bool jpeg_store_slot_has(int slot);
int  jpeg_store_slot_count_valid(void);
int  jpeg_store_slot_mmap(int slot, const uint8_t **data, int *length);
void jpeg_store_slot_clear(int slot);
void jpeg_store_unmap(void);
void jpeg_store_clear(void);

// Backwards-compatible wrappers over slot 0 (keeps existing call sites working).
int jpeg_store_begin(uint32_t total_length);
int jpeg_store_write(const uint8_t *data, int length);
int jpeg_store_end(void);
bool jpeg_store_has_valid(void);
int jpeg_store_mmap(const uint8_t **data, int *length);

// Decode-frame buffer on the separate `imgframe` partition (unchanged).
int jpeg_frame_begin(uint32_t byte_count);
int jpeg_frame_write(uint32_t offset, const uint8_t *data, int length);
int jpeg_frame_mmap(const uint8_t **data, uint32_t byte_count);
void jpeg_frame_unmap(void);
uint32_t jpeg_frame_capacity(void);
