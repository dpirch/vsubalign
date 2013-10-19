#ifndef LANGMODEL_H_
#define LANGMODEL_H_

#include "common.h"

struct dictword;
struct swlist;

struct lmbuilder;

struct lmbuilder *lmbuilder_create(void);
void lmbuilder_delete(struct lmbuilder *lmb);

void lmbuilder_addword(struct lmbuilder *lmb, const struct dictword *word);
void lmbuilder_break(struct lmbuilder *lmb);

void lmbuilder_add_subnodes(struct lmbuilder *lmb, const struct swlist *list);

void lmbuilder_compute_model(const struct lmbuilder *lmb, float discount);

bool lmbuilder_write_model(const struct lmbuilder *lmb, const char *filename);



#endif /* LANGMODEL_H_ */
