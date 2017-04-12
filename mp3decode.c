/*
http://blog.bjrn.se/2008/10/lets-build-mp3-decoder.html
http://home.deib.polimi.it/dossi/fond_tlc/mpeg1layer3.pdf (bottom)
https://upload.wikimedia.org/wikipedia/commons/0/01/Mp3filestructure.svg
https://patentimages.storage.googleapis.com/US7689429B2/US07689429-20100330-D00000.png
https://github.com/piaopolar/mp3decode
*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>


void
freadordie(unsigned char *buf, size_t count, int emptyok)
{
	size_t got = fread(buf, 1, count, stdin);
	if (ferror(stdin)) {
		perror("mp3decode: fread");
		exit(EXIT_FAILURE);
	} else if (emptyok && !got) {
		exit(EXIT_SUCCESS);
	} else if (got != count) {
		fprintf(stderr, "mp3decode: expected %lu more bytes, got %lu\n", count, got);
		exit(EXIT_FAILURE);
	}
}

int
main(void)
{
	unsigned char buf[10];
	while (1) {
		// Header
		freadordie(buf, 4, 1);
		// Skip ID3 info
		if (!strncmp(buf, "ID3", 3)) {
			freadordie(&buf[4], 6, 0);
			uint32_t size = 0;
			for (int i=0; i<4; i++)
				size += sizebuf[6+i] << 7*(3-i);
			unsigned char _[size];
			freadordie(_, size, 0);
			if (buf[5] & 0x10)  // footer
				freadordie(buf, 10, 0);
			continue;
		}
		if (buf[1] & 1)
			fread(buf, 2, 0);
		main_data_end 9 uimsbf
		private_bits 3 bslbf
		ch0, 1:
			scfsi_band0, 1, 2, 3:
				scsfi[scsfi_band][ch] 1 bslbf
		gr0, 1:
			ch0, 1:
				part2_3_length[gr][ch] 12 uimsbf
				big_values[gr][ch] 9 uimsbf
				global_gain[gr][ch] 8 uimsbf
				scalefac_compress[gr][ch] 4 bslbf
				blocksplit_flag[gr][ch] 1 bslbf
				if blocksplit_flag[gr][ch]:
					block_type[gr][ch] 2 bslbf
					switch_point[gr][ch] 1 uimsbf
					region0, 1:
						table_select[region][gr][ch] 5 bslbf
					window0, 1, 2:
						subblock_gain[window][gr][ch] 3 uimsbf
				else:
					region0, 1, 2:
						table_select[region][gr][ch] 5 bslbf
					region_address1[gr][ch] 4 bslbf
					region_address2[gr][ch] 3 bslbf
				preflag[gr][ch] 1 bslbf
				scalefac_scale[gr][ch] 1 bslbf
				count1table_select[gr][ch] 1 bslbf
		gr0, 1: // from main_data_end
			ch0, 1:
				if blocksplit_flag[gr][ch] and block_type[gr][ch]==2:
					cb0..switch_point_l[gr][ch]:
						if scfsi[cb]==0 or gr==0:
							scalefac[cb][gr][ch] 0..4 uimsbf
					cb`switch_point_s[gr][ch]..cblimit_short
						window0, 1, 2:
							if scfsi[cb]==0 or gr==0:
								scalefac[cb][window][gr][ch] 0..4 uimsbf
				else:
					cb0,..cblimit:
						if scfsi[cb]==0 or gr==0:
							scalefac[cb][gr][ch] 0..4 uimsbf
				Huffmancodebits (part2_3_length-part2_length) bslbf
				while position != main_data_end:
					ancillary_bit 1 bslbf
	}
}
