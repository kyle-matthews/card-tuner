// Native unit tests for frequency <-> note conversion.
// Run with: pio test -e native

#include <string.h>
#include <unity.h>

#include "tuning.h"

static void expectNote(float hz, const char* name, int octave, float cents, float a4 = 440) {
    tuning::Note n = tuning::fromHz(hz, a4);
    TEST_ASSERT_EQUAL_STRING(name, n.name);
    TEST_ASSERT_EQUAL_INT(octave, n.octave);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, cents, n.cents);
}

static void test_reference_a4() { expectNote(440.0f, "A", 4, 0); }

static void test_open_strings() {
    expectNote(30.868f, "B", 0, 0);   // 5-string bass low B
    expectNote(41.203f, "E", 1, 0);   // bass low E
    expectNote(55.000f, "A", 1, 0);
    expectNote(82.407f, "E", 2, 0);   // guitar low E
    expectNote(146.83f, "D", 3, 0);
    expectNote(246.94f, "B", 3, 0);
    expectNote(329.63f, "E", 4, 0);   // guitar high E
}

static void test_cents_sharp_and_flat() {
    expectNote(440.0f * 1.0057929f, "A", 4, 10);   // +10 c
    expectNote(440.0f / 1.0057929f, "A", 4, -10);  // -10 c
    expectNote(440.0f * 1.0292430f, "A", 4, 49.9f);  // just short of A#
}

static void test_rounds_to_nearest_note() {
    // 51 cents above A4 is nearer A#4 (-49 c).
    expectNote(440.0f * 1.0299001f, "A#", 4, -49);
}

static void test_other_a4_reference() {
    expectNote(432.0f, "A", 4, 0, 432);
    expectNote(440.0f, "A", 4, 31.77f, 432);
}

static void test_hz_of() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 440.0f, tuning::hzOf(69));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 41.2034f, tuning::hzOf(28));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 432.0f, tuning::hzOf(69, 432));
}

static void test_low_octave_numbers() {
    TEST_ASSERT_EQUAL_INT(-1, tuning::fromHz(tuning::hzOf(0)).octave);
    TEST_ASSERT_EQUAL_STRING("C", tuning::nameOf(0));
    TEST_ASSERT_EQUAL_STRING("B", tuning::nameOf(-1));
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_reference_a4);
    RUN_TEST(test_open_strings);
    RUN_TEST(test_cents_sharp_and_flat);
    RUN_TEST(test_rounds_to_nearest_note);
    RUN_TEST(test_other_a4_reference);
    RUN_TEST(test_hz_of);
    RUN_TEST(test_low_octave_numbers);
    return UNITY_END();
}
