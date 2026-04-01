/**
 * test_partitions.c
 *
 * N4: Partition arithmetic validation.
 *
 * Parses partitions.csv (path supplied via PARTITIONS_CSV_PATH macro defined
 * in CMakeLists.txt), validates:
 *   1. All partition offset + size pairs are non-overlapping.
 *   2. The last byte of the last partition does not exceed 0x800000 (8MiB),
 *      which is the flash size of the ESP32-S3-PICO-1-N8R8.
 *   3. The sum of all partition sizes is less than or equal to 0x800000.
 *
 * Exit 0 on success, non-zero on any violation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#define FLASH_SIZE_BYTES  (0x800000u)   /* 8 MiB */
#define MAX_PARTITIONS    (16u)

typedef struct {
    char     name[32];
    char     type[16];
    char     subtype[16];
    uint32_t offset;
    uint32_t size;
} partition_entry_t;

/** Parse a hex (0x...) or decimal string into uint32_t. */
static uint32_t parse_uint32(const char *s)
{
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return (uint32_t)strtoul(s + 2, NULL, 16);
    }
    return (uint32_t)strtoul(s, NULL, 10);
}

/** Strip leading/trailing whitespace in-place. Returns pointer to start. */
static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') { s++; }
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t'
                       || end[-1] == '\n' || end[-1] == '\r')) {
        end--;
    }
    *end = '\0';
    return s;
}

int main(void)
{
    const char *csv_path = PARTITIONS_CSV_PATH;

    FILE *f = fopen(csv_path, "r");
    if (!f) {
        fprintf(stderr, "test_partitions: cannot open %s\n", csv_path);
        return 1;
    }

    partition_entry_t parts[MAX_PARTITIONS];
    uint32_t count = 0;
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        char *p = trim(line);
        /* Skip comments and blank lines. */
        if (*p == '#' || *p == '\0') { continue; }

        if (count >= MAX_PARTITIONS) {
            fprintf(stderr, "test_partitions: too many partitions (max %u)\n",
                    MAX_PARTITIONS);
            fclose(f);
            return 1;
        }

        /* Parse CSV columns: name, type, subtype, offset, size */
        char *fields[5];
        uint32_t fi = 0;
        char *tok = strtok(p, ",");
        while (tok && fi < 5) {
            fields[fi++] = trim(tok);
            tok = strtok(NULL, ",");
        }
        if (fi < 5) {
            fprintf(stderr, "test_partitions: malformed line: %s\n", p);
            fclose(f);
            return 1;
        }

        strncpy(parts[count].name,    fields[0], sizeof(parts[count].name) - 1);
        strncpy(parts[count].type,    fields[1], sizeof(parts[count].type) - 1);
        strncpy(parts[count].subtype, fields[2], sizeof(parts[count].subtype) - 1);
        parts[count].offset = parse_uint32(fields[3]);
        parts[count].size   = parse_uint32(fields[4]);
        count++;
    }
    fclose(f);

    if (count == 0) {
        fprintf(stderr, "test_partitions: no partitions parsed\n");
        return 1;
    }

    printf("test_partitions: parsed %u partitions from %s\n", count, csv_path);

    /* --- Validation 1: no partition exceeds flash boundary --- */
    for (uint32_t i = 0; i < count; i++) {
        uint64_t end = (uint64_t)parts[i].offset + (uint64_t)parts[i].size;
        if (end > FLASH_SIZE_BYTES) {
            fprintf(stderr,
                    "test_partitions: FAIL partition '%s' end=0x%08llX exceeds "
                    "flash size 0x%08X\n",
                    parts[i].name, (unsigned long long)end, FLASH_SIZE_BYTES);
            return 1;
        }
        printf("  [%u] %-20s offset=0x%06X size=0x%06X end=0x%06llX\n",
               i, parts[i].name, parts[i].offset, parts[i].size,
               (unsigned long long)end);
    }

    /* --- Validation 2: no overlapping partitions ---
     *
     * B3: Use uint64_t for a_end and b_end to prevent uint32_t wrap-around.
     * A partition with offset=0xFFF000 and size=0x002000 would produce
     * a_end=0x1001000 in 64-bit arithmetic, correctly detected as out-of-range.
     * In 32-bit arithmetic the same computation wraps to 0x001000, falsely
     * passing the overlap check. The flash-boundary check (Validation 1 above)
     * already uses uint64_t; this block must match for consistent safety.
     */
    for (uint32_t i = 0; i < count; i++) {
        for (uint32_t j = i + 1; j < count; j++) {
            uint64_t a_start = (uint64_t)parts[i].offset;
            uint64_t a_end   = (uint64_t)parts[i].offset + (uint64_t)parts[i].size;
            uint64_t b_start = (uint64_t)parts[j].offset;
            uint64_t b_end   = (uint64_t)parts[j].offset + (uint64_t)parts[j].size;

            int overlaps = (a_start < b_end) && (b_start < a_end);
            if (overlaps) {
                fprintf(stderr,
                        "test_partitions: FAIL partition '%s' [0x%llX..0x%llX) overlaps "
                        "'%s' [0x%llX..0x%llX)\n",
                        parts[i].name,
                        (unsigned long long)a_start, (unsigned long long)a_end,
                        parts[j].name,
                        (unsigned long long)b_start, (unsigned long long)b_end);
                return 1;
            }
        }
    }

    /* --- Validation 3: total size fits in 8MiB --- */
    uint64_t total = 0;
    for (uint32_t i = 0; i < count; i++) {
        total += parts[i].size;
    }
    printf("test_partitions: total partition size = 0x%08llX / 0x%08X\n",
           (unsigned long long)total, FLASH_SIZE_BYTES);
    if (total > FLASH_SIZE_BYTES) {
        fprintf(stderr,
                "test_partitions: FAIL total size 0x%08llX exceeds flash 0x%08X\n",
                (unsigned long long)total, FLASH_SIZE_BYTES);
        return 1;
    }

    printf("test_partitions: PASS — all %u partitions are valid, no overlaps, "
           "fits in 8MiB flash\n", count);
    return 0;
}
