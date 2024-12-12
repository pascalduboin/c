//
//  main.c
//  nic2dsk
//
//  Created by pascal on 12/12/2024.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_PATH 1024

static const unsigned char decTable[] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x02,0x03,0x00,0x04,0x05,0x06,
    0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x08,0x00,0x00,0x00,0x09,0x0a,0x0b,0x0c,0x0d,
    0x00,0x00,0x0e,0x0f,0x10,0x11,0x12,0x13,0x00,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1b,0x00,0x1c,0x1d,0x1e,
    0x00,0x00,0x00,0x1f,0x00,0x00,0x20,0x21,0x00,0x22,0x23,0x24,0x25,0x26,0x27,0x28,
    0x00,0x00,0x00,0x00,0x00,0x29,0x2a,0x2b,0x00,0x2c,0x2d,0x2e,0x2f,0x30,0x31,0x32,
    0x00,0x00,0x33,0x34,0x35,0x36,0x37,0x38,0x00,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f
};

static const unsigned char scramble[] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

static const unsigned char FlipBit[4] = { 0, 2, 1, 3 };

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

static int ends_with_nic(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (!dot) return 0;
    return (strcasecmp(dot, ".nic") == 0);
}

void conv_nic2dsk(const char *name)
{
    if (!name || !*name) {
        fprintf(stderr, "No file specified.\n");
        return;
    }

    if (!ends_with_nic(name)) {
        fprintf(stderr, "Use a .NIC file.\n");
        return;
    }

    FILE *fp = fopen(name, "rb");
    if (!fp) {
        perror("Can't open the NIC file");
        return;
    }

    char path[MAX_PATH];
    strncpy(path, name, MAX_PATH - 1);
    path[MAX_PATH - 1] = '\0';

    remove_extension(path);
    strncat(path, ".DSK", MAX_PATH - strlen(path) - 1);

    FILE *fpw = fopen(path, "wb");
    if (!fpw) {
        perror("Can't write DSK file");
        fclose(fp);
        return;
    }

    unsigned char src[512], dst[256 + 2]; // +2 par prudence (ici du code initial)
    // volume est défini mais non utilisé dans le code d'origine, on peut l'ignorer.
    // int volume = 0xfe; // non utilisé

    for (int track = 0; track < 35; track++) {
        const long track_offset = track * (256 * 16);
        for (int sector = 0; sector < 16; sector++) {
            long offset = track_offset + (scramble[sector] * 256);
            if (fseek(fpw, offset, SEEK_SET) != 0) {
                fprintf(stderr, "File seek error.\n");
                fclose(fp);
                fclose(fpw);
                return;
            }

            if (fread(src, 1, 512, fp) != 512) {
                fprintf(stderr, "File read error.\n");
                fclose(fp);
                fclose(fpw);
                return;
            }

            unsigned char ox = 0;
            int i, j;

            // Décodage des 0x38 octets (86 en décimal)
            // (0x8e - 0x38 = 0x56 = 86)
            for (j = 0, i = 0x38; i < 0x8e; i++, j++) {
                unsigned char x = ((ox ^ decTable[src[i]]) & 0x3f);
                dst[j + 172] = FlipBit[(x >> 4) & 3];
                dst[j + 86]  = FlipBit[(x >> 2) & 3];
                dst[j]       = FlipBit[x & 3];
                ox = x;
            }

            // Décodage du reste (0x18e - 0x8e = 0x100 = 256)
            for (j = 0, i = 0x8e; i < 0x18e; i++, j++) {
                unsigned char x = ((ox ^ decTable[src[i]]) & 0x3f);
                dst[j] |= (x << 2);
                ox = x;
            }

            if (fwrite(dst, 1, 256, fpw) != 256) {
                fprintf(stderr, "File write error.\n");
                fclose(fp);
                fclose(fpw);
                return;
            }
        }
    }

    fclose(fpw);
    fclose(fp);
    // fprintf(stderr, "The DSK file created.\n");
}

int main(int argc, char *argv[])
{
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            conv_nic2dsk(argv[i]);
        }
    } else {
        fprintf(stderr, "Usage: %s file1.nic [file2.nic ...]\n", argv[0]);
    }
    return 0;
}