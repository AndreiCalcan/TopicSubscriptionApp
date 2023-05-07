#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define INITIAL_CAP 1

#define DIE(assertion, call_description)                                       \
  do {                                                                         \
    if (assertion) {                                                           \
      fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                       \
      perror(call_description);                                                \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

typedef struct vector
{
    void *vector;
    u_int32_t capacity;
    int length;
    u_int32_t element_size;
} Vector;

Vector* init_vector(u_int32_t element_size);
void add_elem_vector(Vector *vect, void *data);
void free_vector(Vector *vect);