#include "../../include/pageman.h"


/*
 *  Page destriptor table, is an a static array of pages indexes. Main idea in
 *  saving pages temporary in static table somewhere in memory. Max size of this
 *  table equals 1024 * 10 = 10Kb.
 *
 *  For working with table we have PAGE struct, that have index of table in PDT. If
 *  we access to pages with full PGM_PDT, we also unload old page and load new page.
 *  (Stack)
 *
 *  Main problem in parallel work. If we have threads, they can try to access this
 *  table at one time. If you use OMP parallel libs, or something like this, please,
 *  define NO_PDT flag (For avoiding deadlocks), or use locks to pages.
 *
 *  Why we need PDT? - https://stackoverflow.com/questions/26250744/efficiency-of-fopen-fclose
*/
static page_t* PGM_PDT[PDT_SIZE] = { NULL };


#pragma region [PDT]

    int PGM_PDT_add_page(page_t* page) {
        #ifndef NO_PDT
            int current = 0;
            for (int i = 0; i < PDT_SIZE; i++) {
                if (PGM_PDT[i] == NULL) {
                    current = i;
                    break;
                }

                if (PGM_PDT[i]->lock == LOCKED) continue;
                current = i;
                break;
            }

            if (PGM_lock_page(PGM_PDT[current], omp_get_thread_num()) != -1) {
                if (PGM_PDT[current] == NULL) return -1;
                if (memcmp(page->header->name, PGM_PDT[current]->header->name, PAGE_NAME_SIZE) != 0) {
                    PGM_PDT_flush_index(current);
                    PGM_PDT[current] = page;
                }
            }
        #endif

        return 1;
    }

    page_t* PGM_PDT_find_page(char* name) {
        #ifndef NO_PDT
            for (int i = 0; i < PDT_SIZE; i++) {
                if (PGM_PDT[i] == NULL) continue;
                if (strncmp((char*)PGM_PDT[i]->header->name, name, PAGE_NAME_SIZE) == 0) {
                    return PGM_PDT[i];
                }
            }
        #endif

        return NULL;
    }

    int PGM_PDT_sync() {
        #ifndef NO_PDT
            for (int i = 0; i < PDT_SIZE; i++) {
                if (PGM_PDT[i] == NULL) continue;
                if (PGM_lock_page(PGM_PDT[i], omp_get_thread_num()) == 1) {
                    PGM_PDT_flush_index(i);
                    PGM_PDT[i] = PGM_load_page(NULL, (char*)PGM_PDT[i]->header->name);
                } else return -1;
            }
        #endif

        return 1;
    }

    int PGM_PDT_clear() {
        #ifndef NO_PDT
            for (int i = 0; i < PDT_SIZE; i++) {
                if (PGM_lock_page(PGM_PDT[i], omp_get_thread_num()) == 1) {
                    PGM_PDT_flush_index(i);
                }
                else {
                    return -1;
                }
            }
        #endif

        return 1;
    }

    int PGM_PDT_flush_page(page_t* page) {
        #ifndef NO_PDT
            if (page == NULL) return -1;

            int index = -1;
            for (int i = 0; i < PDT_SIZE; i++) {
                if (PGM_PDT[i] == NULL) continue;
                if (page == PGM_PDT[i]) {
                    index = i;
                    break;
                }
            }

            if (index != -1) PGM_PDT_flush_index(index);
            else PGM_free_page(page);
        #else
            PGM_free_page(page);
        #endif

        return 1;
    }

    int PGM_PDT_flush_index(int index) {
        #ifndef NO_PDT
            if (PGM_PDT[index] == NULL) return -1;

            PGM_save_page(PGM_PDT[index], NULL);
            PGM_free_page(PGM_PDT[index]);

            PGM_PDT[index] = NULL;
        #endif

        return 1;
    }

#pragma endregion