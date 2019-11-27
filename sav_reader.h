#ifndef _SAV_READER_H
#define _SAV_READER_H

#include <stdbool.h>
#include "readstat.h"

struct Data* parse_sav(const char *input_file);
void cleanup(struct Data*);

struct Header {
    char *var_name;
    char *var_description;
    readstat_type_t var_type;
    size_t length;
    int precision;
    char *label_name;
};

struct Rows {
    char **row_data;
    int row_position;
    int row_length;
};

struct Labels {
    char *name;
    char *label;
    float       float_value;
    double      double_value;
    int8_t      i8_value;
    int16_t     i16_value;
    int32_t     i32_value;
    char *string_value;
    readstat_type_t var_type;
    int tag_missing;
    int system_missing;
    char tag;
};

struct Data {
    struct Header **header;
    unsigned long header_count;
    int header_position;

    struct Labels **labels;
    int labels_count;

    struct Rows **rows;
    unsigned long row_count;
    unsigned long rows_position;

    char *buffer;
    unsigned long buffer_size;

    int variable_count;
};


#endif
