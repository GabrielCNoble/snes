#include "ppu.h"
#include "addr_common.h"


union
{
    unsigned char ppu_regs[0x80];

    struct
    {
        unsigned char inidps;
        unsigned char objsel;
        unsigned char oamddl;
        unsigned char oamddh;
        unsigned char oamdata_write;
        unsigned char bgmode;
        unsigned char mosaic;
        unsigned char bg1sc;
        unsigned char bg2sc;
        unsigned char bg3sc;
        unsigned char bg4sc;
        unsigned char bg12nba;
        unsigned char bg34nba;
        unsigned char bg1h0fs;
        unsigned char bg1v0fs;
        unsigned char bg2h0fs;
        unsigned char bg2v0fs;
        unsigned char bg3h0fs;
        unsigned char bg3v0fs;
        unsigned char bg4h0fs;
        unsigned char bg4v0fs;
        unsigned char vmainc;
        unsigned char vmaddl;
        unsigned char vmaddh;
        unsigned char vmdatal_write;
        unsigned char vmdatah_write;
        unsigned char m7sel;
        unsigned char m7a;
        unsigned char m7b;
        unsigned char m7c;
        unsigned char m7d;
        unsigned char m7x;
        unsigned char m7y;
        unsigned char cgadd;
        unsigned char cgdata_write;
        unsigned char w12sel;
        unsigned char w34sel;
        unsigned char wobjsel;
        unsigned char wh0;
        unsigned char wh1;
        unsigned char wh2;
        unsigned char wh3;
        unsigned char wbglog;
        unsigned char wobjlog;
        unsigned char tm;
        unsigned char ts;
        unsigned char tmw;
        unsigned char tsw;
        unsigned char cgswsel;
        unsigned char cgadsub;
        unsigned char coldata;
        unsigned char setini;
        unsigned char mpyl;
        unsigned char mpym;
        unsigned char mpyh;
        unsigned char slhv;
        unsigned char oamdata_read;
        unsigned char vmdatal_read;
        unsigned char vmdatah_read;
        unsigned char cgdata_read;
        unsigned char ophct;
        unsigned char opvct;
        unsigned char stat77;
        unsigned char stat78;
        unsigned char apuio0;
        unsigned char apuio1;
        unsigned char apuio2;
        unsigned char apuio3;
        unsigned char unused[0x3c];
        unsigned char wmdata;
        unsigned char wmaddl;
        unsigned char wmaddm;
        unsigned char wmaddh;
    };
}ppu_regs;

__fastcall void *ppu_pointer(unsigned int effective_address)
{
    unsigned int ppu_address;
    ppu_address = (effective_address & 0xffff) - PPU_START_ADDRESS;
    return (void *)(ppu_regs.ppu_regs + ppu_address);
}

void step_ppu()
{

}

