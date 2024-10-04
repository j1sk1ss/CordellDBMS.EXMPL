#include "../../include/dirman.h"


/*
 *  Directory destriptor table, is an a static array of directory indexes. Main idea in
 *  saving dirs temporary in static table somewhere in memory. Max size of this
 *  table equals (11 + 255 * 8) * 10 = 20.5Kb.
 *
 *  For working with table we have directory struct, that have index of table in DDT. If
 *  we access to dirs with full DRM_DDT, we also unload old dirs and load new dirs.
 *  (Stack)
 *
 *  Main problem in parallel work. If we have threads, they can try to access this
 *  table at one time. If you use OMP parallel libs, or something like this, please,
 *  define NO_DDT flag (For avoiding deadlocks), or use locks to directories.
 *
 *  Why we need DDT? - https://stackoverflow.com/questions/26250744/efficiency-of-fopen-fclose
*/
static directory_t* DRM_DDT[DDT_SIZE] = { NULL };


#pragma region [DDT]

    int DRM_DDT_add_directory(directory_t* directory) {
        #ifndef NO_DDT
            int current = 0;
            for (int i = 0; i < DDT_SIZE; i++) {
                if (DRM_DDT[i] == NULL) {
                    current = i;
                    break;
                }

                if (DRM_DDT[i]->lock == LOCKED) continue;
                current = i;
                break;
            }

            if (DRM_lock_directory(DRM_DDT[current], omp_get_thread_num()) != -1) {
                if (DRM_DDT[current] == NULL) return -1;
                if (memcmp(directory->header->name, DRM_DDT[current]->header->name, DIRECTORY_NAME_SIZE) != 0) {
                    DRM_DDT_flush_index(current);
                    DRM_DDT[current] = directory;
                }
            }
        #endif

        return 1;
    }

    directory_t* DRM_DDT_find_directory(char* name) {
        #ifndef NO_DDT
            if (name == NULL) return NULL;
            for (int i = 0; i < DDT_SIZE; i++) {
                if (DRM_DDT[i] == NULL) continue;
                if (strncmp((char*)DRM_DDT[i]->header->name, name, DIRECTORY_NAME_SIZE) == 0) {
                    return DRM_DDT[i];
                }
            }
        #endif

        return NULL;
    }

    int DRM_DDT_sync() {
        #ifndef NO_DDT
            for (int i = 0; i < DDT_SIZE; i++) {
                if (DRM_DDT[i] == NULL) continue;
                if (DRM_lock_directory(DRM_DDT[i], omp_get_thread_num()) == 1) {
                    DRM_DDT_flush_index(i);
                    DRM_DDT[i] = DRM_load_directory(NULL, (char*)DRM_DDT[i]->header->name);
                } else return -1;
            }
        #endif

        return 1;
    }

    int DRM_DDT_clear() {
        #ifndef NO_DDT
            for (int i = 0; i < DDT_SIZE; i++) {
                if (DRM_lock_directory(DRM_DDT[i], omp_get_thread_num()) == 1) {
                    DRM_DDT_flush_index(i);
                }
                else {
                    return -1;
                }
            }
        #endif

        return 1;
    }

    int DRM_DDT_flush_directory(directory_t* directory) {
        #ifndef NO_DDT
            if (directory == NULL) return -1;

            int index = -1;
            for (int i = 0; i < DDT_SIZE; i++) {
                if (DRM_DDT[i] == NULL) continue;
                if (directory == DRM_DDT[i]) {
                    index = i;
                    break;
                }
            }

            if (index != -1) DRM_DDT_flush_index(index);
            else DRM_free_directory(directory);
        #else
            DRM_free_directory(directory);
        #endif

        return 1;
    }

    int DRM_DDT_flush_index(int index) {
        #ifndef NO_DDT
            if (DRM_DDT[index] == NULL) return -1;
            DRM_save_directory(DRM_DDT[index], NULL);
            DRM_free_directory(DRM_DDT[index]);

            DRM_DDT[index] = NULL;
        #endif

        return 1;
    }

#pragma endregion