#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 1

typedef struct vector
{
    void *vector;
    u_int32_t capacity;
    u_int32_t length;
    u_int8_t element_size;
} Vector;

Vector* init_vector(u_int32_t element_size);
void add_elem_vector(Vector *vect, void *data);
void free_vector(Vector *vect);