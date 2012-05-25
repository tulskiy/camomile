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

#ifndef AVCODEC_WMADEC_FIXEDPOINT_H
#define AVCODEC_WMADEC_FIXEDPOINT_H

#define CONFIG_FIXED_POINT

#include "wma.h"
#include "get_bits.h"

#define fixed32         int32_t
#define fixed64         int64_t

#define M_PI    3.14159265358979323846
#define M_PI_F  0x3243f // in fixed 32 format
#define TWO_M_PI_F  0x6487f   //in fixed 32

#define EXPVLCBITS 8
#define EXPMAX ((19+EXPVLCBITS-1)/EXPVLCBITS)

#define HGAINVLCBITS 9
#define HGAINMAX ((13+HGAINVLCBITS-1)/HGAINVLCBITS)

typedef int32_t FFTSampleFixed;

typedef struct FFTComplexFixed {
    int32_t re, im;
} FFTComplexFixed;

typedef struct FFTContextFixed {
    int nbits;
    int inverse;
} FFTContextFixed;


typedef struct MDCTContextFixed {
    int n;     /* size of MDCT (i.e. number of input data * 2) */
    int nbits; /* n = 2^nbits */
    /* pre/post rotation tables */
    int32_t *tcos;
    int32_t *tsin;
    FFTContextFixed fft;
} MDCTContextFixed;


typedef struct WMADecodeContext
{
    AVCodecContext* avctx;
	GetBitContext gb;

    int sample_rate;
    int nb_channels;
    int bit_rate;
    int version; /* 1 = 0x160 (WMAV1), 2 = 0x161 (WMAV2) */
    int block_align;
    int use_bit_reservoir;
    int use_variable_block_len;
    int use_exp_vlc;  /* exponent coding: 0 = lsp, 1 = vlc + delta */
    int use_noise_coding; /* true if perceptual noise is added */
    int byte_offset_bits;
    VLC exp_vlc;
    int exponent_sizes[BLOCK_NB_SIZES];
    uint16_t exponent_bands[BLOCK_NB_SIZES][25];
    int high_band_start[BLOCK_NB_SIZES]; /* index of first coef in high band */
    int coefs_start;               /* first coded coef */
    int coefs_end[BLOCK_NB_SIZES]; /* max number of coded coefficients */
    int exponent_high_sizes[BLOCK_NB_SIZES];
    int exponent_high_bands[BLOCK_NB_SIZES][HIGH_BAND_MAX_SIZE];
    VLC hgain_vlc;

    /* coded values in high bands */
    int high_band_coded[MAX_CHANNELS][HIGH_BAND_MAX_SIZE];
    int high_band_values[MAX_CHANNELS][HIGH_BAND_MAX_SIZE];

    /* there are two possible tables for spectral coefficients */
    VLC coef_vlc[2];
    uint16_t *run_table[2];
    uint16_t *level_table[2];
    const CoefVLCTable *coef_vlcs[2];
    
    /* frame info */
    int frame_len;       /* frame length in samples */
    int frame_len_bits;  /* frame_len = 1 << frame_len_bits */
    int nb_block_sizes;  /* number of block sizes */
    
    /* block info */
    int reset_block_lengths;
    int block_len_bits; /* log2 of current block length */
    int next_block_len_bits; /* log2 of next block length */
    int prev_block_len_bits; /* log2 of prev block length */
    int block_len; /* block length in samples */
    int block_num; /* block number in current frame */
    int block_pos; /* current position in frame */
    uint8_t ms_stereo; /* true if mid/side stereo mode */
    
    uint8_t channel_coded[MAX_CHANNELS]; /* true if channel is coded */
    int exponents_bsize[MAX_CHANNELS];      // log2 ratio frame/exp. length
    DECLARE_ALIGNED(16, fixed32, exponents)[MAX_CHANNELS][BLOCK_MAX_SIZE];
    fixed32 max_exponent[MAX_CHANNELS];
    int16_t coefs1[MAX_CHANNELS][BLOCK_MAX_SIZE];
    //fixed32 (*coefs)[MAX_CHANNELS][BLOCK_MAX_SIZE];
    DECLARE_ALIGNED(16, fixed32, coefs)[MAX_CHANNELS][BLOCK_MAX_SIZE];
    
    MDCTContextFixed mdct_ctx[BLOCK_NB_SIZES];
    
    fixed32 *windows[BLOCK_NB_SIZES];
    /* output buffer for one frame and the last for IMDCT windowing */
    
    DECLARE_ALIGNED(16, fixed32, frame_out)[MAX_CHANNELS][BLOCK_MAX_SIZE*2];

    /* last frame info */
    DECLARE_ALIGNED(16, uint8_t, last_superframe)[MAX_CODED_SUPERFRAME_SIZE + 4]; /* padding added */
    int last_bitoffset;
    int last_superframe_len;
    fixed32 *noise_table;
    int noise_index;
    fixed32 noise_mult; /* XXX: suppress that and integrate it in the noise array */
    /* lsp_to_curve tables */
    DECLARE_ALIGNED(16, fixed32, lsp_cos_table)[BLOCK_MAX_SIZE];
    //void *lsp_pow_m_table1;
    //void *lsp_pow_m_table2;
    VLC_TYPE lsp_pow_m_table1[(1 << LSP_POW_BITS)];
    VLC_TYPE lsp_pow_m_table2[(1 << LSP_POW_BITS)];
    
	DECLARE_ALIGNED(16, fixed32, stat0)[2048]; 
	DECLARE_ALIGNED(16, fixed32, stat1)[1024]; 
	DECLARE_ALIGNED(16, fixed32, stat2)[512]; 
	DECLARE_ALIGNED(16, fixed32, stat3)[256];
	DECLARE_ALIGNED(16, fixed32, stat4)[128];
	
	int32_t scratch_buf[BLOCK_MAX_SIZE * MAX_CHANNELS];

#ifdef TRACE

    int frame_count;
#endif
} WMADecodeContext;

#endif
