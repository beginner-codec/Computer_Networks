//421. Maximum XOR of Two Numbers in an Array
#include <string.h>
#define SIZE 1000003
static int hashTable[SIZE];
static int used[SIZE];
int hash(int x) {
    unsigned int h = (unsigned int)x;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h % SIZE;
}
void add(int x) {
    int h = hash(x);
    while (used[h] && hashTable[h] != x)
        h = (h + 1) % SIZE;
    hashTable[h] = x;
    used[h] = 1;
}
int contains(int x) {
    int h = hash(x);
    while (used[h]) {
        if (hashTable[h] == x)
            return 1;
        h = (h + 1) % SIZE;
    }
    return 0;
}
int findMaximumXOR(int* nums, int numsSize) {
    int answer = 0;
    int mask = 0;
    int bit;
    for (bit = 30; bit >= 0; bit--) {
        mask |= (1 << bit);
        memset(used, 0, sizeof(used));
        int i;
        for (i = 0; i < numsSize; i++) {
            add(nums[i] & mask);
        }
       int candidate = answer | (1 << bit);
        for (i = 0; i < numsSize; i++) {
            int prefix = nums[i] & mask;
            if (contains(prefix ^ candidate)) {
                answer = candidate;
                break;
            }
        }
    }
    return answer;
}
