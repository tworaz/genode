/*
 * \brief  LibC test program used during the libC porting process
 * \author Norman Feske
 * \author Alexander Böttcher
 * \author Christian Helmuth
 * \date   2008-10-22
 */

/*
 * Copyright (C) 2008-2017 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU Affero General Public License version 3.
 */

/*
 * Mixing Genode headers and libC to see it they collide...
 */

/* Genode includes */
#include <base/env.h>

/* libC includes */
extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/random.h>
#include <sys/syscall.h>
#include <sys/limits.h>
#include <time.h>
#include <inttypes.h>
}

int main(int argc, char **argv)
{
	printf("--- libC test ---\n");

	printf("Does printf work?\n");
	printf("We can find out by printing a floating-point number: %f. How does that work?\n", 1.2345);

	fprintf(stdout, "stdout: ");
	for (int x = 0; x < 10; ++x)
		fprintf(stdout, "%d ", x);
	fprintf(stdout, "\n");

	fprintf(stderr, "stderr: ");
	for (int x = 0; x < 10; ++x)
		fprintf(stderr, "%d ", x);
	fprintf(stderr, "\n\n");

	enum { ROUNDS = 64, SIZE_LARGE = 2048 };

	unsigned error_count = 0;

	printf("Malloc: check small sizes\n");
	for (size_t size = 1; size < SIZE_LARGE; size = 2*size + 3) {
		void *addr[ROUNDS];
		for (unsigned i = 0; i < ROUNDS; ++i) {
			addr[i] = malloc(size);

			if (!((unsigned long)addr[i] & 0xf))
				continue;

			printf("%u. malloc(%zu) returned addr = %p - ERROR\n", i, size, addr[i]);
			++error_count;
		}
		for (unsigned i = 0; i < ROUNDS; ++i) free(addr[i]);
	}

	printf("Malloc: check larger sizes\n");
	for (size_t size = SIZE_LARGE; size < 1024*1024; size = 2*size + 15) {
		void *addr[ROUNDS];
		for (unsigned i = 0; i < ROUNDS; ++i) {
			addr[i] = malloc(size);

			if (!((unsigned long)addr[i] & 0xf))
				continue;

			printf("%u. malloc(%zu) returned addr = %p - ERROR\n", i, size, addr[i]);
			++error_count;
		}
		for (unsigned i = 0; i < ROUNDS; ++i) free(addr[i]);
	}

	printf("Malloc: check realloc\n");
	{
		void *addr = malloc(32);
		memset(addr, 13, 32);

		for (unsigned i = 0; i < ROUNDS; ++i) {
			size_t const size = 32 + 11*i;
			char *a = (char *)realloc(addr, size);
			if (memcmp(addr, a, 32) || a[32] != 0) {
				printf("realloc data error");
				++error_count;
			}
			addr = a;

			if (!((unsigned long)addr & 0xf))
				continue;

			printf("%u. realloc(%zu) returned addr = %p - ERROR\n", i, size, addr);
			++error_count;
		}
		for (int i = ROUNDS - 1; i >= 0; --i) {
			size_t const size = 32 + 11*i;
			char *a = (char *)realloc(addr, size);
			if (memcmp(addr, a, 32) || a[32] != 0) {
				printf("realloc data error");
				++error_count;
			}
			addr = a;

			if (!((unsigned long)addr & 0xf))
				continue;

			printf("%u. realloc(%zu) returned addr = %p - ERROR\n", i, size, addr);
			++error_count;
		}
	}

	printf("Malloc: check really large allocation\n");
	for (unsigned i = 0; i < 4; ++i) {
		size_t const size = 250*1024*1024;
		void *addr = malloc(size);
		free(addr);

		if ((unsigned long)addr & 0xf) {
			printf("large malloc(%zu) returned addr = %p - ERROR\n", size, addr);
			++error_count;
		}
	}

	{
		pid_t const tid = syscall(SYS_thr_self);
		if (tid == -1) {
			printf("syscall(SYS_thr_self) returned %d (%s) - ERROR\n", tid, strerror(errno));
			++error_count;
		} else {
			printf("syscall(SYS_thr_self) returned %d\n", tid);
		}
		int const ret = syscall(0xffff);
		if (ret != -1) {
			printf("syscall(unknown) returned %d - ERROR\n", ret);
			++error_count;
		} else {
			printf("syscall(unknown) returned %d (%s)\n", ret, strerror(errno));
		}
	}

	perror("perror");

	struct timespec ts;
	for (int i = 0; i < 3; ++i) {
		sleep(1);
		ts.tv_sec = ts.tv_nsec = 0;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		printf("sleep/gettime(CLOCK_MONOTONIC): %.09f\n", ts.tv_sec + ts.tv_nsec / 1000000000.0);
	}

	ts.tv_sec = ts.tv_nsec = 0;
	clock_gettime(CLOCK_REALTIME, &ts);
	printf("sleep/gettime(CLOCK_REALTIME): %.09f\n", ts.tv_sec + ts.tv_nsec / 1000000000.0);

	{
		unsigned long long buf = 0;
		getrandom(&buf, sizeof(buf), 0);
		printf("getrandom %llx\n", buf);
	}

	{
		unsigned long long buf = 0;
		getentropy(&buf, sizeof(buf));
		printf("getentropy %llx\n", buf);
	}

	do {
		struct tm tm { };
		/* 2019-05-27 12:30 */
		tm.tm_sec  = 0;
		tm.tm_min  = 30;
		tm.tm_hour = 12;
		tm.tm_mday = 27;
		tm.tm_mon  = 4;
		tm.tm_year = 119;

		time_t t1 = mktime(&tm);
		if (t1 == (time_t) -1) {
			++error_count;
			long long v1 = t1;
			printf("Check mktime failed: %lld\n", v1);
			break;
		}
		struct tm *tm_gmt = gmtime(&t1);
		time_t t2 = mktime(tm_gmt);
		if (t1 != t2) {
			++error_count;
			long long v1 = t1, v2 = t2;
			printf("Check mktime failed: %lld != %lld\n", v1, v2);
			break;
		}

		puts("Check mktime: success");
	} while (0);

	exit(error_count);
}
