#pragma once
#include <Adafruit_GFX.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h> // https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-DMA/issues/433

// Ecran virtuel 64x96 formé des panneaux empilés verticalement.
// Convertit les coordonnées (x,y) en position dans la chaîne physique, et retourne
// de 180° les panneaux indiqués par PANEL_FLIP_MASK (config.h).
class PanelStack : public Adafruit_GFX {
public:
	PanelStack(MatrixPanel_I2S_DMA* dma, int panelW, int panelH, int rows, bool firstAtTop, uint8_t flipMask);
	void drawPixel(int16_t x, int16_t y, uint16_t color) override;
	// fillScreen() commence chaque image : on attend d'abord que le tampon soit vraiment libre
	void fillScreen(uint16_t color) override { waitBackBufferFree(); dma->fillScreen(color); }
	void flipDMABuffer() { dma->flipDMABuffer(); flippedAt = micros(); }
	uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return dma->color565(r, g, b); }
private:
	MatrixPanel_I2S_DMA* dma;
	int panelW, panelH, rows;
	bool firstAtTop;
	uint8_t flipMask;
	uint32_t flippedAt = 0;
	// Sur ESP32-S3, la bascule n'a lieu qu'à la fin de l'image en cours d'affichage : dessiner
	// tout de suite dans l'ancien tampon (encore affiché) provoque un clignotement.
	void waitBackBufferFree();
};

// C'est sur cet écran qu'on dessine
extern PanelStack* matrix;

#define ZONE_H 32 // hauteur d'une zone = un panneau

void displaySetup(); // appeler settingsLoad() avant (luminosité)
void displaySetBrightness(uint8_t b); // 0..255

// Centre un texte (police par défaut 6x8) horizontalement dans [x0, x0+width[
void printCentered(const char* txt, int x0, int width, int y);
// Centre un texte en police GFX dans la boîte (x0,y0,w,h)
void printCenteredGFX(const char* txt, int x0, int y0, int w, int h);
