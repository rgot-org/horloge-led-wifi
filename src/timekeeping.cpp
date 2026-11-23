#include "timekeeping.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>

#define RTC_ADDR 0x51 // BM8563 / PCF8563

static volatile bool ntpSynced = false; // positionné par le callback SNTP, traité dans timeLoop()

static uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static uint8_t bin2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

// Nombre de jours depuis le 01/01/1970, pour convertir une date UTC en time_t
static int32_t daysFromCivil(int y, unsigned m, unsigned d) {
	y -= m <= 2;
	const int32_t era = (y >= 0 ? y : y - 399) / 400;
	const unsigned yoe = (unsigned)(y - era * 400);
	const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return era * 146097 + (int32_t)doe - 719468;
}

// Lit la RTC et met l'horloge système à l'heure. Retourne false si la RTC n'est pas fiable.
static bool rtcRead() {
	Wire.beginTransmission(RTC_ADDR);
	Wire.write(0x02); // registre des secondes
	if (Wire.endTransmission(false) != 0) { Serial.println("RTC absente"); return false; }
	if (Wire.requestFrom(RTC_ADDR, 7) != 7) return false;
	uint8_t r[7];
	for (int i = 0; i < 7; i++) r[i] = Wire.read();
	if (r[0] & 0x80) { Serial.println("RTC : heure perdue (pile/batterie)"); return false; } // bit VL
	int sec = bcd2bin(r[0] & 0x7F), min = bcd2bin(r[1] & 0x7F), hour = bcd2bin(r[2] & 0x3F);
	int day = bcd2bin(r[3] & 0x3F), mon = bcd2bin(r[5] & 0x1F), year = 2000 + bcd2bin(r[6]);
	if (year < 2024 || mon < 1 || mon > 12 || day < 1) return false;
	struct timeval tv = { (time_t)daysFromCivil(year, mon, day) * 86400 + hour * 3600 + min * 60 + sec, 0 };
	settimeofday(&tv, NULL);
	Serial.printf("RTC -> %04d-%02d-%02d %02d:%02d:%02d UTC\n", year, mon, day, hour, min, sec);
	return true;
}

// Ecrit l'heure système (UTC) dans la RTC
static void rtcWrite() {
	time_t now = time(NULL);
	struct tm u;
	gmtime_r(&now, &u);
	Wire.beginTransmission(RTC_ADDR);
	Wire.write(0x02);
	Wire.write(bin2bcd(u.tm_sec)); // efface aussi le bit VL
	Wire.write(bin2bcd(u.tm_min));
	Wire.write(bin2bcd(u.tm_hour));
	Wire.write(bin2bcd(u.tm_mday));
	Wire.write(u.tm_wday);
	Wire.write(bin2bcd(u.tm_mon + 1));
	Wire.write(bin2bcd(u.tm_year % 100));
	if (Wire.endTransmission() == 0) Serial.println("NTP -> RTC mise à jour");
}

static void onNtpSync(struct timeval* tv) { ntpSynced = true; }

static void applyTimezone() {
	setenv("TZ", TIMEZONE, 1);
	tzset();
}

void timeBegin() {
	Wire.begin(WF2_BM8563_I2C_SDA, WF2_BM8563_I2C_SCL);
	rtcRead();
	applyTimezone();
}

void ntpStart() {
	sntp_set_time_sync_notification_cb(onNtpSync);
	configTime(0, 0, NTP_SERVER1, NTP_SERVER2);
	applyTimezone(); // configTime écrase TZ : on remet le fuseau avec heure d'été
}

void timeLoop() {
	if (ntpSynced) {
		ntpSynced = false;
		rtcWrite();
	}
}
