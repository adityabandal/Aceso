#include "Hashtable.h"

#include <stdlib.h>
#include <assert.h>

#define NUMBER64_1 11400714785074694791ULL
#define NUMBER64_2 14029467366897019727ULL
#define NUMBER64_3 1609587929392839161ULL
#define NUMBER64_4 9650029242287828579ULL
#define NUMBER64_5 2870177450012600261ULL

#define hash_get64bits(x) hash_read64_align(x, align)
#define hash_get32bits(x) hash_read32_align(x, align)
#define shifting_hash(x, r) ((x << r) | (x >> (64 - r)))
#define TO64(x) (((U64_INT *)(x))->v)
#define TO32(x) (((U32_INT *)(x))->v)

typedef struct U64_INT {
    uint64_t v;
} U64_INT;

typedef struct U32_INT {
    uint32_t v;
} U32_INT;

static uint64_t hash_read64_align(const void * ptr, uint32_t align) {
    if (align == 0) {
        return TO64(ptr);
    }
    return *(uint64_t *)ptr;
}

static uint32_t hash_read32_align(const void * ptr, uint32_t align) {
    if (align == 0) {
        return TO32(ptr);
    }
    return *(uint32_t *)ptr;
}

static uint64_t string_key_hash_computation(const void * data, uint64_t length, 
        uint64_t seed, uint32_t align) {
    const uint8_t * p = (const uint8_t *)data;
    const uint8_t * end = p + length;
    uint64_t hash;

    if (length >= 32) {
        const uint8_t * const limitation  = end - 32;
        uint64_t v1 = seed + NUMBER64_1 + NUMBER64_2;
        uint64_t v2 = seed + NUMBER64_2;
        uint64_t v3 = seed + 0;
        uint64_t v4 = seed - NUMBER64_1;

        do {
            v1 += hash_get64bits(p) * NUMBER64_2;
            p += 8;
            v1 = shifting_hash(v1, 31);
            v1 *= NUMBER64_1;
            v2 += hash_get64bits(p) * NUMBER64_2;
            p += 8;
            v2 = shifting_hash(v2, 31);
            v2 *= NUMBER64_1;
            v3 += hash_get64bits(p) * NUMBER64_2;
            p += 8;
            v3 = shifting_hash(v3, 31);
            v3 *= NUMBER64_1;
            v4 += hash_get64bits(p) * NUMBER64_2;
            p += 8;
            v4 = shifting_hash(v4, 31);
            v4 *= NUMBER64_1;
        } while (p <= limitation);

        hash = shifting_hash(v1, 1) + shifting_hash(v2, 7) + shifting_hash(v3, 12) + shifting_hash(v4, 18);

        v1 *= NUMBER64_2;
        v1 = shifting_hash(v1, 31);
        v1 *= NUMBER64_1;
        hash ^= v1;
        hash = hash * NUMBER64_1 + NUMBER64_4;

        v2 *= NUMBER64_2;
        v2 = shifting_hash(v2, 31);
        v2 *= NUMBER64_1;
        hash ^= v2;
        hash = hash * NUMBER64_1 + NUMBER64_4;

        v3 *= NUMBER64_2;
        v3 = shifting_hash(v3, 31);
        v3 *= NUMBER64_1;
        hash ^= v3;
        hash = hash * NUMBER64_1 + NUMBER64_4;

        v4 *= NUMBER64_2;
        v4 = shifting_hash(v4, 31);
        v4 *= NUMBER64_1;
        hash ^= v4;
        hash = hash * NUMBER64_1 + NUMBER64_4;
    } else {
        hash = seed + NUMBER64_5;
    }

    hash += (uint64_t)length;

    while (p + 8 <= end) {
        uint64_t k1 = hash_get64bits(p);
        k1 *= NUMBER64_2;
        k1 = shifting_hash(k1, 31);
        k1 *= NUMBER64_1;
        hash ^= k1;
        hash = shifting_hash(hash, 27) * NUMBER64_1 + NUMBER64_4;
        p += 8;
    }

    if (p + 4 <= end) {
        hash ^= (uint64_t)(hash_get32bits(p)) * NUMBER64_1;
        hash = shifting_hash(hash, 23) * NUMBER64_2 + NUMBER64_3;
        p += 4;
    }

    while (p < end) {
        hash ^= (*p) * NUMBER64_5;
        hash = shifting_hash(hash, 11) * NUMBER64_1;
        p ++;
    }

    hash ^= hash >> 33;
    hash *= NUMBER64_2;
    hash ^= hash >> 29;
    hash *= NUMBER64_3;
    hash ^= hash >> 32;

    return hash;
}

uint64_t VariableLengthHash(const void * data, uint64_t length, uint64_t seed) {
    if ((((uint64_t)data) & 7) == 0) {
        return string_key_hash_computation(data, length, seed, 1);
    }
    return string_key_hash_computation(data, length, seed, 0);
}

uint32_t GetFreeSlotNum(RaceHashBucket * bucket, __OUT uint32_t * free_idx) {
    *free_idx = RACE_HASH_ASSOC_NUM;
    uint32_t free_num = 0;
    for (int i = 0; i < RACE_HASH_ASSOC_NUM; i++) {
        if (IsEmptySlotInsert(&bucket->slots[i])) {
            free_num ++;
            *free_idx = i;
        }
    }
    return free_num;
}

bool IsEmptyPointer(uint8_t * pointer, uint32_t num) {
    for (int i = 0; i < num; i ++) {
        if (pointer[i] != 0) {
            return false;
        }
    }
    return true;
}

uint8_t HashIndexComputeFp(uint64_t hash) {
    uint8_t fp = 0;
    hash >>= 48;
    fp ^= hash;
    hash >>= 8;
    fp ^= hash;
    return fp;
}

uint8_t KVAddrComputeFp(uint64_t addr) {
    uint8_t fp = 0;
    fp ^= addr;
    addr >>= 8;
    fp ^= addr;
    addr >>= 8;
    fp ^= addr;
    addr >>= 8;
    fp ^= addr;
    return fp;
}


uint32_t HashIndexComputeServerId(uint64_t hash, uint32_t num_memory) {
    hash ^= hash >> 33;
    hash *= 0xFF51AFD7ED558CCDULL;
    hash ^= hash >> 33;
    hash *= 0xC4CEB9FE1A85EC53ULL;
    hash ^= hash >> 33;
    uint8_t index = hash & 0xFF;
    return index % num_memory;
}

bool CheckKey(void * r_key_addr, uint32_t r_key_len, void * l_key_addr, uint32_t l_key_len) {
    if (r_key_len != l_key_len)
        return false;

    uint64_t r_hash_value = VariableLengthHash(r_key_addr, r_key_len, 0);
    uint64_t l_hash_value = VariableLengthHash(l_key_addr, l_key_len, 0);

    if (r_hash_value != l_hash_value) 
        return false;

    return memcmp(r_key_addr, l_key_addr, r_key_len) == 0;
}