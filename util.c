/*
 * The maximum of two integers, used for clamping values etc.
 */
int maximum(const int a, const int b)
{
	return a > b ? a : b;
}

/*
 * Calculates how many bits are needed to fit a value, e.g. floor_log2(7) = 3.
 * Mathematically, this is equivalent to floor(log2(n)) or
 * the number m such that n >> (m + 1) == 0.
 */
unsigned floor_log2(const int n)
{
	assert(n >= 0); // Only valid for non-negative integers
	unsigned shift = 0;
	while ((unsigned) n >> (shift + 1) > 0)
		shift++;
	return shift;
}
