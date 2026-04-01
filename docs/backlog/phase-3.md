# Phase 3: Core Data Structures & Storage Layout

## Item 1: Define Base Enums and Structs

### User Story
As a developer, I need the central `types.h` header containing character and inventory state structs so all modules can pass them around uniformly.

### Acceptance Criteria
- [ ] `components/game/include/types.h` is created and self-contained (only standard int types included).
- [ ] Defines `fq_character_t` with ID, name, class enum, base stats, rebirth count, legacy tree bitmask, etc.
- [ ] Defines `fq_inventory_t` holding `uint16_t items[32]`.
- [ ] Structs are padded explicitly using standard C packing or ordering to prevent memory holes.

### Negative Test Requirements (from spec-challenger)
- **Padding Leakage:** Write a test that initializes a struct on the stack, writes arbitrary `0xAA` values surrounding it, and confirms `sizeof` matches exactly the byte-accumulation of fields to ensure the compiler hasn't inserted undocumented padding bytes where secrets or uninitialized stack data could leak during save serialization.
- **Name Array Overflow:** Attempt to write a 17-character name into the 16-char `name` buffer. Assert that `strncpy` or custom string bounds safely terminate at `15` without clobbering the adjacent struct field in RAM.

### Implementation Steps
1. Transcribe the Design Doc structures (1.1 Character, 1.3 Inventory) strictly to `types.h`.
2. Create class and trigger enums (`FQ_CLASS_BRUISER`, `FQ_TRIGGER_ON_ATTACK`).

### Test Expectations
- `test_sizeof.c` (or similar) executes `TEST_ASSERT_EQUAL` on the `sizeof` these structs to ensure they fit comfortably inside the 512-byte LittleFS reserve block.

### Files to Create/Modify
- `components/game/include/types.h`
- `test/host/test_types_sizeof.c`

### Commit Messages
- `feat: define core C structs for Character and Inventory`

---

## Item 2: Implement LittleFS Serialization

### User Story
As a storage engineer, I need independent field-by-field serialization of `fq_character_t` so that future schema migrations don't break if I reorder struct fields in RAM.

### Acceptance Criteria
- [ ] `components/game/include/save_format.h` exposes `fq_save_serialize` and `fq_save_deserialize`.
- [ ] Buffer encoding uses explicit bit-shifts (Little Endian format) for 16-bit and 32-bit values to avoid pointer-casting endian issues.
- [ ] Resulting byte payload is prefixed with schema `version`, payload bytes, and suffixed with `crc32`.

### Negative Test Requirements (from spec-challenger)
- **Endianness Casting Trap:** Verify the test suite rejects struct-casting directly to byte arrays. The serialization MUST use bitwise shifting (e.g. `val >> 8`), validated by feeding it `0x11223344` and ensuring the byte array is ordered `44 33 22 11` exactly.
- **Buffer Overflow:** Pass a buffer length of `10` when the serialized struct requires `50`. Ensure `fq_save_serialize` actively bounds-checks and returns `FQ_ERR_BUFFER_OVERFLOW` instead of destroying the heap.

### Implementation Steps
1. Create `save_format.c`.
2. Write serializers taking a `uint8_t *buf` and appending fields one by one (`*buf++ = val & 0xFF;`).
3. Write deserializers doing the inverse, asserting against standard size bounds.

### Test Expectations
- `test_save_format.c` populates a mock `fq_character_t`, serializes it, modifies the mock, deserializes over it, and asserts exact equality with the original.

### Files to Create/Modify
- `components/game/include/save_format.h`
- `components/game/src/save_format.c`
- `test/host/test_save_format.c`

### Commit Messages
- `feat: field-by-field binary serialization for Save files`

---

## Item 3: Validate Disk Corruption Handling

### User Story
As a QA engineer, I need to ensure that random bit flips in the save file (common on cheap flash) are caught by the CRC check rather than crashing the deserializer.

### Acceptance Criteria
- [ ] The deserializer returns `FQ_SAVE_ERR_CRC` if the payload hash mismatches the stored hash.
- [ ] Reads short buffers safely (returns `FQ_SAVE_ERR_BUFFER_TOO_SMALL`).

### Negative Test Requirements (from spec-challenger)
- **Garbage Version Byte:** Inject an unsupported high version byte `0x99` into the save payload and ensure it fails gracefully with `ERR_UNSUPPORTED_VERSION` instead of trying to parse.
- **Single Bit Flip:** Verify flipping exactly ONE bit anywhere in the 500-byte payload consistently fails the CRC check using IEEE standards.

### Implementation Steps
1. Write `test_save_corruption.c`.
2. Write to buffer, manually flip one bit inside the payload.
3. Call `fq_save_deserialize` and assert return code `FQ_SAVE_ERR_CRC`.

### Test Expectations
- All boundary constraint host tests pass.

### Files to Create/Modify
- `test/host/test_save_corruption.c`

### Commit Messages
- `test: test save corruption checks and CRC fallback`
