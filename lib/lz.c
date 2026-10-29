#include <lib/lz.h>
#include <lib/syscall.h>

/* Helper for simple buffer management */
struct databuffer {
    uint8_t *data;
    uint32_t size;
};

/* struct compressed struct for handling compressed data */
struct compressed {
    uint8_t *comp_data; 
    uint32_t comp_size; 
    uint32_t orig_size; 
    uint8_t ptr_len_width;
};

/* Frees allocated buffer */
void free_buffer(struct databuffer *buf) {
    free(buf->data);
    free(buf);
}

/* Quick analysis of repetitive patterns to estimate the best pointer length width */
uint8_t estimate_best_ptr_len_width(struct databuffer *input) {
    uint32_t total_match_length = 0;
    uint32_t match_count = 0;

    /* Loop through input data to find repetition patterns */
    for (uint32_t code_pos = 1; code_pos < input->size; ++code_pos) {
        for (uint32_t temp_p_pos = 1; temp_p_pos <= code_pos; ++temp_p_pos) {
            uint32_t look_b = code_pos - temp_p_pos;
            uint32_t look_a = code_pos;
            uint32_t temp_p_len = 0;

            while (look_a < input->size && input->data[look_a] == input->data[look_b]) {
                temp_p_len++;
                look_a++;
                look_b++;
            }

            /* If a match was found, accumulate its length */
            if (temp_p_len > 0) {
                total_match_length += temp_p_len;
                match_count++;
                /* Stop after finding the first match for this position */
                break; 
            }
        }
    }

    if (match_count == 0) {
        return 5;
    }


    uint32_t avg_match_length = total_match_length / match_count;
    /* Estimate the pointer length width based on average match length */
    if (avg_match_length >= 128) return 8;
    if (avg_match_length >= 64) return 7;
    if (avg_match_length >= 32) return 6;
    return 5;
}

/* Compresses input using lz algorithm */
uint32_t __lz_compress(struct databuffer *input, struct compressed *output) {
    uint16_t p_pos, temp_p_pos, out_ptr, p_len, temp_p_len;
    uint32_t comp_pos, code_pos, out_ref, look_b, look_a;
    uint16_t max_p_pos, max_p_len;

    /* Max pointer position */
    max_p_pos = 1 << (16 - output->ptr_len_width);
    /* Max pointer length */
    max_p_len = 1 << output->ptr_len_width;

    /*
     * Worst case is one 3-byte unit per input byte, plus the 5-byte header, since a
     * unit always writes a pointer and a literal and advances by at least one byte.
     */
    if (input->size > (0xFFFFFFFFu - 5) / 3) {
        return 0;
    }
    output->comp_data = malloc(input->size * 3 + 5);
    if (!output->comp_data){
        return 0;
    }
    
    *((uint32_t *) output->comp_data) = input->size;  /* Store original size */
    *(output->comp_data + 4) = output->ptr_len_width; /* Store pointer length width */
    comp_pos = output->comp_size = 5;

    /* Loop over the input data to compress it */
    for (code_pos = 0; code_pos < input->size; code_pos++) {
        p_pos = 0;
        p_len = 0;
        /* Find the longest match */
        for (temp_p_pos = 1; (temp_p_pos < max_p_pos) && (temp_p_pos <= code_pos); temp_p_pos++) {
            look_b = code_pos - temp_p_pos;
            look_a = code_pos;
            /* Stop at the end of the input, so the match can't read past it */
            for (temp_p_len = 0; look_a < input->size && temp_p_len < max_p_len && input->data[look_a] == input->data[look_b]; ++temp_p_len) {
                look_a++;
                look_b++;
            }
            if (temp_p_len > p_len) {
                p_pos = temp_p_pos;
                p_len = temp_p_len;
                if (p_len == max_p_len)
                    break;
            }
        }

        /* Check if the pointer length is maximum */
        code_pos += p_len;
        if ((code_pos == input->size) && p_len) {
            out_ptr = (p_len == 1) ? 0 : ((p_pos << output->ptr_len_width) | (p_len - 2));
            out_ref = code_pos - 1;
        } else {
            out_ptr = (p_pos << output->ptr_len_width) | (p_len ? (p_len - 1) : 0);
            out_ref = code_pos;
        }

        /* Store the compressed data */ 
        *((uint16_t *) (output->comp_data + comp_pos)) = out_ptr;
        comp_pos += 2;
        *(output->comp_data + comp_pos++) = *(input->data + out_ref);
        output->comp_size += 3;
    }

    return output->comp_size;
}

/* Decompresses input using lz algorithm */
uint32_t __lz_decompress(struct compressed *input, struct databuffer *output) {
    uint8_t ptr_len_width;
    uint16_t in_ptr, p_len, p_pos, len_mask;
    uint32_t comp_pos, code_pos, ptr_offset;

    /* The header is 4 bytes of original size and 1 byte of pointer width */
    if (input->comp_size < 5) {
        return 0;
    }

    /* Get original size */
    output->size = *((uint32_t *) input->comp_data);
    /* Get pointer length width */
    ptr_len_width = *(input->comp_data + 4);
    if (ptr_len_width < 1 || ptr_len_width > 8) {
        return 0;
    }
    comp_pos = 5;
    len_mask = (1 << ptr_len_width) - 1;

    /* One extra byte, so the result can be printed as a string */
    output->data = malloc(output->size + 1);
    if (!output->data) {
        return 0;
    }

    /* Loop to decompress */
    for (code_pos = 0; code_pos < output->size; ++code_pos) {
        /* A unit is a 2-byte pointer and a literal, so there must be three bytes left */
        if (comp_pos + 3 > input->comp_size) {
            free(output->data);
            output->data = NULL;
            return 0;
        }
        in_ptr = *((uint16_t *) (input->comp_data + comp_pos));
        comp_pos += 2;
        p_pos = in_ptr >> ptr_len_width;
        p_len = p_pos ? ((in_ptr & len_mask) + 1) : 0;
        if (p_pos) {
            /* The match has to be inside what's already written, and fit before the literal */
            if (p_pos > code_pos || p_len > output->size - code_pos) {
                free(output->data);
                output->data = NULL;
                return 0;
            }
            for (ptr_offset = code_pos - p_pos; p_len > 0; --p_len)
                output->data[code_pos++] = output->data[ptr_offset++];
        }
        /* There must be room for the literal */
        if (code_pos >= output->size) {
            free(output->data);
            output->data = NULL;
            return 0;
        }
        output->data[code_pos] = input->comp_data[comp_pos++];
    }

    output->data[output->size] = 0;

    return output->size;
}

/* API for compression */
uint32_t lz_compress(uint8_t *input, uint32_t input_size, uint8_t **output, int find_best) {
    struct databuffer input_buf = {input, input_size};
    struct compressed output_buf = {0};

    *output = NULL;

    if (find_best) {
        output_buf.ptr_len_width = estimate_best_ptr_len_width(&input_buf);
    } else {
        output_buf.ptr_len_width = 5; /* Default pointer length width */
    }

    uint32_t comp_size = __lz_compress(&input_buf, &output_buf);
    *output = output_buf.comp_data;

    return comp_size;
}

/* API for decompression */
uint32_t lz_decompress(uint8_t *input, uint32_t input_size, uint8_t **output) {
    struct compressed input_buf = {input, input_size, 0, 0};
    struct databuffer output_buf = {0};

    *output = NULL;

    uint32_t orig_size = __lz_decompress(&input_buf, &output_buf);
    *output = output_buf.data;

    return orig_size;
}