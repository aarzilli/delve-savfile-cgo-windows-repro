#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "sav_reader.h"

const int ACCOC_SIZE = 256 * 1024 * 1024;
const int STRUCT_ACCOC_SIZE = 1024;
const int SAV_BUFFER_SIZE = 128;  // initial buffer size for a value, will grow if necessary

void add_new_row(struct Data *data)  {

    int row_size = sizeof(struct Rows);
    data->rows = realloc(data->rows, (data->row_count * row_size)  + row_size);

    struct Rows *row = malloc(sizeof(struct Rows));
    data->rows[data->row_count] = row;

    data->rows[data->row_count]->row_position = 0;
    data->rows[data->row_count]->row_length = 0;
    data->rows[data->row_count]->row_data = malloc(sizeof(char *) * data->header_count);
    data->row_count++;
}

void add_to_row(struct Data *data, const char *value) {

    struct Rows *current_row = data->rows[data->row_count - 1];
    int position = current_row->row_position;

    char *var = malloc(strlen(value) + 1);
    strcpy(var, value);

    current_row->row_data[position] = var;

    current_row->row_position++;
    current_row->row_length++;

    return;
}

int handle_metadata(readstat_metadata_t *metadata, void *ctx) {
    struct Data *data = (struct Data *) ctx;
    data->variable_count = readstat_get_var_count(metadata);
    return READSTAT_HANDLER_OK;
}

int handle_value_labels(const char *val_labels, readstat_value_t value, const char *label, void *ctx) {
    struct Data *data = (struct Data *) ctx;

    int labels_size = sizeof(struct Labels);
    data->labels = realloc(data->labels, (data->labels_count * labels_size)  + labels_size);

    struct Labels *label_struct = malloc(sizeof(struct Labels));
    data->labels[data->labels_count] = label_struct;

    label_struct->name = malloc(strlen(val_labels) + 1);
    strcpy(label_struct->name, val_labels);

    label_struct->label = malloc(strlen(label) + 1);
    strcpy(label_struct->label, label);

    label_struct->tag_missing = value.is_tagged_missing;
    label_struct->system_missing = value.is_system_missing;
    label_struct->tag = value.tag;
    label_struct->var_type = value.type;

    switch (value.type) {
        case READSTAT_TYPE_STRING:
        case READSTAT_TYPE_STRING_REF:
            if (value.v.string_value != NULL) {
                label_struct->string_value = malloc(strlen(value.v.string_value) + 1);
                strcpy(label_struct->string_value, value.v.string_value);
            } else {
               label_struct->string_value = NULL;
            }
            break;

        case READSTAT_TYPE_INT8:
            label_struct->i8_value = value.v.i8_value;
            break;

        case READSTAT_TYPE_INT16:
            label_struct->i16_value = value.v.i8_value;
            break;

        case READSTAT_TYPE_INT32:
            label_struct->i32_value = value.v.i32_value;
            break;

        case READSTAT_TYPE_FLOAT:
            label_struct->float_value = value.v.float_value;
            break;

        case READSTAT_TYPE_DOUBLE:
            label_struct->double_value = value.v.double_value;
            break;

    }

    data->labels_count++;

    return READSTAT_HANDLER_OK;
}

int handle_variable(int index, readstat_variable_t *variable, const char *val_labels, void *ctx) {
    struct Data *data = (struct Data *) ctx;
    const char *var_name = readstat_variable_get_name(variable);
    const char *var_description = readstat_variable_get_label(variable);

    int header_size = sizeof(struct Header);
    data->header = realloc(data->header, (data->header_count * header_size)  + header_size);

    struct Header *header = malloc(sizeof(struct Header));
    data->header[data->header_count] = header;

    unsigned long var_len = strlen(var_name) + 1;
    header->var_name = malloc(var_len);

    strcpy(header->var_name, var_name);

    if (var_description != NULL) {
        unsigned long desc_len = strlen(var_description) + 1;
        header->var_description = malloc(desc_len);
        strcpy(header->var_description, var_description);
    } else {
        header->var_description = NULL;
    }

    if (val_labels != NULL) {
        unsigned long len = strlen(val_labels) + 1;
        header->label_name = malloc(len);
        strcpy(header->label_name, val_labels);
    } else {
        header->label_name = NULL;
    }

    header->var_type = readstat_variable_get_type(variable);
    header->length = readstat_variable_get_storage_width(variable);
    header->precision = variable->decimals;

    data->header_count++;

    return READSTAT_HANDLER_OK;
}

int handle_value(int obs_index, readstat_variable_t *variable, readstat_value_t value, void *ctx) {

    struct Data *data = (struct Data *) ctx;
    int var_index = readstat_variable_get_index(variable);

    if (var_index == 0) {
        add_new_row(data);
    }

    readstat_type_t type = readstat_value_type(value);

    char *buf = data->buffer;

    switch (type) {
        case READSTAT_TYPE_STRING:

            // This will be the only place we can expect a value larger than the
            // existing SAV_BUFFER_SIZE
            // We use snprintf as it's much faster
            if (data->buffer_size <= strlen(readstat_string_value(value)) + 1) {
                data->buffer_size = strlen(readstat_string_value(value)) + SAV_BUFFER_SIZE + 1;
                data->buffer = realloc(data->buffer, data->buffer_size);
                buf = data->buffer;
            }
            char *str = (char *) readstat_string_value(value);
            for (char* p = str; (p = strchr(p, ',')) ; ++p) {
                *p = ' ';
            }
            snprintf(buf, data->buffer_size, "%s", readstat_string_value(value));
            add_to_row(data, buf);
            break;

        case READSTAT_TYPE_INT8:
            if (readstat_value_is_system_missing(value)) {
                snprintf(buf, data->buffer_size, "NaN");
            } else {
                snprintf(buf, data->buffer_size, "%d", readstat_int8_value(value));
            }
            add_to_row(data, buf);
            break;

        case READSTAT_TYPE_INT16:
            if (readstat_value_is_system_missing(value)) {
                snprintf(buf, data->buffer_size, "NaN");
            } else {
                snprintf(buf, data->buffer_size, "%d", readstat_int16_value(value));
            }
            add_to_row(data, buf);
            break;

        case READSTAT_TYPE_INT32:
            if (readstat_value_is_system_missing(value)) {
                snprintf(buf, data->buffer_size, "Nan");
            } else {
                snprintf(buf, data->buffer_size, "%d", readstat_int32_value(value));
            }
            add_to_row(data, buf);
            break;

        case READSTAT_TYPE_FLOAT:
            if (readstat_value_is_system_missing(value)) {
                snprintf(buf, data->buffer_size, "NaN");
            } else {
                snprintf(buf, data->buffer_size, "%f", readstat_float_value(value));
            }
            add_to_row(data, buf);

            break;

        case READSTAT_TYPE_DOUBLE:
            if (readstat_value_is_system_missing(value)) {
                snprintf(buf, data->buffer_size, "NaN");
            } else {
                snprintf(buf, data->buffer_size, "%lf", readstat_double_value(value));
            }
            add_to_row(data, buf);

            break;

        default:
            return READSTAT_HANDLER_OK;
    }

    return READSTAT_HANDLER_OK;
}

void cleanup(struct Data *data) {

   for (int i = 0; i < data->header_count; i++) {
        struct Header *header = data->header[i];
        if (header->var_name != NULL) free(header->var_name);
        if (header->var_description != NULL) free(header->var_description);
        if (header->label_name != NULL) free(header->label_name);
        free(header);
   }

   if (data->header != NULL) free(data->header);

    for (int i = 0; i < data->labels_count; i++) {
        struct Labels *l = data->labels[i];
        if (l->name != NULL) free(l->name);
        if (l->label != NULL) free(l->label);
        if (l->var_type == READSTAT_TYPE_STRING && l->string_value != NULL) free(l->string_value);
        free(l);
   }

   if (data->labels != NULL) free(data->labels);

   for (int i = 0; i < data->row_count; i++) {
       struct Rows *rows = data->rows[i];
       for (int j = 0; j < rows->row_length; j++) {
          if (rows->row_data[j] != 0) free(rows->row_data[j]);
       }
       free(rows);
   }

   if (data->rows != NULL) free(data->rows);
   if (data->buffer != NULL) free(data->buffer);

}

struct Data *parse_sav(const char *input_file) {

    if (input_file == 0) {
        return NULL;
    }

    readstat_error_t error;
    readstat_parser_t *parser = readstat_parser_init();
    readstat_set_metadata_handler(parser, &handle_metadata);
    readstat_set_variable_handler(parser, &handle_variable);
    readstat_set_value_handler(parser, &handle_value);
    readstat_set_value_label_handler(parser, &handle_value_labels);

    struct Data *sav_data = (struct Data *) malloc(sizeof(struct Data));
    sav_data->rows = NULL;
    sav_data->row_count = 0;

    sav_data->buffer = malloc(SAV_BUFFER_SIZE);
    sav_data->buffer_size = SAV_BUFFER_SIZE;

    sav_data->header = NULL;
    sav_data->header_count = 0;

    sav_data->labels = NULL;
    sav_data->labels_count = 0;

    error = readstat_parse_sav(parser, input_file, sav_data);

    readstat_parser_free(parser);

    if (error != READSTAT_OK) {
      return NULL;
    }

    return sav_data;

}
