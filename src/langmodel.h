#ifndef LANGMODEL_H_
#define LANGMODEL_H_

#include "common.h"

struct dict;
struct dictword;
struct subnodelist;

typedef struct lmbuilder_t lmbuilder_t;

lmbuilder_t *lmbuilder_create(struct dict *dict);
void lmbuilder_delete(lmbuilder_t *lmb);

void lmbuilder_addword(lmbuilder_t *lmb, const struct dictword *word);
void lmbuilder_break(lmbuilder_t *lmb);

void lmbuilder_add_subnodes(lmbuilder_t *lmb, const struct subnodelist *list);

void lmbuilder_compute_model(const lmbuilder_t *lmb, float discount);

bool lmbuilder_write_model(const lmbuilder_t *lmb, const char *filename);



#endif /* LANGMODEL_H_ */
