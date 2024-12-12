//
//  main.c
//  dsk2nic
//
//  Created by pascal on 12/12/2024.
//

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#define MAX_PATH 1024

static void writeAAVal(unsigned char val, FILE *fp) {
    fputc((0xAA | (val >> 1)) & 0xFF, fp);
    fputc((0xAA | val) & 0xFF, fp);
}

static char *find_extension(char *path) {
    char *dot = strrchr(path, '.');
    return dot ? dot : path + strlen(path);
}

static void remove_extension(char *path) {
    char *dot = find_extension(path);
    if (dot && *dot == '.') {
        *dot = '\0';
    }
}

static int ends_with_dsk(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return 0;
    return (strcasecmp(dot, ".dsk") == 0);
}

static void conv_dsk2nic(const char *name) {
    static const unsigned char encTable[] = {
        0x96,0x97,0x9A,0x9B,0x9D,0x9E,0x9F,0xA6,
        0xA7,0xAB,0xAC,0xAD,0xAE,0xAF,0xB2,0xB3,
        0xB4,0xB5,0xB6,0xB7,0xB9,0xBA,0xBB,0xBC,
        0xBD,0xBE,0xBF,0xCB,0xCD,0xCE,0xCF,0xD3,
        0xD6,0xD7,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,
        0xDF,0xE5,0xE6,0xE7,0xE9,0xEA,0xEB,0xEC,
        0xED,0xEE,0xEF,0xF2,0xF3,0xF4,0xF5,0xF6,
        0xF7,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF
    };
    static const unsigned char scramble[] = {
        0,7,14,6,13,5,12,4,11,3,10,2,9,1,8,15
    };
    static const unsigned char FlipBit1[4] = {0,2,1,3};
    static const unsigned char FlipBit2[4] = {0,8,4,12};
    static const unsigned char FlipBit3[4] = {0,32,16,48};

    unsigned char src[258]; // 256 + 2
    int volume = 0xFE;

    if (!name || !*name) {
        fprintf(stderr, "No input file specified.\n");
        return;
    }

    if (!ends_with_dsk(name)) {
        fprintf(stderr, "Please use a .DSK file as input.\n");
        return;
    }

    FILE *fp = fopen(name, "rb");
    if (!fp) {
        fprintf(stderr, "Can't open the DSK file %s.\n", name);
        return;
    }

    char path[MAX_PATH];
    strncpy(path, name, MAX_PATH - 1);
    path[MAX_PATH - 1] = '\0';

    remove_extension(path);
    strncat(path, ".NIC", MAX_PATH - strlen(path) - 1);

    FILE *fpw = fopen(path, "wb");
    if (!fpw) {
        fprintf(stderr, "Can't write to %s.\n", path);
        fclose(fp);
        return;
    }

    // Boucle sur 35 pistes, 16 secteurs
    for (int track = 0; track < 35; track++) {
        for (int sector = 0; sector < 16; sector++) {
            long pos = track * (256 * 16) + scramble[sector] * 256;
            if (fseek(fp, pos, SEEK_SET) != 0) {
                fprintf(stderr, "Error seeking in DSK file.\n");
                fclose(fp);
                fclose(fpw);
                return;
            }

            // Ecriture du préambule
            for (int i = 0; i < 22; i++)
                fputc(0xFF, fpw);

            // Sync bytes
            const unsigned char syncBytes[] = {
                0x03,0xFC,0xFF,0x3F,0xCF,0xF3,0xFC,0xFF,0x3F,0xCF,0xF3,0xFC
            };
            fwrite(syncBytes, 1, sizeof(syncBytes), fpw);

            // Address field
            fputc(0xD5, fpw);
            fputc(0xAA, fpw);
            fputc(0x96, fpw);
            writeAAVal(volume, fpw);
            writeAAVal(track, fpw);
            writeAAVal(sector, fpw);
            writeAAVal(volume ^ track ^ sector, fpw);
            fputc(0xDE, fpw);
            fputc(0xAA, fpw);
            fputc(0xEB, fpw);

            // Gap
            for (int i = 0; i < 5; i++)
                fputc(0xFF, fpw);

            // Data field
            fputc(0xD5, fpw);
            fputc(0xAA, fpw);
            fputc(0xAD, fpw);

            size_t readBytes = fread(src, 1, 256, fp);
            if (readBytes != 256) {
                fprintf(stderr, "Error reading DSK file.\n");
                fclose(fp);
                fclose(fpw);
                return;
            }
            src[256] = 0;
            src[257] = 0;

            unsigned char ox = 0;
            unsigned char x;

            // Encode les 86 octets séparés en tranches (3 * 86 = 258)
            for (int i = 0; i < 86; i++) {
                x = FlipBit1[src[i]&3] | FlipBit2[src[i+86]&3] | FlipBit3[src[i+172]&3];
                fputc(encTable[(x^ox)&0x3F], fpw);
                ox = x;
            }

            // Encode les 256 octets (décalés de 2 bits)
            for (int i = 0; i < 256; i++) {
                x = (unsigned char)(src[i] >> 2);
                fputc(encTable[(x^ox)&0x3F], fpw);
                ox = x;
            }

            // Dernier octet
            fputc(encTable[ox&0x3F], fpw);

            fputc(0xDE, fpw);
            fputc(0xAA, fpw);
            fputc(0xEB, fpw);

            // Fin de secteur
            for (int i = 0; i < 14; i++)
                fputc(0xFF, fpw);

            // Padding jusqu'à 512 bytes
            for (int i = 0; i < (512 - 416); i++)
                fputc(0x00, fpw);
        }
    }

    fclose(fpw);
    fclose(fp);
    // fprintf(stderr, "NIC image created.\n");
}

int main(int argc, char *argv[]) {
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            conv_dsk2nic(argv[i]);
        }
    } else {
        fprintf(stderr, "Usage: %s <file1.dsk> [file2.dsk ...]\n", argv[0]);
    }
    return 0;
}
