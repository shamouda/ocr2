/**
 * @brief Utility functions for OCR
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-hal.h"
#include "debug.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/******************************************************/
/*  ABORT / EXIT OCR                                  */
/******************************************************/
// BUG #590: These need to move. SAL or HAL?
void ocr_abort() {
    hal_abort();
}

void ocr_exit() {
    hal_exit(1);
}

// These operations could be replaced by hardware versions of them. Keeping general
// for now
// FLS: Find last set:
//  - returns the MSB that is set.
//  - WARNING: returns 0 for both input of 0 and 1
// CTZ: Count trailing zeros:
//  - returns the number of trailing zeros, so for 100b, returns 2 which is the bit
//    position of the LSB set
//  - If If the input value is 0, returns the number of bits -1 of the size of the input
//    (so 15, 31 or 63)

u32 fls16(u16 val) {
    u32 bit = 15;

    if(!(val & 0xff00)) {
        val <<= 8;
        bit -= 8;
    }
    if(!(val & 0xf000)) {
        val <<= 4;
        bit -= 4;
    }
    if(!(val & 0xc000)) {
        val <<= 2;
        bit -= 2;
    }
    if(!(val & 0x8000)) {
        val <<= 1;
        bit -= 1;
    }

    return bit;
}

u32 fls32(u32 val) {
    u32 bit = 31;

    if(!(val & 0xffff0000)) {
        val <<= 16;
        bit -= 16;
    }
    if(!(val & 0xff000000)) {
        val <<= 8;
        bit -= 8;
    }
    if(!(val & 0xf0000000)) {
        val <<= 4;
        bit -= 4;
    }
    if(!(val & 0xc0000000)) {
        val <<= 2;
        bit -= 2;
    }
    if(!(val & 0x80000000)) {
        val <<= 1;
        bit -= 1;
    }

    return bit;
}

u32 fls64(u64 val) {
    u32 bit = 63;

    if(!(val & 0xffffffff00000000)) {
        val <<= 32;
        bit -= 32;
    }
    if(!(val & 0xffff000000000000)) {
        val <<= 16;
        bit -= 16;
    }
    if(!(val & 0xff00000000000000)) {
        val <<= 8;
        bit -= 8;
    }
    if(!(val & 0xf000000000000000)) {
        val <<= 4;
        bit -= 4;
    }
    if(!(val & 0xc000000000000000)) {
        val <<= 2;
        bit -= 2;
    }
    if(!(val & 0x8000000000000000)) {
        val <<= 1;
        bit -= 1;
    }

    return bit;
}

u32 ctz16(u16 val) {
    // Fast path for odd numbers
    if (val & 0x1) {
        return 0;
    }
    u32 bit = 1;
    if ((val & 0xff) == 0) {
        val >>= 8;
        bit += 8;
    }
    if ((val & 0xf) == 0) {
        val >>= 4;
        bit += 4;
    }
    if ((val & 0x3) == 0) {
        val >>= 2;
        bit += 2;
    }
    bit -= val & 0x1;
    return bit;
}

u32 ctz32(u32 val) {
    // Fast path for odd numbers
    if (val & 0x1) {
        return 0;
    }
    u32 bit = 1;
    if ((val & 0xffff) == 0) {
        val >>= 16;
        bit += 16;
    }
    if ((val & 0xff) == 0) {
        val >>= 8;
        bit += 8;
    }
    if ((val & 0xf) == 0) {
        val >>= 4;
        bit += 4;
    }
    if ((val & 0x3) == 0) {
        val >>= 2;
        bit += 2;
    }
    bit -= val & 0x1;
    return bit;
}

u32 ctz64(u64 val) {
    // Fast path for odd numbers
    if (val & 0x1) {
        return 0;
    }
    u32 bit = 1;
    if ((val & 0xffffffff) == 0) {
        val >>= 32;
        bit += 32;
    }
    if ((val & 0xffff) == 0) {
        val >>= 16;
        bit += 16;
    }
    if ((val & 0xff) == 0) {
        val >>= 8;
        bit += 8;
    }
    if ((val & 0xf) == 0) {
        val >>= 4;
        bit += 4;
    }
    if ((val & 0x3) == 0) {
        val >>= 2;
        bit += 2;
    }
    bit -= val & 0x1;
    return bit;
}

void ocrGuidTrackerInit(ocrGuidTracker_t *self) {
    self->slotsStatus = 0xFFFFFFFFFFFFFFFFULL;
}

u32 ocrGuidTrackerTrack(ocrGuidTracker_t *self, ocrGuid_t toTrack) {
    u32 slot = 64;
    if(self->slotsStatus == 0) return slot;
    slot = fls64(self->slotsStatus);
    self->slotsStatus &= ~(1ULL<<slot);
    ASSERT(slot <= 63);
    self->slots[slot] = toTrack;
    return slot;
}

bool ocrGuidTrackerRemove(ocrGuidTracker_t *self, ocrGuid_t toTrack, u32 id) {
    if(id > 63) return false;
    if(!(ocrGuidIsEq(self->slots[id], toTrack))) return false;

    self->slotsStatus |= (1ULL<<(id));
    return true;
}

u32 ocrGuidTrackerIterateAndClear(ocrGuidTracker_t *self) {
    u64 rstatus = ~(self->slotsStatus);
    u32 slot;
    if(rstatus) return 64;
    slot = fls64(rstatus);
    self->slotsStatus |= (1ULL << slot);
    return slot;
}

u32 ocrGuidTrackerFind(ocrGuidTracker_t *self, ocrGuid_t toFind) {
    u32 result = 64, slot;
    u64 rstatus = ~(self->slotsStatus);
    while(rstatus) {
        slot = fls64(rstatus);
        rstatus &= ~(1ULL << slot);
        if(ocrGuidIsEq(self->slots[slot], toFind)) {
            result = slot;
            break;
        }
    }
    return result;
}

s32 ocrStrcmp(u8 *str1, u8 *str2) {
    u32 index = 0;
    while((str1[index] != '\0') && (str2[index] != '\0')) {
        if(str1[index] == str2[index]) index++;
        else break;
    }
    return(str1[index]-str2[index]);
}

s32 ocrStrncmp(u8 *str1, u8 *str2, u32 n) {
    if (n == 0) { return 0; }
    u32 index = 0;
    while((index < (n-1)) && (str1[index] != '\0') && (str2[index] != '\0')) {
        if(str1[index] == str2[index]) index++;
        else break;
    }
    return(str1[index]-str2[index]);
}

u64 ocrStrlen(const char* str) {
    u64 res = 0;
    if(str == NULL) return res;
    while(*str++ != '\0') ++res;
    return res;
}

bool ocrIsDigit(u8 c) {
    return ((c >= '0') && (c <= '9'));
}



/* This is not currently used. What to do with it?
void ocrPlaceTrackerAllocate ( ocrPlaceTracker_t** toFill ) {
    *toFill = (ocrPlaceTracker_t*) malloc(sizeof(ocrPlaceTracker_t));
}

void ocrPlaceTrackerInsert ( ocrPlaceTracker_t* self, unsigned char currPlace ) {
    self->existInPlaces |= (1ULL << currPlace);
}

void ocrPlaceTrackerRemove ( ocrPlaceTracker_t* self, unsigned char currPlace ) {
    self->existInPlaces &= ~(1ULL << currPlace);
}

void ocrPlaceTrackerInit( ocrPlaceTracker_t* self ) {
    self->existInPlaces = 0ULL;
}
*/
