#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"
#include "khash.h"
//#include "bloom.h"

//#include <bzlib.h>


#define QUERY_FIELD_QID 0
#define QUERY_FIELD_A1 1
#define QUERY_FIELD_A2 2
#define QUERY_FIELD_A3 3
#define QUERY_FIELD_A4 4
#define QUERY_FIELD_BS 5
#define QUERY_FIELD_BE 6

Person *person_map;
unsigned int *knows_map;
unsigned short *interest_map;

unsigned long person_length, knows_length, interest_length;

FILE *outfile;

int result_comparator(const void *v1, const void *v2) {
    Result *r1 = (Result *) v1;
    Result *r2 = (Result *) v2;
    if (r1->score > r2->score)
        return -1;
    else if (r1->score < r2->score)
        return +1;
    else if (r1->person_id < r2->person_id)
        return -1;
    else if (r1->person_id > r2->person_id)
        return +1;
     else if (r1->knows_id < r2->knows_id)
        return -1;
    else if (r1->knows_id > r2->knows_id)
        return +1;
    else
        return 0;
}

KHASH_MAP_INIT_INT(pht, char)
KHASH_MAP_INIT_INT(ft, unsigned long)


void query(unsigned short qid, unsigned short artist, unsigned short areltd[], unsigned short bdstart, unsigned short bdend) {
	unsigned int person_offset;
	unsigned long knows_offset, knows_offset2, interest_offset;

	Person *person, *knows;
	unsigned char score;
	bool likesa1 = false;
	unsigned short interest;

	khash_t(pht) *birthdayboys =  kh_init(pht);
	khash_t(ft) *a1likers =  kh_init(ft);

	khiter_t k;
	int kret;

	unsigned int result_length = 0, result_idx, result_set_size = 10000;
	Result* results = malloc(result_set_size * sizeof (Result));
	printf("Running query %d\n", qid);
	// bloom filters were way slower than probing hash table!	
	// struct bloom birthdayboys_filter;
	// struct bloom a1likers_filter;

	// bloom_init(&birthdayboys_filter, 100000, 0.1);
	// bloom_init(&a1likers_filter, 10000, 0.1);

	// TODO: store person as arrays?

	// first pass, find candidates

	// TODO: clean this up
//	FILE * personfile = fopen("dataset-sf100/person-local.bin", "r");
//	int bzerror;
//	BZFILE* personfile = BZ2_bzReadOpen(&bzerror, personfilebz ,0, 0, NULL, 0);
	//int chunksize = 10000;
	//Person * personbuf = (Person *) malloc(chunksize * sizeof(Person));
	//int buffset;

	//int bread;
	//person_offset = 0;

	// while((bread = BZ2_bzRead(&bzerror, personfile, personbuf, chunksize)) > 0) {
	// 	for (buffofset = 0; buffofset < bread/sizeof(Person); buffofset++) {
	// 		person = &personbuf[buffofset];
			//printf("%lu\n", person->person_id);

	//while ((bread = fread(personbuf, sizeof(Person), chunksize, personfile)) > 0) {
//		for (buffset = 0; buffset < bread; buffset++) {
//			person = &personbuf[buffset];
	// mmap way
	for (person_offset = 0; person_offset < person_length/sizeof(Person); person_offset++) {
	 	person = &person_map[person_offset];

		score = 0;
		likesa1 = false;

		// TODO: also use fread to scan interests?
		for (interest_offset = person->interests_first; 
			interest_offset < person->interests_first + person->interest_n; 
			interest_offset++) {
			
			interest = interest_map[interest_offset];
			if (interest == artist) {
				likesa1 = true;
				break; // for people who like a1, the score is irrelevant
			}
			score += (interest == areltd[0]) + (interest == areltd[1]) + (interest == areltd[2]);
		}
		
		if (!likesa1 && score > 0 && person->birthday >= bdstart && person->birthday <= bdend) {
			k = kh_put(pht, birthdayboys, person_offset, &kret);
			kh_value(birthdayboys, k) = score;
			//bloom_add(&birthdayboys_filter, &person_offset, sizeof(unsigned int));
			//			printf("b:%lu\n", person->person_id);

			//continue;
		}

		if (likesa1) {
			k = kh_put(ft, a1likers, person_offset, &kret);
			kh_value(a1likers, k) = person->person_id;
			//							printf("a:%lu\n", person->person_id);

			//bloom_add(&a1likers_filter, &person_offset, sizeof(unsigned int));
		}
		//person_offset++;
	}
	//rewind(personfile);
	//person_offset = 0;

	// TODO: build bloom filters from hash tables
	// second pass, probe hash table and again hash table
	
	// while ((bread = fread(personbuf, sizeof(Person), chunksize, personfile)) > 0) {
	// 	for (buffset = 0; buffset < bread; buffset++) {
	// 		person = &personbuf[buffset];
	// 		//printf("%lu\n", person->person_id);

	bool mutual;

	for (person_offset = 0; person_offset < person_length/sizeof(Person); person_offset++) {
		person = &person_map[person_offset];

		// TODO: bloom filter here?

		// if (!bloom_check(&birthdayboys_filter, &person_offset, sizeof(unsigned int))) {
		// 	continue;
		// }

		k = kh_get(pht, birthdayboys, person_offset);
		//person_offset++;

		if (k == kh_end(birthdayboys)) {
			continue;
		}
		//person = &person_map[person_offset];
		score = kh_value(birthdayboys, k);
			//						printf("b:%lu\n", person->person_id);

		// TODO: also use fread to scan knows?

		// check if friend lives in same city and likes artist 
		for (knows_offset = person->knows_first; 
			knows_offset < person->knows_first + person->knows_n; 
			knows_offset++) {

			// avoid looking at interests
			// TODO: bloom filter here?
			k = kh_get(ft, a1likers, knows_map[knows_offset]);
			if (k == kh_end(a1likers)) {
				continue;
			}

			mutual = false;
			// only mutual friendships matter
			// TODO: do this on the smaller file?
			knows = &person_map[knows_map[knows_offset]];
			for (knows_offset2 = knows->knows_first;
				knows_offset2 < knows->knows_first + knows->knows_n;
				knows_offset2++) {
			
				if (knows_map[knows_offset2] == person_offset) {
					mutual = true;
					break;
				}
			}
			if (!mutual) continue;

			// realloc result array if we run out of space
			if (result_length >= result_set_size) {
				result_set_size *= 2;
				results = realloc(results, result_set_size * sizeof (Result));
			}
			// do not look up friend information from person, would be random access
			results[result_length].person_id = person->person_id;
			results[result_length].knows_id = kh_value(a1likers, k);;
			results[result_length].score = score;
			result_length++;
		}
	}

	// sort result
	qsort(results, result_length, sizeof(Result), &result_comparator);

	// output
	for (result_idx = 0; result_idx < result_length; result_idx++) {
		fprintf(outfile, "%d|%d|%lu|%lu\n", qid, results[result_idx].score, 
			results[result_idx].person_id, results[result_idx].knows_id);
	}
}

void query_line_handler(unsigned char nfields, char** tokens) {
	unsigned short q_id, q_artist, q_bdaystart, q_bdayend;
	unsigned short q_relartists[3];

	q_id            = atoi(tokens[QUERY_FIELD_QID]);
	q_artist        = atoi(tokens[QUERY_FIELD_A1]);
	q_relartists[0] = atoi(tokens[QUERY_FIELD_A2]);
	q_relartists[1] = atoi(tokens[QUERY_FIELD_A3]);
	q_relartists[2] = atoi(tokens[QUERY_FIELD_A4]);
	q_bdaystart     = birthday_to_short(tokens[QUERY_FIELD_BS]);
	q_bdayend       = birthday_to_short(tokens[QUERY_FIELD_BE]);
	
	query(q_id, q_artist, q_relartists, q_bdaystart, q_bdayend);
}

int main(int argc, char *argv[]) {
	if (argc < 4) {
		fprintf(stderr, "Usage: [datadir] [query file] [results file]\n");
		exit(1);
	}
	/* memory-map files created by loader */
	person_map   = (Person *)         mmapr(makepath(argv[1], "person-local",   "bin"), &person_length);
	interest_map = (unsigned short *) mmapr(makepath(argv[1], "interest-local", "bin"), &interest_length);
	knows_map    = (unsigned int *)   mmapr(makepath(argv[1], "knows-local",    "bin"), &knows_length);

  	outfile = fopen(argv[3], "w");  
  	if (outfile == NULL) {
  		fprintf(stderr, "Can't write to output file at %s\n", argv[3]);
		exit(-1);
  	}

  	/* run through queries */
	parse_csv(argv[2], &query_line_handler);
	return 0;
}
