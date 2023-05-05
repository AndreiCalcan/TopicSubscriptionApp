#include "vector.h"

Vector* init_vector(u_int32_t element_size) {
    Vector *new_vect = calloc(1, sizeof(struct vector));
    DIE(new_vect == NULL, "vector alloc");
    new_vect->element_size = element_size;
    new_vect->capacity = INITIAL_CAP;
    new_vect->vector = malloc(INITIAL_CAP * element_size);
    DIE(new_vect->vector == NULL, "vector contents alloc");
    new_vect->length = 0;
    return new_vect;
}

void add_elem_vector(Vector *vect, void *data){
    if(vect->length == vect->capacity){
        (vect->capacity) *= 2;
        vect->vector = realloc(vect->vector, vect->capacity * vect->element_size);
        DIE(vect->vector == NULL, "realloc crapa");
    }
    memcpy(vect->vector + (vect->length * vect->element_size), data, vect->element_size);
    // if(vect->element_size == 1560){
    //     u_int8_t *buf = vect->vector + (vect->length * vect->element_size);
    //     for(int i = 0; i < 1560; i++){
    //         printf("%hhu ", buf[i]);
    //     }
    // }
    (vect->length)++;
}

void free_vector(Vector *vect){
    free(vect->vector);
    free(vect);
}