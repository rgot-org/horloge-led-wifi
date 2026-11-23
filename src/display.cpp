#include "display.h"
#include "config.h"
#include "settings.h"

static MatrixPanel_I2S_DMA* dma_display = nullptr; // chaîne physique 192x32
PanelStack* matrix = nullptr;                      // écran virtuel 64x96

void displaySetup() {
	HUB75_I2S_CFG::i2s_pins pins = {
		WF2_X1_R1_PIN, WF2_X1_G1_PIN, WF2_X1_B1_PIN, WF2_X1_R2_PIN, WF2_X1_G2_PIN, WF2_X1_B2_PIN,
		WF2_A_PIN, WF2_B_PIN, WF2_C_PIN, WF2_D_PIN, WF2_X1_E_PIN, WF2_LAT_PIN, WF2_OE_PIN, WF2_CLK_PIN };
	HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);
	mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_8M;       // horloge la plus lente = signal plus propre
	mxconfig.clkphase = PANEL_CLKPHASE;
	mxconfig.latch_blanking = PANEL_LAT_BLANKING;
	mxconfig.double_buff = true;

	dma_display = new MatrixPanel_I2S_DMA(mxconfig);
	dma_display->begin();
#if PANEL_SOFT_EDGES
	// Fronts moins raides sur les signaux de contrôle -> moins de rebonds sur la nappe
	// (un rebond sur CLK est compté comme un front en trop et décale des pixels)
	gpio_set_drive_capability((gpio_num_t)WF2_CLK_PIN, GPIO_DRIVE_CAP_0);
	gpio_set_drive_capability((gpio_num_t)WF2_LAT_PIN, GPIO_DRIVE_CAP_0);
	gpio_set_drive_capability((gpio_num_t)WF2_OE_PIN,  GPIO_DRIVE_CAP_0);
#endif
	Serial.printf("Rafraichissement panneaux : %d Hz\n", dma_display->calculated_refresh_rate);
	dma_display->setBrightness8(settings.brightness);
	dma_display->clearScreen();
	matrix = new PanelStack(dma_display, PANEL_RES_X, PANEL_RES_Y, PANEL_ROWS, PANEL_FIRST_AT_TOP, PANEL_FLIP_MASK);
}

void printCentered(const char* txt, int x0, int width, int y) {
	int w = strlen(txt) * 6 - 1;
	matrix->setCursor(x0 + (width - w) / 2, y);
	matrix->print(txt);
}

void printCenteredGFX(const char* txt, int x0, int y0, int w, int h) {
	int16_t bx, by; uint16_t bw, bh;
	matrix->getTextBounds(txt, 0, 0, &bx, &by, &bw, &bh);
	matrix->setCursor(x0 + (w - (int)bw) / 2 - bx, y0 + (h - (int)bh) / 2 - by);
	matrix->print(txt);
}

// ---------- PanelStack ----------
PanelStack::PanelStack(MatrixPanel_I2S_DMA* d, int w, int h, int r, bool top, uint8_t mask)
	: Adafruit_GFX(w, h * r), dma(d), panelW(w), panelH(h), rows(r), firstAtTop(top), flipMask(mask) {}

void PanelStack::drawPixel(int16_t x, int16_t y, uint16_t color) {
	if (x < 0 || y < 0 || x >= panelW || y >= panelH * rows) return;
	int row = y / panelH;      // 0 = panneau du haut
	int px = x, py = y % panelH;
	if (flipMask & (1 << row)) { // panneau retourné de 180°
		px = panelW - 1 - px;
		py = panelH - 1 - py;
	}
	// Dans la chaîne DMA, le panneau branché sur l'ESP correspond aux x les plus grands
	int segment = firstAtTop ? (rows - 1 - row) : row;
	dma->drawPixel(segment * panelW + px, py, color);
}

void displaySetBrightness(uint8_t b) {
	dma_display->setBrightness8(b);
}

// Attend la fin d'un rafraîchissement complet après la dernière bascule de tampon
void PanelStack::waitBackBufferFree() {
	int hz = dma->calculated_refresh_rate > 0 ? dma->calculated_refresh_rate : 60;
	uint32_t frameUs = 1000000UL / hz + 500; // une image + marge
	while (micros() - flippedAt < frameUs) delayMicroseconds(200);
}
