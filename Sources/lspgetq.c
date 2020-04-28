/* ITU-T G.729 Software Package Release 2 (November 2006) */
/*
 ITU-T G.729A Speech Coder    ANSI-C Source Code
 Version 1.1    Last modified: September 1996

 Copyright (c) 1996,
 AT&T, France Telecom, NTT, Universite de Sherbrooke, Lucent Technologies
 All rights reserved.
 */

#include <stdio.h>
#include "typedef.h"
#include "basic_op.h"
#include "ld8a.h"

void Lsp_get_quant(const Word16 lspcb1[][M], /* (i) Q13 : first stage LSP codebook      */
const Word16 lspcb2[][M], /* (i) Q13 : Second stage LSP codebook     */
Word16 code0, /* (i)     : selected code of first stage  */
Word16 code1, /* (i)     : selected code of second stage */
Word16 code2, /* (i)     : selected code of second stage */
const Word16 fg[][M], /* (i) Q15 : MA prediction coef.           */
Word16 freq_prev[MA_NP][M], /* (i) Q13 : previous LSP vector           */
Word16 lspq[], /* (o) Q13 : quantized LSP parameters      */
const Word16 fg_sum[] /* (i) Q15 : present MA prediction coef.   */
) {
	Word16 j;
	Word16 buf[M]; /* Q13 */

	for (j = 0; j < NC; j++)
		buf[j] = add(lspcb1[code0][j], lspcb2[code1][j]);

	for (j = NC; j < M; j++)
		buf[j] = add(lspcb1[code0][j], lspcb2[code2][j]);

	Lsp_expand_1_2(buf, GAP1);
	Lsp_expand_1_2(buf, GAP2);

	Lsp_prev_compose(buf, lspq, fg, (const Word16 (*)[M]) freq_prev, fg_sum);

	Lsp_prev_update(buf, freq_prev);

	Lsp_stability(lspq);

	return;
}

void Lsp_expand_1(Word16 buf[], /* (i/o) Q13 : LSP vectors */
Word16 gap /* (i)   Q13 : gap         */
) {
	Word16 j, tmp;
	Word16 diff; /* Q13 */

	for (j = 1; j < NC; j++) {
		diff = sub(buf[j - 1], buf[j]);
		tmp = shr(add(diff, gap), 1);

		if (tmp > 0) {
			buf[j - 1] = sub(buf[j - 1], tmp);
			buf[j] = add(buf[j], tmp);
		}
	}
	return;
}

void Lsp_expand_2(Word16 buf[], /* (i/o) Q13 : LSP vectors */
Word16 gap /* (i)   Q13 : gap         */
) {
	Word16 j, tmp;
	Word16 diff; /* Q13 */

	for (j = NC; j < M; j++) {
		diff = sub(buf[j - 1], buf[j]);
		tmp = shr(add(diff, gap), 1);

		if (tmp > 0) {
			buf[j - 1] = sub(buf[j - 1], tmp);
			buf[j] = add(buf[j], tmp);
		}
	}
	return;
}

void Lsp_expand_1_2(Word16 buf[], /* (i/o) Q13 : LSP vectors */
Word16 gap /* (i)   Q13 : gap         */
) {
	Word16 j, tmp;
	Word16 diff; /* Q13 */

	for (j = 1; j < M; j++) {
		diff = sub(buf[j - 1], buf[j]);
		tmp = shr(add(diff, gap), 1);

		if (tmp > 0) {
			buf[j - 1] = sub(buf[j - 1], tmp);
			buf[j] = add(buf[j], tmp);
		}
	}
	return;
}

/*
 Functions which use previous LSP parameter (freq_prev).
 */

/*
 compose LSP parameter from elementary LSP with previous LSP.
 */
void Lsp_prev_compose(const Word16 lsp_ele[restrict M], /* (i) Q13 : LSP vectors                 */
Word16 lsp[restrict M], /* (o) Q13 : quantized LSP parameters    */
const Word16 fg[restrict MA_NP][M], /* (i) Q15 : MA prediction coef.         */
const Word16 freq_prev[restrict MA_NP][M], /* (i) Q13 : previous LSP vector         */
const Word16 fg_sum[restrict M] /* (i) Q15 : present MA prediction coef. */
) {
	Word16 j, k;
	Word32 L_acc; /* Q29 */

	for (j = 0; j < M; j++) {
		L_acc = L_mult(lsp_ele[j], fg_sum[j]);
		for (k = 0; k < MA_NP; k++)
			L_acc = L_mac(L_acc, freq_prev[k][j], fg[k][j]);

		lsp[j] = extract_h(L_acc);
	}
	return;
}

/*
 extract elementary LSP from composed LSP with previous LSP
 */
void Lsp_prev_extract(const Word16 lsp[M], /* (i) Q13 : unquantized LSP parameters  */
Word16 lsp_ele[M], /* (o) Q13 : target vector               */
const Word16 fg[MA_NP][M], /* (i) Q15 : MA prediction coef.         */
const Word16 freq_prev[MA_NP][M], /* (i) Q13 : previous LSP vector         */
const Word16 fg_sum_inv[M] /* (i) Q12 : inverse previous LSP vector */
) {
	Word16 j, k;
	Word32 L_temp; /* Q19 */
	Word16 temp; /* Q13 */

	for (j = 0; j < M; j++) {
		L_temp = L_deposit_h(lsp[j]);
		for (k = 0; k < MA_NP; k++)
			L_temp = L_msu(L_temp, freq_prev[k][j], fg[k][j]);

		temp = extract_h(L_temp);
		L_temp = L_mult(temp, fg_sum_inv[j]);
		lsp_ele[j] = extract_h(L_shl(L_temp, 3));

	}
	return;
}

extern void g729_itu_debug_out(const char *msg);
void Lsp_stability(Word16 buf[] /* (i/o) Q13 : quantized LSP parameters      */
) {
	Word16 j;
	Word16 tmp;
	Word32 L_diff;
	Word32 L_acc, L_accb;

	for (j = 0; j < M - 1; j++) {
		L_acc = L_deposit_l(buf[j + 1]);
		L_accb = L_deposit_l(buf[j]);
		L_diff = L_sub(L_acc, L_accb);

		if (L_diff < 0L) {
			/* exchange buf[j]<->buf[j+1] */
			tmp = buf[j + 1];
			buf[j + 1] = buf[j];
			buf[j] = tmp;
		}
	}

	if (sub(buf[0], L_LIMIT) < 0) {
		buf[0] = L_LIMIT;
		g729_itu_debug_out("lsp_stability warning Low");
	}
	for (j = 0; j < M - 1; j++) {
		L_acc = L_deposit_l(buf[j + 1]);
		L_accb = L_deposit_l(buf[j]);
		L_diff = L_sub(L_acc, L_accb);

		if (L_sub(L_diff, GAP3) < 0L) {
			buf[j + 1] = add(buf[j], GAP3);
		}
	}

	if (sub(buf[M - 1], M_LIMIT) > 0) {
		buf[M - 1] = M_LIMIT;
		g729_itu_debug_out("lsp_stability warning High");
	}
	return;
}
