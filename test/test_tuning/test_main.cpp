// Native unit tests for frequency <-> note conversion.
// Run with: pio test -e native

#include <string.h>
#include <unity.h>

#include <initializer_list>

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

// ---- Presets ----------------------------------------------------------------

static void test_standard_presets_come_first() {
    using tuning::Instrument;
    const tuning::Preset& g = tuning::preset(Instrument::Guitar, 0);
    TEST_ASSERT_EQUAL_STRING("E Standard", g.name);
    const int guitar[] = {40, 45, 50, 55, 59, 64};
    TEST_ASSERT_EQUAL_size_t(6, g.count);
    TEST_ASSERT_EQUAL_INT_ARRAY(guitar, g.strings, 6);

    const tuning::Preset& b = tuning::preset(Instrument::Bass, 0);
    const int bass[] = {28, 33, 38, 43};
    TEST_ASSERT_EQUAL_size_t(4, b.count);
    TEST_ASSERT_EQUAL_INT_ARRAY(bass, b.strings, 4);
}

static void test_presets_are_well_formed() {
    using tuning::Instrument;
    for (Instrument inst : {Instrument::Guitar, Instrument::Bass}) {
        TEST_ASSERT_GREATER_THAN(1, tuning::presetCount(inst));
        for (size_t i = 0; i < tuning::presetCount(inst); i++) {
            const tuning::Preset& p = tuning::preset(inst, i);
            TEST_ASSERT_TRUE(p.count >= 4 && p.count <= tuning::MAX_STRINGS);
            for (size_t s = 0; s < p.count; s++) {
                // Lowest string first, and every string within the detector's range.
                if (s) TEST_ASSERT_TRUE_MESSAGE(p.strings[s] > p.strings[s - 1], p.name);
                TEST_ASSERT_TRUE_MESSAGE(tuning::hzOf(p.strings[s]) > tuning::lowestHz(inst) * 1.05f, p.name);
            }
        }
    }
}

static void test_out_of_range_preset_index_falls_back() {
    TEST_ASSERT_EQUAL_STRING("E Standard", tuning::preset(tuning::Instrument::Bass, 99).name);
}

static void test_nearest_string() {
    const tuning::Preset& g = tuning::preset(tuning::Instrument::Guitar, 0);
    TEST_ASSERT_EQUAL_INT(0, tuning::nearestString(g, 40.0f));    // low E in tune
    TEST_ASSERT_EQUAL_INT(0, tuning::nearestString(g, 39.2f));    // low E, 80 c flat
    TEST_ASSERT_EQUAL_INT(1, tuning::nearestString(g, 43.0f));    // G2: nearer A (2) than E (3)
    TEST_ASSERT_EQUAL_INT(4, tuning::nearestString(g, 58.6f));    // B string, 40 c flat
    TEST_ASSERT_EQUAL_INT(5, tuning::nearestString(g, 64.3f));    // high E
    TEST_ASSERT_EQUAL_INT(-1, tuning::nearestString(g, 76.0f));   // an octave above: no string
    TEST_ASSERT_EQUAL_INT(-1, tuning::nearestString(g, 30.0f));   // far below
}

static void test_flat_spelling() {
    TEST_ASSERT_EQUAL_STRING("D#", tuning::nameOf(63));
    TEST_ASSERT_EQUAL_STRING("Eb", tuning::nameOf(63, true));
    TEST_ASSERT_EQUAL_STRING("Bb", tuning::nameOf(58, true));
    TEST_ASSERT_EQUAL_STRING("E", tuning::nameOf(64, true));  // naturals unchanged
}

static void test_eb_standard_is_written_with_flats() {
    using tuning::Instrument;
    const char* expected[] = {"Eb", "Ab", "Db", "Gb", "Bb", "Eb"};
    for (Instrument inst : {Instrument::Guitar, Instrument::Bass}) {
        for (size_t i = 0; i < tuning::presetCount(inst); i++) {
            const tuning::Preset& p = tuning::preset(inst, i);
            // Only Eb standard uses flats; Open D keeps its F#.
            TEST_ASSERT_EQUAL_MESSAGE(strcmp(p.name, "Eb Standard") == 0, p.flats, p.name);
            if (!p.flats) continue;
            for (size_t s = 0; s < p.count; s++) {
                TEST_ASSERT_EQUAL_STRING(expected[s], tuning::nameOf(p.strings[s], p.flats));
            }
        }
    }
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
    RUN_TEST(test_standard_presets_come_first);
    RUN_TEST(test_presets_are_well_formed);
    RUN_TEST(test_out_of_range_preset_index_falls_back);
    RUN_TEST(test_nearest_string);
    RUN_TEST(test_flat_spelling);
    RUN_TEST(test_eb_standard_is_written_with_flats);
    return UNITY_END();
}
