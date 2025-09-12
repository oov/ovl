#include "id3v2.c"
#include <ovtest.h>
#include <string.h>

static void test_iso8859_1_to_utf8(void) {
  uint8_t input[256];
  char output[512];
  for (int i = 0; i < 256; i++) {
    input[i] = (uint8_t)i;
  }
  size_t const len = iso8859_1_to_utf8((char *)(void *)input, 256, output, sizeof(output));
  size_t j = 0;
  for (int i = 0; i < 256; i++) {
    if (i < 128) {
      TEST_CHECK(output[j] == (char)i);
      j++;
      continue;
    }
    uint8_t const expected0 = 0xc0 | (uint8_t)(i >> 6);
    uint8_t const expected1 = 0x80 | (uint8_t)(i & 0x3f);
    uint8_t const got0 = (uint8_t)output[j++];
    uint8_t const got1 = (uint8_t)output[j++];
    TEST_CHECK(got0 == expected0 && got1 == expected1);
    TEST_MSG("i=%d, expected0=%d, expected1=%d, got0=%d, got1=%d", i, expected0, expected1, got0, got1);
  }
  TEST_CHECK(output[len] == '\0');
}

static void test_utf16_to_utf8(void) {
  struct test_case {
    const char *name;
    enum id3v2_encoding encoding;
    char *input;
    size_t len;
  } test_cases[] = {
      {"LE BOM", id3v2_encoding_utf_16_with_bom, (char[]){0xff, 0xfe, 0x41, 0x00, 0x42, 0x00, 0x43, 0x00}, 8},
      {"BE BOM", id3v2_encoding_utf_16_with_bom, (char[]){0xfe, 0xff, 0x00, 0x41, 0x00, 0x42, 0x00, 0x43}, 8},
      {"BE without BOM", id3v2_encoding_utf_16be_without_bom, (char[]){0x00, 0x41, 0x00, 0x42, 0x00, 0x43}, 6},
  };
  char output[8];
  for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++) {
    struct test_case const *const c = &test_cases[i];
    TEST_CASE(c->name);
    TEST_CHECK(utf16_to_utf8(c->input, c->len, c->encoding, output, sizeof(output)) == 3);
    TEST_CHECK(strcmp(output, "ABC") == 0);
  }
}

TEST_LIST = {
    {"test_iso8859_1_to_utf8", test_iso8859_1_to_utf8},
    {"test_utf16_to_utf8", test_utf16_to_utf8},
    {NULL, NULL},
};
