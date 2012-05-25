/*
 * WMA compatible decoder
 * Copyright (c) 2002 The FFmpeg Project.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * @file wmadec.c
 * WMA compatible decoder.
 */

//#include <android/log.h>
//#define  LOG_TAG    "mpegaudiodec.c"
//#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
//#define  LOGW(...)  __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)

#include "avcodec.h"

#define PRECISION 16
#define PRECISION64 16

#include "wmadec_fixedpoint.h"
#include "wmadata_fixedpoint.h"

#ifndef CONFIG_FIXED_POINT
#error NOT CONFIG_FIXED_POINT
#endif

#ifndef FXF 
#define FXF(x) ((fixed32)((x) * (float)(1 << PRECISION) + ((x) < 0 ? -0.5 : 0.5)))
#endif

#include "wmadata_shared.h"

/*MDCT reconstruction windows*/
//DECLARE_ALIGNED(16, fixed32, stat0)[2048]; 
//DECLARE_ALIGNED(16, fixed32, stat1)[1024]; 
//DECLARE_ALIGNED(16, fixed32, stat2)[512]; 
//DECLARE_ALIGNED(16, fixed32, stat3)[256];
//DECLARE_ALIGNED(16, fixed32, stat4)[128];    


#define fixtof64(x)       (float)((float)(x) / (float)(1 << PRECISION64))        //does not work on int64_t!
#define ftofix32(x)       ((fixed32)((x) * (float)(1 << PRECISION) + ((x) < 0 ? -0.5 : 0.5)))
#define itofix64(x)       (IntTo64(x))
#define itofix32(x)       ((x) << PRECISION)
#define fixtoi32(x)       ((x) >> PRECISION)
#define fixtoi64(x)       (IntFrom64(x))

static inline fixed64 IntTo64(int x){
    fixed64 res = 0;
    unsigned char *p = (unsigned char *)&res;

#if HAVE_BIGENDIAN
    p[5] = x & 0xff;
    p[4] = (x & 0xff00)>>8;
    p[3] = (x & 0xff0000)>>16;
    p[2] = (x & 0xff000000)>>24;
#else
    p[2] = x & 0xff;
    p[3] = (x & 0xff00)>>8;
    p[4] = (x & 0xff0000)>>16;
    p[5] = (x & 0xff000000)>>24;
#endif
    return res;
}

static inline int IntFrom64(fixed64 x)
{
    int res = 0;
    unsigned char *p = (unsigned char *)&x;

#if HAVE_BIGENDIAN
    res = p[5] | (p[4]<<8) | (p[3]<<16) | (p[2]<<24);
#else
    res = p[2] | (p[3]<<8) | (p[4]<<16) | (p[5]<<24);
#endif
    return res;
}

static inline fixed32 Fixed32From64(fixed64 x)
{
  return x & 0xFFFFFFFF;
}

static inline fixed64 Fixed32To64(fixed32 x)
{
  return (fixed64)x;
}

static inline fixed32 fixmul32(fixed32 x, fixed32 y)
{
    fixed64 temp;
    temp = x;
    temp *= y;

    temp >>= PRECISION;

    return (fixed32)temp;
}

static inline fixed32 fixmul32b(fixed32 x, fixed32 y)
{
    fixed64 temp;

    temp = x;
    temp *= y;

    temp >>= 31;        //16+31-16 = 31 bits

    return (fixed32)temp;
}

static inline void vector_fmul_add_add(fixed32 *dst, const fixed32 *src0, const fixed32 *src1, int len){
    int i;
    for(i=0; i<len; i++)
        dst[i] = fixmul32b(src0[i], src1[i]) + dst[i];
}

static inline void vector_fmul_reverse(fixed32 *dst, const fixed32 *src0, const fixed32 *src1, int len){
    int i;
    src1 += len-1;
    for(i=0; i<len; i++)
        dst[i] = fixmul32b(src0[i], src1[-i]);
}

static fixed32 fixdiv32(fixed32 x, fixed32 y)
{
    fixed64 temp;

    if(x == 0)
        return 0;
    if(y == 0)
        return 0x7fffffff;
    temp = x;
    temp <<= PRECISION;
    return (fixed32)(temp / y);
}

static fixed64 fixdiv64(fixed64 x, fixed64 y)
{
    fixed64 temp;

    if(x == 0)
        return 0;
    if(y == 0)
        return 0x07ffffffffffffffLL;
    temp = x;
    temp <<= PRECISION64;
    return (fixed64)(temp / y);
}


static fixed32 fixsqrt32(fixed32 x)
{
    unsigned long r = 0, s, v = (unsigned long)x;

#define STEP(k) s = r + (1 << k * 2); r >>= 1; \
    if (s <= v) { v -= s; r |= (1 << k * 2); }

    STEP(15);
    STEP(14);
    STEP(13);
    STEP(12);
    STEP(11);
    STEP(10);
    STEP(9);
    STEP(8);
    STEP(7);
    STEP(6);
    STEP(5);
    STEP(4);
    STEP(3);
    STEP(2);
    STEP(1);
    STEP(0);

    return (fixed32)(r << (PRECISION / 2));
}

static const long cordic_circular_gain = 0xb2458939; /* 0.607252929 */  
     
/* Table of values of atan(2^-i) in 0.32 format fractions of pi where pi = 0xffffffff / 2 */    
static const unsigned long atan_table[] = {     
     0x1fffffff, /* +0.785398163 (or pi/4) */    
     0x12e4051d, /* +0.463647609 */  
     0x09fb385b, /* +0.244978663 */  
     0x051111d4, /* +0.124354995 */  
     0x028b0d43, /* +0.062418810 */  
     0x0145d7e1, /* +0.031239833 */  
     0x00a2f61e, /* +0.015623729 */  
     0x00517c55, /* +0.007812341 */  
     0x0028be53, /* +0.003906230 */  
     0x00145f2e, /* +0.001953123 */  
     0x000a2f98, /* +0.000976562 */  
     0x000517cc, /* +0.000488281 */  
     0x00028be6, /* +0.000244141 */  
     0x000145f3, /* +0.000122070 */  
     0x0000a2f9, /* +0.000061035 */  
     0x0000517c, /* +0.000030518 */  
     0x000028be, /* +0.000015259 */  
     0x0000145f, /* +0.000007629 */  
     0x00000a2f, /* +0.000003815 */  
     0x00000517, /* +0.000001907 */  
     0x0000028b, /* +0.000000954 */  
     0x00000145, /* +0.000000477 */  
     0x000000a2, /* +0.000000238 */  
     0x00000051, /* +0.000000119 */  
     0x00000028, /* +0.000000060 */  
     0x00000014, /* +0.000000030 */  
     0x0000000a, /* +0.000000015 */  
     0x00000005, /* +0.000000007 */  
     0x00000002, /* +0.000000004 */  
     0x00000001, /* +0.000000002 */  
     0x00000000, /* +0.000000001 */  
     0x00000000, /* +0.000000000 */  
};  
     
/**     
* Implements sin and cos using CORDIC rotation.    
*  
* @param phase has range from 0 to 0xffffffff, representing 0 and  
*        2*pi respectively.    
* @param cos return address for cos    
* @return sin of phase, value is a signed value from LONG_MIN to LONG_MAX,     
*         representing -1 and 1 respectively.  
*  
*        Gives at least 24 bits precision (last 2-8 bits or so are probably off)   
*/     
static long fsincos(unsigned long phase, fixed32 *cos)     
{   
     int32_t x, x1, y, y1;   
     unsigned long z, z1;    
     int i;  

     /* Setup initial vector */  
     x = cordic_circular_gain;   
     y = 0;  
     z = phase;  
     
     /* The phase has to be somewhere between 0..pi for this to work right */    
     if (z < 0xffffffff / 4) {   
         /* z in first quadrant, z += pi/2 to correct */     
         x = -x;     
         z += 0xffffffff / 4;    
     } else if (z < 3 * (0xffffffff / 4)) {  
         /* z in third quadrant, z -= pi/2 to correct */     
         z -= 0xffffffff / 4;    
     } else {    
         /* z in fourth quadrant, z -= 3pi/2 to correct */   
         x = -x;     
         z -= 3 * (0xffffffff / 4);  
     }   
     
     /* Each iteration adds roughly 1-bit of extra precision */  
     for (i = 0; i < 31; i++) {  
         x1 = x >> i;    
         y1 = y >> i;    
         z1 = atan_table[i];     
     
         /* Decided which direction to rotate vector. Pivot point is pi/2 */     
         if (z >= 0xffffffff / 4) {  
             x -= y1;    
             y += x1;    
             z -= z1;    
         } else {    
             x += y1;    
             y -= x1;    
             z += z1;    
         }   
     }   
     
     if (cos)    
         *cos = x;   
     
     return y;   
 }


#include "wmadec_fixedpoint_mdct.h"


/**
  * Apply MDCT window and add into output.
  *
  * We ensure that when the windows overlap their squared sum
  * is always 1 (MDCT reconstruction rule).
  *
  * The Vorbis I spec has a great diagram explaining this process.
  * See section 1.3.2.3 of http://xiph.org/vorbis/doc/Vorbis_I_spec.html
  */
static void fixed_wma_window(WMADecodeContext *s, fixed32 *in, fixed32 *out)
{
     //float *in = s->output;
     int block_len, bsize, n;

     /* left part */
     
     /* previous block was larger, so we'll use the size of the current 
      * block to set the window size*/
     if (s->block_len_bits <= s->prev_block_len_bits) {
         block_len = s->block_len;
         bsize = s->frame_len_bits - s->block_len_bits;

         vector_fmul_add_add(out, in, s->windows[bsize], block_len);

     } else {
         /*previous block was smaller or the same size, so use it's size to set the window length*/
         block_len = 1 << s->prev_block_len_bits;
         /*find the middle of the two overlapped blocks, this will be the first overlapped sample*/
         n = (s->block_len - block_len) / 2;
         bsize = s->frame_len_bits - s->prev_block_len_bits;

         vector_fmul_add_add(out+n, in+n, s->windows[bsize],  block_len);

         memcpy(out+n+block_len, in+n+block_len, n*sizeof(fixed32));
     }
    /* Advance to the end of the current block and prepare to window it for the next block.
     * Since the window function needs to be reversed, we do it backwards starting with the
     * last sample and moving towards the first
     */
     out += s->block_len;
     in += s->block_len;

     /* right part */
     if (s->block_len_bits <= s->next_block_len_bits) {
         block_len = s->block_len;
         bsize = s->frame_len_bits - s->block_len_bits;

         vector_fmul_reverse(out, in, s->windows[bsize], block_len);

     } else {
         block_len = 1 << s->next_block_len_bits;
         n = (s->block_len - block_len) / 2;
         bsize = s->frame_len_bits - s->next_block_len_bits;

         memcpy(out, in, n*sizeof(fixed32));

         vector_fmul_reverse(out+n, in+n, s->windows[bsize], block_len);

         memset(out+n+block_len, 0, n*sizeof(fixed32));
     }
 }




/* XXX: use same run/length optimization as mpeg decoders */
static void fixed_wma_init_coef_vlc(VLC *vlc, uint16_t **prun_table, 
							uint16_t **plevel_table, // no int_table!
                          const CoefVLCTable *vlc_table)
{
    int n = vlc_table->n;
    const uint8_t  *table_bits   = vlc_table->huffbits;
    const uint32_t *table_codes  = vlc_table->huffcodes;
    const uint16_t *levels_table = vlc_table->levels;
    uint16_t *run_table, *level_table;//, *int_table;
    uint16_t *flevel_table;
    int i, l, j, k, level;

    init_vlc(vlc, VLCBITS, n, table_bits, 1, 1, table_codes, 4, 4, 0);

    run_table   = av_malloc(n * sizeof(uint16_t));
    level_table = av_malloc(n * sizeof(uint16_t));
    flevel_table= av_malloc(n * sizeof(*flevel_table));
    //int_table   = av_malloc(n * sizeof(uint16_t));
    i = 2;
    level = 1;
    k = 0;
    while (i < n) {
        //int_table[k] = i;
        l = levels_table[k++];
        for (j = 0; j < l; j++) {
            run_table[i]   = j;
            level_table[i] = level;
            flevel_table[i]= level;
            i++;
        }
        level++;
    }
    *prun_table   = run_table;
    *plevel_table = flevel_table;
    //*pint_table   = int_table;
    av_free(level_table);
}

int fixed_ff_wma_init(AVCodecContext *avctx, int flags2)
{
    WMADecodeContext *s = avctx->priv_data;
    int i;
    //float bps1, high_freq;
    fixed64 bps1;
    fixed32 high_freq;
    //volatile float bps;
    fixed64 bps;
    int sample_rate1;
    int coef_vlc_table;

    if (   avctx->sample_rate <= 0 || avctx->sample_rate > 50000
        || avctx->channels    <= 0 || avctx->channels    > 8
        || avctx->bit_rate    <= 0)
        return -1;

    s->sample_rate = avctx->sample_rate;
    s->nb_channels = avctx->channels;
    s->bit_rate    = avctx->bit_rate;
    s->block_align = avctx->block_align;

    //dsputil_init(&s->dsp, avctx);

    if (avctx->codec->id == CODEC_ID_WMAV1) {
        s->version = 1;
    } else {
        s->version = 2;
    }

    /* compute MDCT block size */
    s->frame_len_bits = ff_wma_get_frame_len_bits(s->sample_rate, s->version, 0);

    s->frame_len = 1 << s->frame_len_bits;
    if (s->use_variable_block_len) {
        int nb_max, nb;
        nb = ((flags2 >> 3) & 3) + 1;
        if ((s->bit_rate / s->nb_channels) >= 32000)
            nb += 2;
        nb_max = s->frame_len_bits - BLOCK_MIN_BITS;
        if (nb > nb_max)
            nb = nb_max;
        s->nb_block_sizes = nb + 1;
    } else {
        s->nb_block_sizes = 1;
    }

    /* init rate dependent parameters */
    s->use_noise_coding = 1;
    //high_freq = s->sample_rate * 0.5;
    high_freq = itofix64(s->sample_rate) >> 1;

    /* if version 2, then the rates are normalized */
    sample_rate1 = s->sample_rate;
    if (s->version == 2) {
        if (sample_rate1 >= 44100) {
            sample_rate1 = 44100;
        } else if (sample_rate1 >= 22050) {
            sample_rate1 = 22050;
        } else if (sample_rate1 >= 16000) {
            sample_rate1 = 16000;
        } else if (sample_rate1 >= 11025) {
            sample_rate1 = 11025;
        } else if (sample_rate1 >= 8000) {
            sample_rate1 = 8000;
        }
    }

    //bps = (float)s->bit_rate / (float)(s->nb_channels * s->sample_rate);
    //s->byte_offset_bits = av_log2((int)(bps * s->frame_len / 8.0 + 0.5)) + 2;

    fixed64 tmp = itofix64(s->bit_rate);
    fixed64 tmp2 = itofix64(s->nb_channels * s->sample_rate);
    bps = fixdiv64(tmp, tmp2);
    fixed64 tim = bps * s->frame_len;
    fixed64 tmpi = fixdiv64(tim,itofix64(8));
    s->byte_offset_bits = av_log2(fixtoi64(tmpi+0x8000)) + 2;
    

    /* compute high frequency value and choose if noise coding should
       be activated */
    bps1 = bps;
    
    
    if (s->nb_channels == 2) {
    	bps1 = fixmul32(bps, 0x1999a);
        //bps1 = bps * 1.6;
    }
    
    if (sample_rate1 == 44100) {
        //if (bps1 >= 0.61) {
    	if (bps1 >= 0x9c29) {
            s->use_noise_coding = 0;
        } else {
            //high_freq = high_freq * 0.4;
        	high_freq = fixmul32(high_freq,0x6666);
        }
    } else if (sample_rate1 == 22050) {
        //if (bps1 >= 1.16) {
    	if (bps1 >= 0x128f6) {
            s->use_noise_coding = 0;
        //} else if (bps1 >= 0.72) {
    	} else if (bps1 >= 0xb852) {
            //high_freq = high_freq * 0.7;
    		high_freq = fixmul32(high_freq,0xb333);
        } else {
            //high_freq = high_freq * 0.6;
        	high_freq = fixmul32(high_freq,0x999a);
        }
    } else if (sample_rate1 == 16000) {
        //if (bps > 0.5) {
    	if (bps > 0x8000) {
            //high_freq = high_freq * 0.5;
    		high_freq = fixmul32(high_freq,0x8000);
        } else {
            //high_freq = high_freq * 0.3;
        	high_freq = fixmul32(high_freq,0x4ccd);
        }
    } else if (sample_rate1 == 11025) {
        //high_freq = high_freq * 0.7;
    	high_freq = fixmul32(high_freq,0xb333);
    } else if (sample_rate1 == 8000) {
        //if (bps <= 0.625) {
    	if (bps <= 0xa000) {
            //high_freq = high_freq * 0.5;
    		high_freq = fixmul32(high_freq,0x8000);
        //} else if (bps > 0.75) {
    	}else if (bps > 0xc000) {
            s->use_noise_coding = 0;
        } else {
            //high_freq = high_freq * 0.65;
        	high_freq = fixmul32(high_freq,0xa666);
        }
    } else {
        //if (bps >= 0.8) {
    	if (bps >= 0xcccd) {
            //high_freq = high_freq * 0.75;
    		high_freq = fixmul32(high_freq,0xc000);
        //} else if (bps >= 0.6) {
    	} else if (bps >= 0x999a) {
            //high_freq = high_freq * 0.6;
    		high_freq = fixmul32(high_freq,0x999a);
        } else {
            //high_freq = high_freq * 0.5;
        	high_freq = fixmul32(high_freq,0x8000);
        }
    }
//    dprintf(s->avctx, "flags2=0x%x\n", flags2);
//    dprintf(s->avctx, "version=%d channels=%d sample_rate=%d bitrate=%d block_align=%d\n",
//            s->version, s->nb_channels, s->sample_rate, s->bit_rate,
//            s->block_align);
//    dprintf(s->avctx, "bps=%f bps1=%f high_freq=%f bitoffset=%d\n",
//            bps, bps1, high_freq, s->byte_offset_bits);
//    dprintf(s->avctx, "use_noise_coding=%d use_exp_vlc=%d nb_block_sizes=%d\n",
//            s->use_noise_coding, s->use_exp_vlc, s->nb_block_sizes);

    /* compute the scale factor band sizes for each MDCT block size */
    {
        int a, b, pos, lpos, k, block_len, i, j, n;
        const uint8_t *table;

        if (s->version == 1) {
            s->coefs_start = 3;
        } else {
            s->coefs_start = 0;
        }
        for (k = 0; k < s->nb_block_sizes; k++) {
            block_len = s->frame_len >> k;

            if (s->version == 1) {
                lpos = 0;
                for (i = 0; i < 25; i++) {
                    a = ff_wma_critical_freqs[i];
                    b = s->sample_rate;
                    pos = ((block_len * 2 * a) + (b >> 1)) / b;
                    if (pos > block_len)
                        pos = block_len;
                    s->exponent_bands[0][i] = pos - lpos;
                    if (pos >= block_len) {
                        i++;
                        break;
                    }
                    lpos = pos;
                }
                s->exponent_sizes[0] = i;
            } else {
                /* hardcoded tables */
                table = NULL;
                a = s->frame_len_bits - BLOCK_MIN_BITS - k;
                if (a < 3) {
                    if (s->sample_rate >= 44100) {
                        table = exponent_band_44100[a];
                    } else if (s->sample_rate >= 32000) {
                        table = exponent_band_32000[a];
                    } else if (s->sample_rate >= 22050) {
                        table = exponent_band_22050[a];
                    }
                }
                if (table) {
                    n = *table++;
                    for (i = 0; i < n; i++)
                        s->exponent_bands[k][i] = table[i];
                    s->exponent_sizes[k] = n;
                } else {
                    j = 0;
                    lpos = 0;
                    for (i = 0; i < 25; i++) {
                        a = ff_wma_critical_freqs[i];
                        b = s->sample_rate;
                        pos = ((block_len * 2 * a) + (b << 1)) / (4 * b);
                        pos <<= 2;
                        if (pos > block_len)
                            pos = block_len;
                        if (pos > lpos)
                            s->exponent_bands[k][j++] = pos - lpos;
                        if (pos >= block_len)
                            break;
                        lpos = pos;
                    }
                    s->exponent_sizes[k] = j;
                }
            }

            /* max number of coefs */
            s->coefs_end[k] = (s->frame_len - ((s->frame_len * 9) / 100)) >> k;
            /* high freq computation */
//            s->high_band_start[k] = (int)((block_len * 2 * high_freq) /
                                          //s->sample_rate + 0.5);
            fixed32 tmp1 = high_freq*2;            /* high_freq is a fixed32!*/
            fixed32 tmp2=itofix32(s->sample_rate>>1);
            s->high_band_start[k] = fixtoi32( fixdiv32(tmp1, tmp2) * (block_len>>1) +0x8000);
            
            n = s->exponent_sizes[k];
            j = 0;
            pos = 0;
            for (i = 0; i < n; i++) {
                int start, end;
                start = pos;
                pos += s->exponent_bands[k][i];
                end = pos;
                if (start < s->high_band_start[k])
                    start = s->high_band_start[k];
                if (end > s->coefs_end[k])
                    end = s->coefs_end[k];
                if (end > start)
                    s->exponent_high_bands[k][j++] = end - start;
            }
            s->exponent_high_sizes[k] = j;
        }
    }

    /* init MDCT windows : simple sinus window */
//    for (i = 0; i < s->nb_block_sizes; i++) {
//        ff_init_ff_sine_windows(s->frame_len_bits - i);
//        s->windows[i] = ff_sine_windows[s->frame_len_bits - i];
//    }
    // For fixed point, it's combined with the mdct initialization in fixed_wma_decode_init
    

    s->reset_block_lengths = 1;

    if (s->use_noise_coding) {

        /* init the noise generator */
        if (s->use_exp_vlc) {
            //s->noise_mult = 0.02;
        	s->noise_mult = 0x51f;
        	s->noise_table = noisetable_exp;
        } else {
            //s->noise_mult = 0.04;
        	s->noise_mult = 0xa3d;

        	// ??? changes noisetable_exp, what about next run?
        	for (i=0;i<NOISE_TAB_SIZE;++i)
                noisetable_exp[i] = noisetable_exp[i]<< 1;
            s->noise_table = noisetable_exp;
        }

//        {
//            unsigned int seed;
//            float norm;
//            seed = 1;
//            norm = (1.0 / (float)(1LL << 31)) * sqrt(3) * s->noise_mult;
//            for (i = 0; i < NOISE_TAB_SIZE; i++) {
//                seed = seed * 314159 + 1;
//                s->noise_table[i] = (float)((int)seed) * norm;
//            }
//        }
    }

    /* choose the VLC tables for the coefficients */
    coef_vlc_table = 2;
    if (s->sample_rate >= 32000) {
        //if (bps1 < 0.72) {
    	if (bps1 < 0xb852) {
            coef_vlc_table = 0;
        //} else if (bps1 < 1.16) {
    	} else if (bps1 < 0x128f6) {
            coef_vlc_table = 1;
        }
    }
    s->coef_vlcs[0]= &coef_vlcs[coef_vlc_table * 2    ];
    s->coef_vlcs[1]= &coef_vlcs[coef_vlc_table * 2 + 1];
    fixed_wma_init_coef_vlc(&s->coef_vlc[0], &s->run_table[0], &s->level_table[0], //&s->int_table[0],
                  s->coef_vlcs[0]);
    fixed_wma_init_coef_vlc(&s->coef_vlc[1], &s->run_table[1], &s->level_table[1], //&s->int_table[1],
                  s->coef_vlcs[1]);

    return 0;
}

/* compute x^-0.25 with an exponent and mantissa table. We use linear
   interpolation to reduce the mantissa table size at a small speed
   expense (linear interpolation approximately doubles the number of
   bits of precision). */
static inline fixed32 fixed_pow_m1_4(WMADecodeContext *s, fixed32 x)
{
    union {
        float f;
        unsigned int v;
    } u, t;
    unsigned int e, m;
    fixed32 a, b;

    u.f = fixtof64(x);
    e = u.v >> 23;
    m = (u.v >> (23 - LSP_POW_BITS)) & ((1 << LSP_POW_BITS) - 1);
    /* build interpolation scale: 1 <= t < 2. */
    t.v = ((u.v << LSP_POW_BITS) & ((1 << 23) - 1)) | (127 << 23);
    a = ((fixed32*)s->lsp_pow_m_table1)[m];
    b = ((fixed32*)s->lsp_pow_m_table2)[m];

    /* lsp_pow_e_table contains 32.32 format */
    /* TODO:  Since we're unlikely have value that cover the whole
     * IEEE754 range, we probably don't need to have all possible exponents */

    return (lsp_pow_e_table[e] * (a + fixmul32(b, ftofix32(t.f))) >>32);
}

static void fixed_wma_lsp_to_curve_init(WMADecodeContext *s, int frame_len)
{
    fixed32 wdel, a, b, temp2;
    int i, m;

    wdel = fixdiv32(itofix32(1), itofix32(frame_len));
    for (i=0; i<frame_len; ++i) {
        /* TODO: can probably reuse the trig_init values here */
        fsincos((wdel*i)<<15, &temp2);
        /* get 3 bits headroom + 1 bit from not doubleing the values */
        s->lsp_cos_table[i] = temp2>>3;

    }
    /* NOTE: these two tables are needed to avoid two operations in
       pow_m1_4 */
    b = itofix32(1);
    int ix = 0;

    //s->lsp_pow_m_table1 = &vlcbuf3[0];
    //s->lsp_pow_m_table2 = &vlcbuf3[VLCBUF3SIZE];

    /*double check this later*/
    for(i=(1 << LSP_POW_BITS) - 1;i>=0;i--)
    {
        m = (1 << LSP_POW_BITS) + i;
        a = pow_a_table[ix++]<<4;
        ((fixed32*)s->lsp_pow_m_table1)[i] = 2 * a - b;
        ((fixed32*)s->lsp_pow_m_table2)[i] = b - a;
        b = a;
    }

}

static int fixed_wma_decode_init(AVCodecContext * avctx)
{
    //LOGW("fixed_wma_decode_init");
    
	WMADecodeContext *s = avctx->priv_data;
    int i, flags2;
    uint8_t *extradata;
    static int init = 0;
    
    s->avctx = avctx;
    
    /* extract flag infos */
    flags2 = 0;
    extradata = avctx->extradata;
    if (avctx->codec->id == CODEC_ID_WMAV1 && avctx->extradata_size >= 4) {
        flags2 = AV_RL16(extradata+2);
    } else if (avctx->codec->id == CODEC_ID_WMAV2 && avctx->extradata_size >= 6) {
        flags2 = AV_RL16(extradata+4);
    }

    s->use_exp_vlc = flags2 & 0x0001;
    s->use_bit_reservoir = flags2 & 0x0002;
    s->use_variable_block_len = flags2 & 0x0004;

    if(fixed_ff_wma_init(avctx, flags2)<0)
        return -1;
    
    if (!init) {
    	fixed_mdct_init_global();
    	init = 1;
    }
    
    for(i = 0; i < s->nb_block_sizes; ++i) {
        fixed_ff_mdct_init(&s->mdct_ctx[i], s->frame_len_bits - i + 1, 1);
    }

    /* ffmpeg uses malloc to only allocate as many window sizes as needed.  
    *  However, we're really only interested in the worst case memory usage.
    *  In the worst case you can have 5 window sizes, 128 doubling up 2048
    *  Smaller windows are handled differently.
    *  Since we don't have malloc, just statically allocate this
    */
    fixed32 *window;
    fixed32 *temp[5];
    temp[0] = s->stat0;
    temp[1] = s->stat1;
    temp[2] = s->stat2;
    temp[3] = s->stat3;
    temp[4] = s->stat4;
    
    /* init MDCT */
    for(i = 0; i < s->nb_block_sizes; i++) {
        int n, j;
        fixed32 alpha;
        n = 1 << (s->frame_len_bits - i);
        window = temp[i];
         
         /* this calculates 0.5/(2*n) */
        alpha = (1<<15)>>(s->frame_len_bits - i+1);  
        for(j=0;j<n;++j) {
            fixed32 j2 = itofix32(j) + 0x8000;
            /*alpha between 0 and pi/2*/
            window[j] = fsincos(fixmul32(j2,alpha)<<16, 0); 
        }
        s->windows[i] = window;
    }
    
    if (s->use_noise_coding) {
        init_vlc(&s->hgain_vlc, HGAINVLCBITS, sizeof(ff_wma_hgain_huffbits),
                 ff_wma_hgain_huffbits, 1, 1,
                 ff_wma_hgain_huffcodes, 2, 2, 0);
    }

    if (s->use_exp_vlc) {
        init_vlc(&s->exp_vlc, EXPVLCBITS, sizeof(ff_aac_scalefactor_bits), //FIXME move out of context
                 ff_aac_scalefactor_bits, 1, 1,
                 ff_aac_scalefactor_code, 4, 4, 0);
    } else {
        fixed_wma_lsp_to_curve_init(s, s->frame_len);
    }

    avctx->sample_fmt = SAMPLE_FMT_S16;
    
    return 0;
fail:
	return -1;
}




/* NOTE: We use the same code as Vorbis here */
/* XXX: optimize it further with SSE/3Dnow */
static void fixed_wma_lsp_to_curve(WMADecodeContext *s,
                             fixed32 *out,
                             fixed32 *val_max_ptr,
                             int n,
                             fixed32 *lsp)
{
    int i, j;
    fixed32 p, q, w, v, val_max, temp2;

    val_max = 0;
    for(i=0;i<n;++i) {
        /* shift by 2 now to reduce rounding error,
         * we can renormalize right before pow_m1_4
         */

        p = 0x8000<<5;
        q = 0x8000<<5;
        w = s->lsp_cos_table[i];

        for (j=1;j<NB_LSP_COEFS;j+=2) {
            /* w is 5.27 format, lsp is in 16.16, temp2 becomes 5.27 format */
            temp2 = ((w - (lsp[j - 1]<<11)));

            /* q is 16.16 format, temp2 is 5.27, q becomes 16.16 */
            q = fixmul32b(q, temp2 )<<4;
            p = fixmul32b(p, (w - (lsp[j]<<11)))<<4;
        }

        /* 2 in 5.27 format is 0x10000000 */
        p = fixmul32(p, fixmul32b(p, (0x10000000 - w)))<<3;
        q = fixmul32(q, fixmul32b(q, (0x10000000 + w)))<<3;

        v = (p + q) >>9;  /* p/q end up as 16.16 */
        v = fixed_pow_m1_4(s, v);
        if (v > val_max)
            val_max = v;
        out[i] = v;
    }

    *val_max_ptr = val_max;
}

/* decode exponents coded with LSP coefficients (same idea as Vorbis)
 * only used for low bitrate (< 16kbps) files
 */
static void fixed_decode_exp_lsp(WMADecodeContext *s, int ch)
{
    fixed32 lsp_coefs[NB_LSP_COEFS];
    int val, i;

    for (i = 0; i < NB_LSP_COEFS; ++i) {
        if (i == 0 || i >= 8)
            val = get_bits(&s->gb, 3);
        else
            val = get_bits(&s->gb, 4);
        lsp_coefs[i] = ff_wma_lsp_codebook[i][val];
    }

    fixed_wma_lsp_to_curve(s,
                     s->exponents[ch],
                     &s->max_exponent[ch],
                     s->block_len,
                     lsp_coefs);
}


static int fixed_decode_exp_vlc(WMADecodeContext *s, int ch)
{
    int last_exp, n, code;
    const uint16_t *ptr;
    fixed32 v, max_scale;
    fixed32 *q, *q_end, iv;
    
    const fixed32 *ptab = ff_wma_pow_tab + 60; 
    const fixed32 *iptab = ptab;

    ptr = s->exponent_bands[s->frame_len_bits - s->block_len_bits];
    q = s->exponents[ch];
    q_end = q + s->block_len;
    max_scale = 0;
    if (s->version == 1) {
        last_exp = get_bits(&s->gb, 5) + 10;
        v = ptab[last_exp];
        iv = iptab[last_exp];
        max_scale = v;
        n = *ptr++;
        switch (n & 3) do {
        case 0: *q++ = iv;
        case 3: *q++ = iv;
        case 2: *q++ = iv;
        case 1: *q++ = iv;
        } while ((n -= 4) > 0);
    }else
        last_exp = 36;

    while (q < q_end) {
        code = get_vlc2(&s->gb, s->exp_vlc.table, EXPVLCBITS, EXPMAX);
        if (code < 0){
            av_log(s->avctx, AV_LOG_ERROR, "Exponent vlc invalid\n");
            return -1;
        }
        /* NOTE: this offset is the same as MPEG4 AAC ! */
        last_exp += code - 60;
        if ((unsigned)last_exp + 60 > FF_ARRAY_ELEMS(pow_tab)) {
            av_log(s->avctx, AV_LOG_ERROR, "Exponent out of range: %d\n",
                   last_exp);
            return -1;
        }
        v = ptab[last_exp];
        iv = iptab[last_exp];
        if (v > max_scale)
            max_scale = v;
        n = *ptr++;
        switch (n & 3) do {
        case 0: *q++ = iv;
        case 3: *q++ = iv;
        case 2: *q++ = iv;
        case 1: *q++ = iv;
        } while ((n -= 4) > 0);
    }
    s->max_exponent[ch] = max_scale;
    return 0;
}

static int fixed_ff_wma_run_level_decode(AVCodecContext* avctx, GetBitContext* gb,
                            VLC *vlc,
                            const uint16_t *level_table, const uint16_t *run_table,
                            int version, int16_t *ptr, int offset,
                            int num_coefs, int block_len, int frame_len_bits,
                            int coef_nb_bits)
{
    //LOGW("fixed_ff_wma_run_level_decode");
	int code, level, sign;
    //const uint32_t *ilvl = (const uint32_t*)level_table;
    //uint32_t *iptr = (uint32_t*)ptr;
    const unsigned int coef_mask = block_len - 1;
    for (; offset < num_coefs; offset++) {
        code = get_vlc2(gb, vlc->table, VLCBITS, VLCMAX);
        if (code > 1) {
            /** normal code */
            offset += run_table[code];
            sign = get_bits1(gb) - 1;
            //iptr[offset & coef_mask] = ilvl[code] ^ sign<<31;
            
            level = level_table[code];
            if(!sign) 
            	level = -level;
            ptr[offset & coef_mask] = level;
            
        } else if (code == 1) {
            /** EOB */
            break;
        } else {
            /** escape */
            if (!version) {
                level = get_bits(gb, coef_nb_bits);
                /** NOTE: this is rather suboptimal. reading
                    block_len_bits would be better */
                offset += get_bits(gb, frame_len_bits);
            } else {
                level = ff_wma_get_large_val(gb);
                /** escape decode */
                if (get_bits1(gb)) {
                    if (get_bits1(gb)) {
                        if (get_bits1(gb)) {
                            av_log(avctx,AV_LOG_ERROR,
                                "broken escape sequence\n");
                            return -1;
                        } else
                            offset += get_bits(gb, frame_len_bits) + 4;
                    } else
                        offset += get_bits(gb, 2) + 1;
                }
            }
            sign = get_bits1(gb) - 1;
            if(!sign) 
            	level = -level;

            ptr[offset & coef_mask] = level; //(level^sign) - sign;
        }
    }
    /** NOTE: EOB can be omitted */
    if (offset > num_coefs) {
        av_log(avctx, AV_LOG_ERROR, "overflow in spectral RLE, ignoring\n");
        return -1;
    }
    
    //LOGW("fixed_ff_wma_run_level_decode OK");

    return 0;
}


/**
 * @return 0 if OK. 1 if last block of frame. return -1 if
 * unrecorrable error.
 */
static int fixed_wma_decode_block(WMADecodeContext *s)
{
    int n, v, a, ch, bsize;
    int coef_nb_bits, total_gain;
    int nb_coefs[MAX_CHANNELS];
    fixed32 mdct_norm;
    
    //LOGW("fixed_wma_decode_block");

    /* compute current block length */
    if (s->use_variable_block_len) {
        n = av_log2(s->nb_block_sizes - 1) + 1;

        if (s->reset_block_lengths) {
            s->reset_block_lengths = 0;
            v = get_bits(&s->gb, n);
            if (v >= s->nb_block_sizes){
                av_log(s->avctx, AV_LOG_ERROR, "prev_block_len_bits %d out of range\n", s->frame_len_bits - v);
                return -1;
            }
            s->prev_block_len_bits = s->frame_len_bits - v;
            v = get_bits(&s->gb, n);
            if (v >= s->nb_block_sizes){
                av_log(s->avctx, AV_LOG_ERROR, "block_len_bits %d out of range\n", s->frame_len_bits - v);
                return -1;
            }
            s->block_len_bits = s->frame_len_bits - v;
        } else {
            /* update block lengths */
            s->prev_block_len_bits = s->block_len_bits;
            s->block_len_bits = s->next_block_len_bits;
        }
        v = get_bits(&s->gb, n);
        if (v >= s->nb_block_sizes){
            av_log(s->avctx, AV_LOG_ERROR, "next_block_len_bits %d out of range\n", s->frame_len_bits - v);
            return -1;
        }
        s->next_block_len_bits = s->frame_len_bits - v;
    } else {
        /* fixed block len */
        s->next_block_len_bits = s->frame_len_bits;
        s->prev_block_len_bits = s->frame_len_bits;
        s->block_len_bits = s->frame_len_bits;
    }
    
    /* now check if the block length is coherent with the frame length */
    s->block_len = 1 << s->block_len_bits;
    if ((s->block_pos + s->block_len) > s->frame_len){
        av_log(s->avctx, AV_LOG_ERROR, "frame_len overflow\n");
        return -1;
    }

    if (s->nb_channels == 2) {
        s->ms_stereo = get_bits1(&s->gb);
    }
    v = 0;
    for(ch = 0; ch < s->nb_channels; ch++) {
        a = get_bits1(&s->gb);
        s->channel_coded[ch] = a;
        //LOGW("s->channel_coded[%d] = %d", ch, a);
        v |= a;
    }

    bsize = s->frame_len_bits - s->block_len_bits;

    /* if no channel coded, no need to go further */
    /* XXX: fix potential framing problems */
    if (!v)
        goto next;

    /* read total gain and extract corresponding number of bits for
       coef escape coding */
    total_gain = 1;
    for(;;) {
        a = get_bits(&s->gb, 7);
        total_gain += a;
        if (a != 127)
            break;
    }

    coef_nb_bits= ff_wma_total_gain_to_bits(total_gain);
    
    /* compute number of coefficients */
    n = s->coefs_end[bsize] - s->coefs_start;
    for(ch = 0; ch < s->nb_channels; ch++)
        nb_coefs[ch] = n;
    
    /* complex coding */
    if (s->use_noise_coding) {

        for(ch = 0; ch < s->nb_channels; ch++) {
            if (s->channel_coded[ch]) {
                int i, n, a;
                n = s->exponent_high_sizes[bsize];
                for(i=0;i<n;i++) {
                    a = get_bits1(&s->gb);
                    s->high_band_coded[ch][i] = a;
                    /* if noise coding, the coefficients are not transmitted */
                    if (a)
                        nb_coefs[ch] -= s->exponent_high_bands[bsize][i];
                }
            }
        }
        for(ch = 0; ch < s->nb_channels; ch++) {
            if (s->channel_coded[ch]) {
                int i, n, val, code;

                n = s->exponent_high_sizes[bsize];
                val = (int)0x80000000;
                for(i=0;i<n;i++) {
                    if (s->high_band_coded[ch][i]) {
                        if (val == (int)0x80000000) {
                            val = get_bits(&s->gb, 7) - 19;
                        } else {
                            code = get_vlc2(&s->gb, s->hgain_vlc.table, HGAINVLCBITS, HGAINMAX);
                            if (code < 0){
                                av_log(s->avctx, AV_LOG_ERROR, "hgain vlc invalid\n");
                                return -1;
                            }
                            val += code - 18;
                        }
                        s->high_band_values[ch][i] = val;
                    }
                }
            }
        }
    }
    
    /* exponents can be reused in short blocks. */
    if ((s->block_len_bits == s->frame_len_bits) ||
        get_bits1(&s->gb)) {
        for(ch = 0; ch < s->nb_channels; ch++) {
            if (s->channel_coded[ch]) {
                if (s->use_exp_vlc) {
                    if (fixed_decode_exp_vlc(s, ch) < 0) {
                        //LOGE("error 1");
                    	return -1;
                    }
                } else {
                    fixed_decode_exp_lsp(s, ch);
                }
                s->exponents_bsize[ch] = bsize;
            }
        }
    }
    
    /* parse spectral coefficients : just RLE encoding */
    for(ch = 0; ch < s->nb_channels; ch++) {
        if (s->channel_coded[ch]) {
            int tindex;
            int16_t* ptr = &s->coefs1[ch][0];

            /* special VLC tables are used for ms stereo because
               there is potentially less energy there */
            tindex = (ch == 1 && s->ms_stereo);
            
            memset(ptr, 0, s->block_len * sizeof(int16_t));
            fixed_ff_wma_run_level_decode(s->avctx, &s->gb, &s->coef_vlc[tindex],
                  s->level_table[tindex], s->run_table[tindex],
                  0, ptr, 0, nb_coefs[ch],
                  s->block_len, s->frame_len_bits, coef_nb_bits);
        }
        if (s->version == 1 && s->nb_channels >= 2) {
            align_get_bits(&s->gb);
        }
    }

    /* normalize */
    {
        int n4 = s->block_len >> 1;
        mdct_norm = 0x10000>>(s->block_len_bits - 1);
        
        if(s->version == 1) {
            mdct_norm *= fixtoi32(fixsqrt32(itofix32(n4)));
        } else {
        	mdct_norm = (mdct_norm << 2) / 3;
        }
    }
    
    /* finally compute the MDCT coefficients */
    for(ch = 0; ch < s->nb_channels; ch++) {
        if (s->channel_coded[ch]) {
            int16_t *coefs1;
            fixed32 *coefs, *exponents, noise;
            int i, j, n, n1, last_high_band, esize;
            fixed32 exp_power[HIGH_BAND_MAX_SIZE];
			fixed32 temp1, temp2, mult2, atemp;
            fixed64 mult;
            fixed64 mult1;
            
            coefs1 = s->coefs1[ch];
            exponents = s->exponents[ch];
            esize = s->exponents_bsize[ch];
            coefs = s->coefs[ch];

            //mult = pow(10, total_gain * 0.05) / s->max_exponent[ch];
            //mult *= mdct_norm;
            
            if (s->use_noise_coding) {
            	//LOGW("noise coding");
            	//mult1 = mult;
                
            	mult = fixdiv64(pow_table[total_gain+20],Fixed32To64(s->max_exponent[ch]));
                mult = mult* mdct_norm;
                mult1 = mult;

                /* very low freqs : noise */
                for(i = 0;i < s->coefs_start; i++) {
                	
                    *coefs++ = fixmul32( (fixmul32(s->noise_table[s->noise_index],
                            exponents[i<<bsize>>esize])>>4),Fixed32From64(mult1)) >>2;
                    s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                }

                n1 = s->exponent_high_sizes[bsize];

                /* compute power of high bands */
//                exponents = s->exponents[ch] +
//                    (s->high_band_start[bsize]<<bsize>>esize); // DIFF! no >>esize for fp
                exponents = s->exponents[ch] + (s->high_band_start[bsize]<<bsize); // DIFF! no >>esize for fp
                last_high_band = 0; /* avoid warning */
                for(j=0;j<n1;j++) {
                    n = s->exponent_high_bands[s->frame_len_bits -
                                              s->block_len_bits][j];
                    if (s->high_band_coded[ch][j]) {
                        fixed32 e2, v;
                        e2 = 0;
                        for(i = 0;i < n; i++) {
                            //v = exponents[i<<bsize>>esize];
                            //e2 += v * v;
                            /*v is normalized later on so its fixed format is irrelevant*/
                            v = exponents[i<<bsize>>esize]>>4;
                            e2 += fixmul32(v, v)>>3;
                        }
                        exp_power[j] = e2 / n;
                        last_high_band = j;
                        tprintf(s->avctx, "%d: power=%f (%d)\n", j, exp_power[j], n);
                    }
                    //exponents += n<<bsize>>esize; // DIFF no >>esize for fp
                    exponents += n<<bsize; // DIFF no >>esize for fp
                }

                /* main freqs and high freqs */
                //exponents = s->exponents[ch] + (s->coefs_start<<bsize>>esize); // DIFF esize
                exponents = s->exponents[ch] + (s->coefs_start<<bsize); // DIFF esize
                for(j=-1;j<n1;j++) {
                    if (j < 0) {
                        n = s->high_band_start[bsize] -
                            s->coefs_start;
                    } else {
                        n = s->exponent_high_bands[s->frame_len_bits -
                                                  s->block_len_bits][j];
                    }
                    
                    if (j >= 0 && s->high_band_coded[ch][j]) {
                    	fixed32 tmp = fixdiv32(exp_power[j], exp_power[last_high_band]);
                        /*mult1 is 48.16, pow_table is 48.16*/
                        mult1 = fixmul32(fixsqrt32(tmp),
                                pow_table[s->high_band_values[ch][j]+20]) >> 16;
                        /*this step has a fairly high degree of error for some reason*/
                        mult1 = fixdiv64(mult1,fixmul32(s->max_exponent[ch],s->noise_mult));
                        mult1 = mult1*mdct_norm>>PRECISION;
                        
                        for(i = 0;i < n; ++i) {
                            noise = s->noise_table[s->noise_index];
                            s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                            *coefs++ = fixmul32((fixmul32(exponents[i<<bsize>>esize],noise)>>4),
                                    Fixed32From64(mult1)) >>2;

                        }
						//exponents += n<<bsize>>esize; // DIFF esize
                        exponents += n<<bsize; // DIFF esize
                    } else {
                        /* coded values + small noise */
                        for(i = 0;i < n; i++) {
                            noise = s->noise_table[s->noise_index];
                            s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                            
                           temp1 = (((int32_t)*coefs1++)<<16) + (noise>>4);
                           temp2 = fixmul32(exponents[i<<bsize>>esize], mult>>18);
                           *coefs++ = fixmul32(temp1, temp2);
                        }
                        //exponents += n<<bsize>>esize; // DIFF esize
                        exponents += n<<bsize; // DIFF esize
                    }
                }
                
                /* very high freqs : noise */
                n = s->block_len - s->coefs_end[bsize];
                //mult1 = mult * exponents[((-1<<bsize))>>esize];
                mult2 = fixmul32(mult>>16,exponents[((-1<<bsize))>>esize]) ;
                for(i = 0; i < n; i++) {
                    /*renormalize the noise product and then reduce to 14.18 precison*/
                    *coefs++ = fixmul32(s->noise_table[s->noise_index],mult2) >>5;
                    s->noise_index = (s->noise_index + 1) & (NOISE_TAB_SIZE - 1);
                }
            } else {
                /*Noise coding not used, simply convert from exp to fixed representation*/
                fixed32 mult3 = (fixed32)(fixdiv64(pow_table[total_gain+20],
                        Fixed32To64(s->max_exponent[ch])));
                mult3 = fixmul32(mult3, mdct_norm);

                /*zero the first 3 coefficients for WMA V1, does nothing otherwise*/
                for(i=0; i<s->coefs_start; i++)
                    *coefs++=0;

                n = nb_coefs[ch];

                for(i = 0;i < n; ++i) {
                	atemp = (coefs1[i] * mult3) >> 1; 
                    *coefs++=fixmul32(atemp,exponents[i<<bsize>>esize]);
                }
                n = s->block_len - s->coefs_end[bsize];
                memset(coefs, 0, n*sizeof(fixed32));
            }
        }
    }
    

    if (s->ms_stereo && s->channel_coded[1]) {
        /* nominal case for ms stereo: we do it before mdct */
        /* no need to optimize this case because it should almost
           never happen */
    	
        fixed32 a, b;
        int i;
    	
        if (!s->channel_coded[0]) {
            memset(s->coefs[0], 0, sizeof(fixed32) * s->block_len);
            s->channel_coded[0] = 1;
        }

        for(i = 0; i < s->block_len; ++i) {
            a = s->coefs[0][i];
            b = s->coefs[1][i];
            s->coefs[0][i] = a + b;
            s->coefs[1][i] = a - b;
        }
    }

next:
	for(ch = 0; ch < s->nb_channels; ch++) {
		//static int32_t scratch_buf[BLOCK_MAX_SIZE * MAX_CHANNELS];		
        int n4, index;
        n4 = s->block_len / 2;
        
        if(s->channel_coded[ch]){
        	fixed_ff_imdct_calc(&s->mdct_ctx[bsize],
			  s->scratch_buf,
			  s->coefs[ch]);
        } else if(!(s->ms_stereo && ch==1)) {
        	memset(s->scratch_buf, 0, sizeof(s->scratch_buf));
        }

        /* multiply by the window and add in the frame */
        index = (s->frame_len / 2) + s->block_pos - n4;
        
        fixed_wma_window(s, s->scratch_buf, &s->frame_out[ch][index]);
    }

    /* update block number */
    s->block_num++;
    s->block_pos += s->block_len;
    
    if (s->block_pos >= s->frame_len)
        return 1;
    else
        return 0;
}


static int fixed_wma_decode_frame(WMADecodeContext *s, int16_t *samples)
{
	int ret, i, n, ch, incr;
    int16_t *ptr;
    fixed32 *iptr;

    /* read each block */
    s->block_num = 0;
    s->block_pos = 0;

    for(;;) {
        ret = fixed_wma_decode_block(s);
        
        if (ret < 0) {
        	return -1;
        }
        if (ret) {
        	
            break;
        }
    }
    
            
    n = s->frame_len;
    incr = s->nb_channels;
    
	for(ch = 0; ch < s->nb_channels; ch++) {
		ptr = samples + ch;
		iptr = s->frame_out[ch];
		
		for(i=0;i<n;i++) {
			*ptr = (*iptr++) >> PRECISION;
			ptr += incr;
		}
		
		/* prepare for next block */
		memmove(&s->frame_out[ch][0], &s->frame_out[ch][s->frame_len],
				s->frame_len * sizeof(fixed32));
	}

	return 0;
}


static int fixed_wma_decode_superframe(AVCodecContext *avctx,
                                 void *data, int *data_size,
                                 AVPacket *avpkt)
{
    const uint8_t *buf = avpkt->data;
    int buf_size = avpkt->size;
    WMADecodeContext *s = avctx->priv_data;
    int nb_frames, bit_offset, i, pos, len;
    uint8_t *q;
    int16_t *samples;

    if(buf_size==0){
        s->last_superframe_len = 0;
        return 0;
    }
    if (buf_size < s->block_align)
        return 0;
    buf_size = s->block_align;

    samples = data;

    init_get_bits(&s->gb, buf, buf_size*8);

    if (s->use_bit_reservoir) {
        /* read super frame header */
        skip_bits(&s->gb, 4); /* super frame index */
        nb_frames = get_bits(&s->gb, 4) - 1;

        if((nb_frames+1) * s->nb_channels * s->frame_len * sizeof(int16_t) > *data_size){
            av_log(s->avctx, AV_LOG_ERROR, "Insufficient output space\n");
            goto fail;
        }

        bit_offset = get_bits(&s->gb, s->byte_offset_bits + 3);

        if (s->last_superframe_len > 0) {
            //        printf("skip=%d\n", s->last_bitoffset);
            /* add bit_offset bits to last frame */
            if ((s->last_superframe_len + ((bit_offset + 7) >> 3)) >
                MAX_CODED_SUPERFRAME_SIZE)
                goto fail;
            q = s->last_superframe + s->last_superframe_len;
            len = bit_offset;
            while (len > 7) {
                *q++ = (get_bits)(&s->gb, 8);
                len -= 8;
            }
            if (len > 0) {
                *q++ = (get_bits)(&s->gb, len) << (8 - len);
            }

            /* XXX: bit_offset bits into last frame */
            init_get_bits(&s->gb, s->last_superframe, MAX_CODED_SUPERFRAME_SIZE*8);
            /* skip unused bits */
            if (s->last_bitoffset > 0)
                skip_bits(&s->gb, s->last_bitoffset);
            /* this frame is stored in the last superframe and in the
               current one */

            if (fixed_wma_decode_frame(s, samples) < 0)
                goto fail;
            samples += s->nb_channels * s->frame_len;
        }

        /* read each frame starting from bit_offset */
        pos = bit_offset + 4 + 4 + s->byte_offset_bits + 3;
        init_get_bits(&s->gb, buf + (pos >> 3), (MAX_CODED_SUPERFRAME_SIZE - (pos >> 3))*8);
        len = pos & 7;
        if (len > 0)
            skip_bits(&s->gb, len);

        s->reset_block_lengths = 1;
        for(i=0;i<nb_frames;i++) {

        	if (fixed_wma_decode_frame(s, samples) < 0)
                goto fail;
            samples += s->nb_channels * s->frame_len;
        }

        /* we copy the end of the frame in the last frame buffer */
        pos = get_bits_count(&s->gb) + ((bit_offset + 4 + 4 + s->byte_offset_bits + 3) & ~7);
        s->last_bitoffset = pos & 7;
        pos >>= 3;
        len = buf_size - pos;
        if (len > MAX_CODED_SUPERFRAME_SIZE || len < 0) {
            av_log(s->avctx, AV_LOG_ERROR, "len %d invalid\n", len);
            goto fail;
        }
        s->last_superframe_len = len;
        //LOGW("memcpy last_superframe");
        memcpy(s->last_superframe, buf + pos, len);
    } else {
        if(s->nb_channels * s->frame_len * sizeof(int16_t) > *data_size){
            av_log(s->avctx, AV_LOG_ERROR, "Insufficient output space\n");
            goto fail;
        }
        /* single frame decode */
        if (fixed_wma_decode_frame(s, samples) < 0)
            goto fail;
        samples += s->nb_channels * s->frame_len;
    }

//av_log(NULL, AV_LOG_ERROR, "%d %d %d %d outbytes:%d eaten:%d\n", s->frame_len_bits, s->block_len_bits, s->frame_len, s->block_len,        (int8_t *)samples - (int8_t *)data, s->block_align);

    *data_size = (int8_t *)samples - (int8_t *)data;
    return s->block_align;
 fail:
    /* when error, we reset the bit reservoir */
    s->last_superframe_len = 0;
    return -1;
}

static av_cold void fixed_flush(AVCodecContext *avctx)
{
    WMADecodeContext *s = avctx->priv_data;

    s->last_bitoffset=
    s->last_superframe_len= 0;
}

int fixed_ff_wma_end(AVCodecContext *avctx)
{
    WMADecodeContext *s = avctx->priv_data;
    int i;

    //for (i = 0; i < s->nb_block_sizes; i++)
    //    ff_mdct_end(&s->mdct_ctx[i]);

    if (s->use_exp_vlc) {
        free_vlc(&s->exp_vlc);
    }
    if (s->use_noise_coding) {
        free_vlc(&s->hgain_vlc);
    }
    for (i = 0; i < 2; i++) {
        free_vlc(&s->coef_vlc[i]);
        av_free(s->run_table[i]);
        av_free(s->level_table[i]);
        //av_free(s->int_table[i]);
    }

    return 0;
}

AVCodec wmav1_fixedpoint_decoder =
{
    "wmav1",
    AVMEDIA_TYPE_AUDIO,
    CODEC_ID_WMAV1,
    sizeof(WMADecodeContext),
    fixed_wma_decode_init,
    NULL,
    fixed_ff_wma_end,
    fixed_wma_decode_superframe,
    .flush=fixed_flush,
    .long_name = NULL_IF_CONFIG_SMALL("Windows Media Audio 1"),
};

AVCodec wmav2_fixedpoint_decoder =
{
    "wmav2",
    AVMEDIA_TYPE_AUDIO,
    CODEC_ID_WMAV2,
    sizeof(WMADecodeContext),
    fixed_wma_decode_init,
    NULL,
    fixed_ff_wma_end,
    fixed_wma_decode_superframe,
    .flush=fixed_flush,
    .long_name = NULL_IF_CONFIG_SMALL("Windows Media Audio 2"),
};
