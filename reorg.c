#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "utils.h"
#include "khash.h"

//#include "pfordelta.h"
//#include "coding_policy.h"


KHASH_MAP_INIT_INT(pht, unsigned int)

#define min(a, b) (((a) < (b)) ? (a) : (b))


int main(int argc, char *argv[]) {
	unsigned long person_length, knows_length;

	if (argc < 2) {
		fprintf(stderr, "Usage: [binpath]\n");
		exit(-1);
	}

	char* pl = makepath(argv[1], "person-local",    "bin");
	Person *person_old_map       = (Person *)         mmapr(makepath(argv[1], "person",   "bin"), &person_length);
	FILE* person_out = open_binout(pl);

	unsigned int *knows_map  = (unsigned int *)   mmapr(makepath(argv[1], "knows",    "bin"), &knows_length);
	FILE* knows_out = open_binout(makepath(argv[1], "knows-local-temp",    "bin"));

	Person *person, *knows;
	unsigned int person_offset, person_new_offset = 0;
	unsigned long knows_offset, knows_offset2;
	unsigned long new_knows_offset = 0;
	unsigned short new_knows_n;
	unsigned int new_knows_first;

	bool mutual;

	khash_t(pht) *newoffsets =  kh_init(pht);
	khiter_t k;
	int kret;

	printf("filtering for only local and mutual knows\n");

	// discard people in different city
	for (person_offset = 0; person_offset < person_length/sizeof(Person); person_offset++) {
		person = &person_old_map[person_offset];
		Person * nperson = malloc(sizeof(Person));
		new_knows_n = 0;
		new_knows_first = new_knows_offset;

		if (person_offset > 0 && person_offset % REPORTING_N == 0) {
			printf("%.2f%%\n", 100 * (person_offset * 1.0/(person_length/sizeof(Person))));
		}

		// only friends in same town matter
		for (knows_offset = person->knows_first; 
			knows_offset < person->knows_first + person->knows_n; 
			knows_offset++) {

			knows = &person_old_map[knows_map[knows_offset]];
			if (person->location != knows->location) continue; 
			/*
			mutual = false;
			// only mutual friendships matter
			// TODO: do this on the smaller file?
			for (knows_offset2 = knows->knows_first;
				knows_offset2 < knows->knows_first + knows->knows_n;
				knows_offset2++) {
			
				if (knows_map[knows_offset2] == person_offset) {
					mutual = true;
					break;
				}
			}
			if (!mutual) continue;
	*/
			fwrite(&knows_map[knows_offset], sizeof(unsigned int), 1, knows_out);
			new_knows_offset++;
			new_knows_n++;
		}

		if (new_knows_n < 1) {
			continue;
		}

		// set knows_first and knows_n to new values
		memcpy(nperson, person, sizeof(Person));
		nperson->knows_n = new_knows_n;
		nperson->knows_first = new_knows_first;

		fwrite(nperson, sizeof(Person), 1, person_out);
		k = kh_put(pht, newoffsets, person_offset, &kret);
		kh_value(newoffsets, k) = person_new_offset;
		person_new_offset++;
	}

	fclose(knows_out);
	fclose(person_out);

	printf("rewrite interests\n");

	// rewrite interest because less people, update person to match new offset
	unsigned long interest_offset, interest_length;
	FILE* interest_out = open_binout(makepath(argv[1], "interest-local",    "bin"));
	Person * person_map  = (Person *) mmaprw(pl, &person_length);
	unsigned short * interest_map  = (unsigned short *) mmapr(makepath(argv[1], "interest",    "bin"), &interest_length);
	unsigned short interest;
	unsigned long new_interest_first; 
	unsigned long new_interest_offset = 0;
	unsigned short new_interest_n;

	for (person_offset = 0; person_offset < person_length/sizeof(Person); person_offset++) {
		person = &person_map[person_offset];
		new_interest_first = new_interest_offset;

		if (person_offset > 0 && person_offset % REPORTING_N == 0) {
			printf("%.2f%%\n", 100 * (person_offset * 1.0/(person_length/sizeof(Person))));
		}

		for (interest_offset = person->interests_first; 
				interest_offset < person->interests_first + person->interest_n; 
				interest_offset++) {
				
			interest = (unsigned short) interest_map[interest_offset];
		// TODO: directly compress those here, set in a buffer, write this out when full and reset
		// much less io?

			fwrite(&interest, sizeof(unsigned short), 1, interest_out);
			new_interest_offset++;
		}
		person->interests_first = new_interest_first;
	}
	fclose(interest_out);
	munmap(person_map, person_length); 

	printf("rewrite knows for new offsets and filter non-mutual knows \n");

	// rewrite knows because offsets changed
	knows_map  = (unsigned int *) mmapr(makepath(argv[1], "knows-local-temp",    "bin"), &knows_length);
	knows_out = open_binout(makepath(argv[1], "knows-local",    "bin"));
	for (knows_offset = 0; knows_offset < knows_length/sizeof(unsigned int); knows_offset++) {
		// TODO: check mutual friendships here? probably...

		unsigned int otherpersonoffset = kh_value(newoffsets, kh_get(pht, newoffsets, knows_map[knows_offset]));


		//Person * knows = person_map[otherpersonoffset];
		if (knows_offset > 0 && knows_offset % REPORTING_N == 0) {
			printf("%.2f%%\n", 100 * (knows_offset * 1.0/(knows_length/sizeof(unsigned int))));
		}
		fwrite(&otherpersonoffset, sizeof(unsigned int), 1, knows_out);
		// TODO: Directly compress these?, buffer etc.
	}
	fclose(knows_out);


/*

	printf("compress knows\n");

	int chunksize = 2048000;
	void *knows_pfor_buf = malloc(sizeof(unsigned int) * chunksize);
	int newsize;
	knows_map  = (unsigned int *) mmapr(makepath(argv[1], "knows-local",    "bin"), &knows_length);

	int blocksize = 128;
	int fi = 0;
	char * fname = malloc(100);

	for (knows_offset = 0; knows_offset < knows_length/sizeof(unsigned int); knows_offset += chunksize) {
		// TODO: we have fewer items than chunksize in last iteration?
		// TODO: check impact of last param
		int nitems = chunksize;
		//if (knows_offset + chunksize > knows_length/sizeof(unsigned int)) {
	//		nitems = knows_length/sizeof(unsigned int) - knows_offset;
	//	}
//		printf("%d\n", nitems);

		//printf("%lu/%lu\n", knows_offset, knows_length/sizeof(unsigned int));
		unsigned int * block = &knows_map[knows_offset];

		// special treatment for last block due to braindead implementation
		if (knows_offset + chunksize > knows_length/sizeof(unsigned int)) {
			nitems = knows_length/sizeof(unsigned int) - knows_offset;
						printf("%d last\n", nitems);
			int padded_size = blocksize * ((nitems/blocksize)+1);
			block = malloc(padded_size);
			memcpy(block, &knows_map[knows_offset], nitems*sizeof(unsigned int));
			int i;
			for (i = nitems*sizeof(unsigned int); i< padded_size;i++) {
				block[i]= 0;
			}
			nitems = padded_size;
		} 

		printf("file %d,  nitems=%d\n", fi, nitems);
		newsize = compress_pfordelta(block, knows_pfor_buf, nitems, blocksize);

		// now knows_pfor_buf contains newsize bytes that we write to the file
		sprintf(fname, "knows-local-%d", fi);
		FILE* knows_pfor_out = open_binout(makepath(argv[1], fname, "pfor"));

		fwrite(knows_pfor_buf, newsize*4, 1, knows_pfor_out);
		fclose(knows_pfor_out);
		fi++;
	}
*/

	return 0;
}

