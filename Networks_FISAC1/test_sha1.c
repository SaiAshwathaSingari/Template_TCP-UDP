#include "server/crypto.h"
#include <stdio.h>
#include <string.h>

void test(const char *label, const char *input, const char *expected_hex) {
    uint8_t digest[20];
    char hex[41];
    int i;

    sha1_hash((const uint8_t *)input, strlen(input), digest);
    for (i = 0; i < 20; i++) sprintf(hex + i*2, "%02x", digest[i]);
    hex[40] = '\0';

    int ok = (strcmp(hex, expected_hex) == 0);
    printf("[%s] %s: %s\n", ok ? "PASS" : "FAIL", label, input);
    if (!ok) {
        printf("  Got:      %s\n", hex);
        printf("  Expected: %s\n", expected_hex);
    }
}

int main() {
    /* NIST test vectors */
    test("abc", "abc", "a9993e364706816aba3e25717850c26c9cd0d89d");
    test("empty", "", "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    test("fox", "The quick brown fox jumps over the lazy dog",
         "2fd4e1c67a2d28fced849ee1bb76e7391b93eb12");

    /* RFC 6455 WebSocket accept key test */
    const char *key = "dGhlIHNhbXBsZSBub25jZQ==";
    const char *magic = "258EAFA5-E914-47DA-95CA-5AB4AA29BE5E";
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", key, magic);
    test("ws-key", combined, "b37a4f2cc0624f1690f64606cf385945b2bec4ea");

    return 0;
}
