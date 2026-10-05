#include "debug_dump.h"

#include "audio_in.h"

namespace debug_dump {

namespace {

int16_t* buf = nullptr;
size_t len = 0;
size_t got = 0;
uint32_t startIndex = 0;
bool isActive = false;
String statusText;

void finish(const String& message) {
    free(buf);
    buf = nullptr;
    isActive = false;
    statusText = message;
}

void send() {
    // Block (up to a second per write) rather than drop data if the PC falls
    // behind; a checksum lets tools/capture.py detect anything that slips.
    Serial.setTxTimeoutMs(1000);
    Serial.printf("#DUMP_BEGIN rate=%lu samples=%u pga=%u hpf=%d\n",
                  (unsigned long)audio_in::SAMPLE_RATE, (unsigned)len,
                  (unsigned)audio_in::pgaStep(), (int)audio_in::highPass());
    uint32_t sum = 0;
    char line[16 * 7 + 2];
    for (size_t i = 0; i < len; i += 16) {
        int n = 0;
        for (size_t j = i; j < i + 16 && j < len; j++) {
            n += snprintf(line + n, sizeof(line) - n, j == i ? "%d" : ",%d", buf[j]);
            sum += (uint32_t)(int32_t)buf[j];
        }
        Serial.println(line);
    }
    Serial.printf("#DUMP_END sum=%lu\n", (unsigned long)sum);
    Serial.flush();
    Serial.setTxTimeoutMs(100);
}

}  // namespace

void start() {
    if (isActive) return;
    if (!Serial) {
        statusText = "no serial host";
        return;
    }
    static constexpr size_t WANT[] = {48000, 32000};  // 3 s, or 2 s if short on RAM
    for (size_t want : WANT) {
        buf = (int16_t*)malloc(want * sizeof(int16_t));
        if (buf) {
            len = want;
            break;
        }
    }
    if (!buf) {
        statusText = "out of memory";
        return;
    }
    got = 0;
    startIndex = audio_in::sampleCount();
    isActive = true;
    statusText = "recording...";
}

void service() {
    if (!isActive) return;
    size_t ready = audio_in::sampleCount() - (startIndex + got);
    size_t take = min(ready, len - got);
    take = min(take, (size_t)2048);
    if (take && !audio_in::copy(startIndex + got, buf + got, take)) {
        finish("fell behind, try again");
        return;
    }
    got += take;
    if (got == len) {
        statusText = "sending...";
        send();
        finish(String("sent ") + len + " samples");
    }
}

bool active() { return isActive; }

const String& status() { return statusText; }

}  // namespace debug_dump
