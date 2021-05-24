#include <gtest/gtest.h>

static struct init_rand {
	init_rand()
	{
		srand(time(NULL));
	}
} init;

