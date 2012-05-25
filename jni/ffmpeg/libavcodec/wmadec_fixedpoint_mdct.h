/*
 * WMA compatible decoder
 * copyright (c) 2002 The FFmpeg Project
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef AVCODEC_WMADEC_FIXEDPOINT_MDCT_H
#define AVCODEC_WMADEC_FIXEDPOINT_MDCT_H


//int fixed_ff_mdct_init(MDCTContextFixed2 *s, int nbits, int inverse);
//void fixed_ff_imdct_calc(MDCTContextFixed2 *s, int32_t *output, int32_t *input);
//int fixed_mdct_init_global(void);


static FFTComplexFixed exptab0[512];

static int32_t *tcosarray[5], *tsinarray[5];
static int32_t tcos0[1024], tcos1[512], tcos2[256], tcos3[128], tcos4[64];
static int32_t tsin0[1024], tsin1[512], tsin2[256], tsin3[128], tsin4[64];

static uint16_t revtab0[1024];

/* butter fly op */
#define BF(pre, pim, qre, qim, pre1, pim1, qre1, qim1) \
{ \
  int32_t ax, ay, bx, by; \
  bx=pre1; \
  by=pim1; \
  ax=qre1; \
  ay=qim1; \
  pre = (bx + ax); \
  pim = (by + ay); \
  qre = (bx - ax); \
  qim = (by - ay); \
}


static inline void CMUL(fixed32 *pre,
          fixed32 *pim,
          fixed32 are,
          fixed32 aim,
          fixed32 bre,
          fixed32 bim)
{
    //int64_t x,y;
    fixed32 _aref = are;
    fixed32 _aimf = aim;
    fixed32 _bref = bre;
    fixed32 _bimf = bim;
    fixed32 _r1 = fixmul32b(_bref, _aref);
    fixed32 _r2 = fixmul32b(_bimf, _aimf);
    fixed32 _r3 = fixmul32b(_bref, _aimf);
    fixed32 _r4 = fixmul32b(_bimf, _aref);
    *pre = _r1 - _r2;
    *pim = _r3 + _r4;

}

int fixed_fft_calc_unscaled(FFTContextFixed *s, FFTComplexFixed *z)
{
    int ln = s->nbits;
    int j, np, np2;
    int nblocks, nloops;
    register FFTComplexFixed *p, *q;
    int l;
    int32_t tmp_re, tmp_im;
    int tabshift = 10-ln;

    np = 1 << ln;

    /* pass 0 */
    p=&z[0];
    j=(np >> 1);
    do
    {
        BF(p[0].re, p[0].im, p[1].re, p[1].im,
           p[0].re, p[0].im, p[1].re, p[1].im);
        p+=2;
    }
    while (--j != 0);

    /* pass 1 */
    p=&z[0];
    j=np >> 2;
    if (s->inverse)
    {
        do
        {
            BF(p[0].re, p[0].im, p[2].re, p[2].im,
               p[0].re, p[0].im, p[2].re, p[2].im);
            BF(p[1].re, p[1].im, p[3].re, p[3].im,
               p[1].re, p[1].im, -p[3].im, p[3].re);
            p+=4;
        }
        while (--j != 0);
    }
    else
    {
        do
        {
            BF(p[0].re, p[0].im, p[2].re, p[2].im,
               p[0].re, p[0].im, p[2].re, p[2].im);
            BF(p[1].re, p[1].im, p[3].re, p[3].im,
               p[1].re, p[1].im, p[3].im, -p[3].re);
            p+=4;
        }
        while (--j != 0);
    }

    /* pass 2 .. ln-1 */
    nblocks = np >> 3;
    nloops = 1 << 2;
    np2 = np >> 1;
    do
    {
        p = z;
        q = z + nloops;
        for (j = 0; j < nblocks; ++j)
        {
            BF(p->re, p->im, q->re, q->im,
               p->re, p->im, q->re, q->im);

            p++;
            q++;
            for(l = nblocks; l < np2; l += nblocks)
            {
                CMUL(&tmp_re, &tmp_im, exptab0[(l<<tabshift)].re, exptab0[(l<<tabshift)].im, q->re, q->im);
                //CMUL(&tmp_re, &tmp_im, exptab[l].re, exptab[l].im, q->re, q->im);
                BF(p->re, p->im, q->re, q->im,
                   p->re, p->im, tmp_re, tmp_im);
                p++;
                q++;
            }

            p += nloops;
            q += nloops;
        }
        nblocks = nblocks >> 1;
        nloops = nloops << 1;
    }
    while (nblocks != 0);

    return 0;
}

int fixed_fft_init_global(void)
{
    int i, n;
    int32_t c1, s1, s2;

    n=1<<10;
    s2 = 1 ? 1 : -1;

    for(i=0;i<(n/2);++i)
    {
        int32_t ifix = itofix32(i);
        int32_t nfix = itofix32(n);
        int32_t res = fixdiv32(ifix,nfix);

        s1 = fsincos(res<<16, &c1);

        exptab0[i].re = c1;
        exptab0[i].im = s1*s2;
    }

    return 0;
}




/**
 * init MDCT or IMDCT computation.
 */
int fixed_ff_mdct_init(MDCTContextFixed *s, int nbits, int inverse)
{
    int n, n4, i;

    memset(s, 0, sizeof(*s));
    n = 1 << nbits;            /* nbits ranges from 12 to 8 inclusive */
    s->nbits = nbits;
    s->n = n;
    n4 = n >> 2;
    s->tcos = tcosarray[12-nbits];
    s->tsin = tsinarray[12-nbits];
    for(i=0;i<n4;i++)
    {
        int32_t ip = itofix32(i) + 0x2000;
        ip = ip >> nbits;

        /*I can't remember why this works, but it seems
          to agree for ~24 bits, maybe more!*/
        s->tsin[i] = - fsincos(ip<<16, &(s->tcos[i]));
        s->tcos[i] *=-1;
    }

    (&s->fft)->nbits = nbits-2;
    (&s->fft)->inverse = inverse;

    return 0;

}

/**
 * Compute inverse MDCT of size N = 2^nbits
 * @param output N samples
 * @param input N/2 samples
 * @param tmp N/2 samples
 */
void fixed_ff_imdct_calc(MDCTContextFixed *s,
                   int32_t *output,
                   int32_t *input)
{
	int k, n8, n4, n2, n, j,scale;
    const int32_t *tcos = s->tcos;
    const int32_t *tsin = s->tsin;
    const int32_t *in1, *in2;
    FFTComplexFixed *z1 = (FFTComplexFixed *)output;
    FFTComplexFixed *z2 = (FFTComplexFixed *)input;
    int revtabshift = 12 - s->nbits;

    n = 1 << s->nbits;

    n2 = n >> 1;
    n4 = n >> 2;
    n8 = n >> 3;

    /* pre rotation */
    in1 = input;
    in2 = input + n2 - 1;

    for(k = 0; k < n4; k++)
    {
        j=revtab0[k<<revtabshift];
        CMUL(&z1[j].re, &z1[j].im, *in2, *in1, tcos[k], tsin[k]);
        in1 += 2;
        in2 -= 2;
    }

    scale = fixed_fft_calc_unscaled(&s->fft, z1);

    /* post rotation + reordering */
    for(k = 0; k < n4; k++)
    {
        CMUL(&z2[k].re, &z2[k].im, (z1[k].re), (z1[k].im), tcos[k], tsin[k]);
    }

    for(k = 0; k < n8; k++)
    {
        int32_t r1,r2,r3,r4,r1n,r2n,r3n;

        r1 = z2[n8 + k].im;
        r1n = r1 * -1;
        r2 = z2[n8-1-k].re;
        r2n = r2 * -1;
        r3 = z2[k+n8].re;
        r3n = r3 * -1;
        r4 = z2[n8-k-1].im;

        output[2*k] = r1n;
        output[n2-1-2*k] = r1;

        output[2*k+1] = r2;
        output[n2-1-2*k-1] = r2n;

        output[n2 + 2*k]= r3n;
        output[n-1- 2*k]= r3n;

        output[n2 + 2*k+1]= r4;
        output[n-2 - 2 * k] = r4;
    }
}

/* init MDCT */

int fixed_mdct_init_global(void)
{
    int i,j,m;

    /* although seemingly degenerate, these cannot actually be merged together without
       a substantial increase in error which is unjustified by the tiny memory savings*/

    tcosarray[0] = tcos0; tcosarray[1] = tcos1; tcosarray[2] = tcos2; tcosarray[3] = tcos3;tcosarray[4] = tcos4;
    tsinarray[0] = tsin0; tsinarray[1] = tsin1; tsinarray[2] = tsin2; tsinarray[3] = tsin3;tsinarray[4] = tsin4;

    /* init the MDCT bit reverse table here rather then in fft_init */

    for(i=0;i<1024;i++)           /*hard coded to a 2048 bit rotation*/
    {                             /*smaller sizes can reuse the largest*/
        m=0;
        for(j=0;j<10;j++)
        {
            m |= ((i >> j) & 1) << (10-j-1);
        }

       revtab0[i]=m;
    }

    fixed_fft_init_global();

    return 0;
}



#endif
