#ifndef MAIN_FONTX_H_
#define MAIN_FONTX_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FontxGlyphBufSize (32*32/8)

struct FontxFile {
	const char *path;
	char  fxname[10];
	bool  opened;
	bool  valid;
	bool  is_ank;
	uint8_t w;
	uint8_t h;
	uint16_t fsz;
	uint8_t bc;
	FILE *file;
};

struct FontxFile;

void AddFontx(struct FontxFile *fx, const char *path);
void InitFontx(struct FontxFile *fxs, const char *f0, const char *f1);
bool OpenFontx(struct FontxFile *fx);
void CloseFontx(struct FontxFile *fx);
void DumpFontx(struct FontxFile *fxs);
uint8_t getFortWidth(struct FontxFile *fx);
uint8_t getFortHeight(struct FontxFile *fx);
bool GetFontx(struct FontxFile *fxs, uint8_t ascii , uint8_t *pGlyph, uint8_t *pw, uint8_t *ph);
void Font2Bitmap(uint8_t *fonts, uint8_t *line, uint8_t w, uint8_t h, uint8_t inverse);
void UnderlineBitmap(uint8_t *line, uint8_t w, uint8_t h);
void ReversBitmap(uint8_t *line, uint8_t w, uint8_t h);
void ShowFont(uint8_t *fonts, uint8_t pw, uint8_t ph);
void ShowBitmap(uint8_t *bitmap, uint8_t pw, uint8_t ph);
uint8_t RotateByte(uint8_t ch);

// UTF8 to SJIS table
// https://www.mgo-tec.com/blog-entry-utf8sjis01.html
//#define Utf8Sjis "Utf8Sjis.tbl"
//uint16_t UTF2SJIS(spiffs_file fd, uint8_t *utf8);
//int String2SJIS(spiffs_file fd, unsigned char *str_in, size_t stlen, uint16_t *sjis, size_t ssize);

#ifdef __cplusplus
}
#endif

#endif /* MAIN_FONTX_H_ */
