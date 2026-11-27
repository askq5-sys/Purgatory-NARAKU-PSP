"""Tint Screen gray: exact pixel math, the black/white skip and cache handling."""
import os as _os, sys as _sys
_sys.path.insert(0, _os.path.dirname(_os.path.abspath(__file__)))
from source_util import main_source
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

HARNESS = r'''
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "runtime/tone_gray.h"
/* The loop body used before tone_gray_pixel was extracted. */
static uint32_t reference(uint32_t c, int gray)
{
    int r=c&255,g=(c>>8)&255,b=(c>>16)&255;
    int hi=r>g?r:g,lo=r<g?r:g,l;
    if (b > hi) hi = b;
    if (b < lo) lo = b;
    l = (hi + lo + 1) / 2;
    r=(r*(255-gray)+l*gray+127)/255;
    g=(g*(255-gray)+l*gray+127)/255;
    b=(b*(255-gray)+l*gray+127)/255;
    return (c&0xFF000000u)|r|(g<<8)|(b<<16);
}
/* RGB tone pass after gray: clamped add, then clamped subtract. */
static uint32_t tone(uint32_t c, int r, int g, int b)
{
    int ch[3]={c&255,(c>>8)&255,(c>>16)&255}, t[3]={r,g,b};
    for (int i=0;i<3;++i) {
        int v=ch[i]+(t[i]>0?t[i]:0); if (v>255) v=255;
        v-= t[i]<0?-t[i]:0; if (v<0) v=0; ch[i]=v;
    }
    return (c&0xFF000000u)|ch[0]|(ch[1]<<8)|(ch[2]<<16);
}
int main(void)
{
    static const int grays[]={1,2,40,127,128,200,254,255};
    for (unsigned k=0;k<sizeof grays/sizeof *grays;++k)
        for (uint32_t c=0;c<0x1000000u;++c) {
            uint32_t p=c|((c*2654435761u)&0xFF000000u);
            assert(tone_gray_pixel(p,grays[k])==reference(p,grays[k]));
        }
    static const int hidden[][3]={{-255,-255,-255},{255,255,255},{-300,-255,-256},{256,300,255}};
    for (unsigned k=0;k<4;++k) {
        assert(tone_hides_gray(hidden[k][0],hidden[k][1],hidden[k][2]));
        for (uint32_t c=0;c<0x1000000u;c+=97)
            for (int gray=1;gray<=255;gray+=37)
                assert(tone(tone_gray_pixel(c,gray),hidden[k][0],hidden[k][1],hidden[k][2])==
                       tone(c,hidden[k][0],hidden[k][1],hidden[k][2]));
    }
    assert(!tone_hides_gray(-30,-30,0));
    assert(!tone_hides_gray(-255,-255,0));
    assert(!tone_hides_gray(255,255,-255));
    puts("ok");
    return 0;
}
'''


def test():
    with tempfile.TemporaryDirectory() as tmp:
        c = Path(tmp) / 't.c'
        exe = Path(tmp) / 't'
        c.write_text(HARNESS)
        subprocess.run(['cc', '-std=c99', '-O0', '-I', str(ROOT), str(c), '-o', str(exe)], check=True)
        subprocess.run([str(exe)], check=True, stdout=subprocess.DEVNULL)
    source = main_source(ROOT)
    block = source[source.index('if (g_world_tone_gray > 0 &&'):]
    block = block[:block.index('sceGuTexFlush();')]
    assert '0x04000000u' in block and '0x44000000u' not in block
    first = block.index('sceKernelDcacheWritebackInvalidateRange(fb,bytes);')
    loop = block.index('tone_gray_pixel(')
    last = block.rindex('sceKernelDcacheWritebackInvalidateRange(fb,bytes);')
    assert block.index('sceGuSync(0,0);') < first < loop < last
    print('PASS: gray tone pixels match the original formula for all 16.7M colours at 8 levels; '
          'black/white tone skip leaves every pixel unchanged; cached VRAM access is flushed around the loop')


if __name__ == '__main__':
    test()
