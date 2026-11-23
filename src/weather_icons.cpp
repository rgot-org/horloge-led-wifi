#include "weather_icons.h"
#include "display.h"

static void iconSun(int cx, int cy, int r) {
	uint16_t y = matrix->color565(255, 200, 0);
	matrix->fillCircle(cx, cy, r, y);
	for (int i = 0; i < 8; i++) {
		float a = i * PI / 4;
		matrix->drawLine(cx + lroundf(cosf(a) * (r + 2)), cy + lroundf(sinf(a) * (r + 2)),
		                 cx + lroundf(cosf(a) * (r + 4)), cy + lroundf(sinf(a) * (r + 4)), y);
	}
}

static void iconMoon(int cx, int cy, int r) {
	matrix->fillCircle(cx, cy, r, matrix->color565(220, 220, 120));
	matrix->fillCircle(cx + r / 2 + 1, cy - r / 2, r, 0);
}

static void iconCloud(int x, int y, uint16_t c) { // ~20x12
	matrix->fillCircle(x + 6,  y + 7, 4, c);
	matrix->fillCircle(x + 12, y + 5, 5, c);
	matrix->fillCircle(x + 17, y + 8, 3, c);
	matrix->fillRect(x + 6, y + 7, 12, 5, c);
}

static void iconRain(int ox, int oy) {
	uint16_t b = matrix->color565(0, 120, 255);
	for (int i = 0; i < 4; i++) {
		int x = ox + 5 + i * 4;
		matrix->drawLine(x + 1, oy + 16, x - 1, oy + 21, b);
	}
}

static void iconSnowflake(int x, int y, uint16_t c) {
	matrix->drawPixel(x, y, c);
	matrix->drawPixel(x - 1, y, c); matrix->drawPixel(x + 1, y, c);
	matrix->drawPixel(x, y - 1, c); matrix->drawPixel(x, y + 1, c);
}

void drawWeatherIcon(int ox, int oy, const String& icon) {
	uint16_t white = matrix->color565(200, 200, 200);
	uint16_t grey  = matrix->color565(100, 100, 100);
	bool night = icon.endsWith("n");
	int code = icon.substring(0, 2).toInt();
	switch (code) {
	case 1:  // ciel dégagé
		if (night) iconMoon(ox + 12, oy + 12, 7); else iconSun(ox + 12, oy + 12, 6);
		break;
	case 2:  // peu nuageux
		if (night) iconMoon(ox + 16, oy + 7, 5); else iconSun(ox + 16, oy + 7, 3);
		iconCloud(ox + 1, oy + 9, white);
		break;
	case 3:  // nuageux
		iconCloud(ox + 2, oy + 6, white);
		break;
	case 4:  // couvert
		iconCloud(ox + 4, oy + 3, grey);
		iconCloud(ox + 0, oy + 9, white);
		break;
	case 9:  // averses
		iconCloud(ox + 2, oy + 2, grey);
		iconRain(ox, oy);
		break;
	case 10: // pluie
		if (night) iconMoon(ox + 17, oy + 5, 4); else iconSun(ox + 17, oy + 5, 3);
		iconCloud(ox + 1, oy + 3, white);
		iconRain(ox, oy);
		break;
	case 11: { // orage
		uint16_t y = matrix->color565(255, 220, 0);
		iconCloud(ox + 2, oy + 2, grey);
		matrix->fillTriangle(ox + 14, oy + 13, ox + 8, oy + 19, ox + 13, oy + 19, y);
		matrix->fillTriangle(ox + 13, oy + 17, ox + 15, oy + 17, ox + 9, oy + 23, y);
		break;
	}
	case 13: // neige
		iconCloud(ox + 2, oy + 2, white);
		iconSnowflake(ox + 6,  oy + 17, white);
		iconSnowflake(ox + 12, oy + 20, white);
		iconSnowflake(ox + 18, oy + 17, white);
		break;
	case 50: // brouillard
		for (int i = 0; i < 4; i++)
			matrix->drawFastHLine(ox + 2 + (i % 2) * 3, oy + 6 + i * 4, 17, grey);
		break;
	default:
		matrix->setFont(); matrix->setTextColor(white);
		matrix->setCursor(ox + 9, oy + 8); matrix->print("?");
	}
}
