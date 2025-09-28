#include <assert.h>

#include "tape.h"

/*
 * Pretty hacky method of comparing two tapes. Because we don't (yet) support random access,
 * we simply move along the tape for a few units in each direction and compare them, making
 * sure to move back to the original position after we're done.
 * NOTE: this may cause us to allocate more memory than the TM run actually would use.
 * Because this is just used for testing it *should* be fine, hopefully.
 */
int tape_cmp(struct tape_t *t1, struct tape_t *t2, int window)
{
	assert(window >= 0);

	int differ = 0;

	// Compare starting position
	if (t1->read(t1) != t2->read(t2))
		differ = 1;

	// Move to the right until we find a difference or reach the end of out window
	int i = 0;
	while (i < window && !differ) {
		t1->move(t1, 1);
		t2->move(t2, 1);
		i++;
		if (t1->read(t1) != t2->read(t2))
			differ = 1;
	}
	// Move back the number of units that we actually moved (may be zero)
	while (i-- > 0) {
		t1->move(t1, -1);
		t2->move(t2, -1);
	}

	// Move to the left
	i = 0;
	while (i < window && !differ) {
		t1->move(t1, -1);
		t2->move(t2, -1);
		i++;
		if (t1->read(t1) != t2->read(t2))
			differ = 1;
	}
	// Move back the correct number of units
	while (i-- > 0) {
		t1->move(t1, 1);
		t2->move(t2, 1);
	}

	return differ;
}
