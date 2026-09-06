[24bcs084@mepcolinux exe3]$cat hamhead.h
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
char *read_binary_string(const char *prompt);
int is_power_of_two(int x);
int calculate_r(int m);
void print_bits_msb_to_lsb(const int *bits, int n);
int *generate_hamming_code(const char *data, int m, int r, int n);
int *check_and_correct(const char *received, int n, int *error_pos_out);
void extract_original_data(const int *bits, int n);

[24bcs084@mepcolinux exe3]$cat hamimp.c
#include "hamhead.h"

/* ------------------------------------------------------------
 * read_binary_string
 *
 * Reads a line of arbitrary length from stdin, growing the
 * buffer with realloc() as needed so there is no fixed limit
 * on how much binary data the user can type. Any character
 * that is not '0' or '1' is silently skipped (this keeps the
 * function forgiving of accidental spaces, etc.).
 * ------------------------------------------------------------ */
char *read_binary_string(const char *prompt)
{
    size_t cap = 16, len = 0;
    char *buf = malloc(cap);
    if (!buf) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    printf("%s", prompt);
    fflush(stdout);

    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        if (c != '0' && c != '1') {
            continue; /* ignore anything that isn't a binary digit */
        }
        if (len + 1 >= cap) { /* +1 leaves room for the NUL terminator */
            cap *= 2;
            char *tmp = realloc(buf, cap);
            if (!tmp) {
                free(buf);
                fprintf(stderr, "Memory allocation failed.\n");
                exit(EXIT_FAILURE);
            }
            buf = tmp;
        }
        buf[len++] = (char)c;
    }
    buf[len] = '\0';

    if (len == 0) {
        free(buf);
        fprintf(stderr, "No valid binary digits ('0'/'1') were entered.\n");
        exit(EXIT_FAILURE);
    }
    return buf;
}

/* ------------------------------------------------------------
 * is_power_of_two
 * ------------------------------------------------------------ */
int is_power_of_two(int x)
{
    return (x > 0) && ((x & (x - 1)) == 0);
}

/* ------------------------------------------------------------
 * calculate_r
 *
 * Dynamically finds the smallest r such that 2^r >= m + r + 1.
 * Starts at r = 0 and grows it until the inequality holds, so
 * it works for any m without a hard-coded table.
 * ------------------------------------------------------------ */
int calculate_r(int m)
{
    int r = 0;
    while ((1 << r) < (m + r + 1)) {
        r++;
    }
    return r;
}

/* ------------------------------------------------------------
 * print_bits_msb_to_lsb
 * ------------------------------------------------------------ */
void print_bits_msb_to_lsb(const int *bits, int n)
{
   int i;
    for (i = n; i >= 1; i--) {
        putchar(bits[i] ? '1' : '0');
    }
    putchar('\n');
}

/* ------------------------------------------------------------
 * generate_hamming_code   (Part 1, steps 3-5)
 *
 * 1) Vacate the power-of-two positions and fill every other
 *    position (1..n) with the m data bits. The data string's
 *    first character (its MSB) is placed at the highest
 *    non-power-of-two position, and its last character (its
 *    LSB) at the lowest non-power-of-two position -- i.e. data
 *    bits keep their natural MSB->LSB order relative to the
 *    codeword's own MSB(n)->LSB(1) layout.
 * 2) For every vacant position 2^x, XOR together all positions
 *    whose index has bit x set, EXCLUDING the parity position
 *    itself (crucial rule for this generation pass), and store
 *    the result as that parity bit (even parity).
 * ------------------------------------------------------------ */
int *generate_hamming_code(const char *data, int m, int r, int n)
{
    (void)m; /* m is implied by walking the data string; kept for clarity */

    int *bits = calloc((size_t)n + 1, sizeof(int)); /* index 0 unused */
    if (!bits) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }

    /* Step: place data bits into all non-power-of-two positions. */
    int dataIndex = 0;
    int i,j;
    for (j = n; j >= 1; j--) {
        if (!is_power_of_two(j)) {
            bits[j] = data[dataIndex] - '0';
            dataIndex++;
        }
    }
    int x;

    /* Step: compute each parity bit with even parity, excluding its
     * own (still-vacant) position from the calculation. */
    for (x = 0; x < r; x++) {
        int parity_pos = 1 << x;
        int value = 0;
        int p;
        for (p = 1; p <= n; p++) {
            if (p == parity_pos) {
                continue; /* Crucial Rule: don't include self here */
            }
            if ((p >> x) & 1) {
                value ^= bits[p];
            }
        }
        bits[parity_pos] = value;
    }

    return bits;
}

/* ------------------------------------------------------------
 * check_and_correct   (Part 2, steps 2-6)
 *
 * 1) Parse the received string into a 1-indexed bit array using
 *    the same LSB(1)..MSB(n) convention as the sender.
 * 2) For each parity position 2^x, XOR together every position
 *    whose index has bit x set -- this time INCLUDING the
 *    parity position itself (crucial rule for the checking
 *    pass, since it now holds a real transmitted value).
 * 3) Combine the per-x syndrome bits into a single decimal
 *    number: that number is 0 if nothing is wrong, or the
 *    exact 1-based LSB position of the single flipped bit.
 * 4) If a bad bit was found, toggle it to correct the codeword.
 * ------------------------------------------------------------ */
int *check_and_correct(const char *received, int n, int *error_pos_out)
{
    int *bits = malloc(((size_t)n + 1) * sizeof(int)); /* index 0 unused */
    if (!bits) {
        fprintf(stderr, "Memory allocation failed.\n");
        exit(EXIT_FAILURE);
    }
    bits[0] = 0;

    /* received[0] is the MSB (position n); received[n-1] is the LSB
     * (position 1) -- same layout print_bits_msb_to_lsb() produces. */
    int idx = 0;
    int j;
    for (j = n; j >= 1; j--) {
        bits[j] = received[idx] - '0';
        idx++;
    }

    /* How many parity positions (2^0, 2^1, ...) fall within 1..n? */
    int r_bits = 0;
    while ((1 << r_bits) <= n) {
        r_bits++;
    }

    /* Recalculate parity for each bit position of the syndrome,
     * this time including the parity bit's own position. */
    int error_pos = 0;
    int x;
    for (x = 0; x < r_bits; x++) {
        int parity_pos = 1 << x;
        int value = 0;
        int p;
        for (p = 1; p <= n; p++) {
            if ((p >> x) & 1) {
                value ^= bits[p]; /* Crucial Rule: self IS included here */
            }
        }
        error_pos += value * parity_pos; /* build the syndrome number */
    }

    *error_pos_out = error_pos;

    if (error_pos > 0 && error_pos <= n) {
        bits[error_pos] ^= 1; /* toggle to correct the single-bit error */
    }

    return bits;
}

/* ------------------------------------------------------------
 * extract_original_data   (Part 2, step 6)
 *
 * Strips the parity bits back out, printing only the data bits
 * from MSB (position n) down to LSB (position 1) -- which
 * reproduces the original input string typed at the sender.
 * ------------------------------------------------------------ */
void extract_original_data(const int *bits, int n)
{
   int i;
    for (i = n; i >= 1; i--) {
        if (!is_power_of_two(i)) {
            putchar(bits[i] ? '1' : '0');
        }
    }
    putchar('\n');
}
[24bcs084@mepcolinux exe3]$cat hamapp.c
#include "hamhead.h"

int main(void)
{
    printf("===================================================\n");
    printf(" Hamming Code Demo - Part 1: Generation (Sender)\n");
    printf("===================================================\n");

    char *data = read_binary_string("Enter a binary data string of any length: ");
    int m = (int)strlen(data);
    int r = calculate_r(m);
    int n = m + r;

    int *encoded = generate_hamming_code(data, m, r, n);

    printf("\nData length (m)        : %d\n", m);
    printf("Parity bits needed (r)  : %d\n", r);
    printf("Total codeword length(n): %d\n", n);
    printf("Generated Hamming Code (MSB -> LSB): ");
    print_bits_msb_to_lsb(encoded, n);

    free(data);
    free(encoded);

    printf("\n===================================================\n");
    printf(" Hamming Code Demo - Part 2: Checking & Correction (Receiver)\n");
    printf("===================================================\n");
    printf("Enter the %d-bit Hamming code above, optionally flipping\n", n);
    printf("one bit to simulate a single-bit transmission error.\n");

    char *received = NULL;
    for (;;) {
        received = read_binary_string("Enter received Hamming code: ");
        if ((int)strlen(received) == n) {
            break;
        }
        printf("Expected exactly %d bits, got %d. Please try again.\n",
               n, (int)strlen(received));
        free(received);
    }

    int error_pos = 0;
    int *corrected = check_and_correct(received, n, &error_pos);

    if (error_pos == 0) {
        printf("\nNo error detected. The received code is valid.\n");
    } else {
        printf("\nError detected at position %d (counted from the LSB).\n", error_pos);
        printf("Bit at that position has been toggled to correct it.\n");
    }

    printf("Corrected Hamming Code (MSB -> LSB): ");
    print_bits_msb_to_lsb(corrected, n);

    printf("Extracted original data (MSB -> LSB): ");
    extract_original_data(corrected, n);

    free(received);
    free(corrected);

    return 0;
}
