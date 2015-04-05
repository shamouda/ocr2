#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "ocr.h"

#define GAP -1
#define TRANSITION -2
#define TRANSVERSION -4
#define MATCH 2

#define FLAGS 0xdead
#define PROPERTIES 0xdead

enum Nucleotide {N_GAP=0, ADENINE, CYTOSINE, GUANINE, THYMINE};

static char alignment_score_matrix[5][5] = {
    {GAP,   GAP,            GAP,            GAP,            GAP},
    {GAP,   MATCH,          TRANSVERSION,   TRANSITION,     TRANSVERSION},
    {GAP,   TRANSVERSION,   MATCH,          TRANSVERSION,   TRANSITION},
    {GAP,   TRANSITION,     TRANSVERSION,   MATCH,          TRANSVERSION},
    {GAP,   TRANSVERSION,   TRANSITION,     TRANSVERSION,   MATCH}
};

typedef struct {
    ocrGuid_t bottom_row_event_guid;
    ocrGuid_t right_column_event_guid;
    ocrGuid_t bottom_right_event_guid;

    intptr_t *paramv;
} Tile_t;

#define TILE_INDEX_2D(tile,x,y) ( tile[ ((x)*(1+tile_width)) + (y) ] )
u8 smith_waterman_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    int index, ii, jj;

    /* Unbox parameters */
    intptr_t* typed_paramv  = (intptr_t* ) paramv;
    int i                   = (int) typed_paramv[0];
    int j                   = (int) typed_paramv[1];
    int tile_width          = (int) typed_paramv[2];
    int tile_height         = (int) typed_paramv[3];
    Tile_t** tile_matrix    = (Tile_t**) typed_paramv[4];
    char* string_1          = (char* ) typed_paramv[5];
    char* string_2          = (char* ) typed_paramv[6];
    int n_tiles_height      = (int) typed_paramv[7];
    int n_tiles_width       = (int)typed_paramv[8];

    /* Get the input datablock data pointers acquired from dependences */
    int* left_tile_right_column     = (int *) depv[0].ptr;
    int* above_tile_bottom_row      = (int *) depv[1].ptr;
    int* diagonal_tile_bottom_right = (int *) depv[2].ptr;

    /* Allocate a haloed local matrix for calculating 'this' tile*/
    int  * curr_tile = (int*)malloc(sizeof(int)*(1+tile_width)*(1+tile_height));
    assert(curr_tile);

    /* Initialize halo from neighbouring tiles */
    /* Set local_tile[0][0] (top left) from the bottom right of the northwest tile */
    TILE_INDEX_2D(curr_tile,0,0) = diagonal_tile_bottom_right[0];

    /* Set local_tile[i+1][0] (left column) from the right column of the left tile */
    for ( index = 1; index < tile_height+1; ++index ) {
        TILE_INDEX_2D(curr_tile,index,0) = left_tile_right_column[index-1];
    }

    /* Set local_tile[0][j+1] (top row) from the bottom row of the above tile */
    for ( index = 1; index < tile_width+1; ++index ) {
        TILE_INDEX_2D(curr_tile,0,index) = above_tile_bottom_row[index-1];
    }

    /* Run a smith-waterman on the local tile */
    for ( ii = 1; ii < tile_height+1; ++ii ) {
        for ( jj = 1; jj < tile_width+1; ++jj ) {
            char char_from_1 = string_1[(j-1)*tile_width+(jj-1)];
            char char_from_2 = string_2[(i-1)*tile_height+(ii-1)];

            /* Get score from northwest, north and west */
            int diag_score = TILE_INDEX_2D(curr_tile,ii-1,jj-1) + alignment_score_matrix[char_from_2][char_from_1];
            int left_score = TILE_INDEX_2D(curr_tile,ii  ,jj-1) + alignment_score_matrix[char_from_1][N_GAP];
            int  top_score = TILE_INDEX_2D(curr_tile,ii-1,jj  ) + alignment_score_matrix[N_GAP][char_from_2];

            int bigger_of_left_top = (left_score > top_score) ? left_score : top_score;

            /* Set the local tile[i][j] to the maximum value of northwest, north and west */
            TILE_INDEX_2D(curr_tile,ii,jj) = (bigger_of_left_top > diag_score) ? bigger_of_left_top : diag_score;
        }
    }

    /* Satisfy the bottom right event of local tile with the data block allocated above */
    int* curr_bottom_right = diagonal_tile_bottom_right;
    curr_bottom_right[0] = TILE_INDEX_2D(curr_tile,tile_height,tile_width);
    ocrEventSatisfy(tile_matrix[i][j].bottom_right_event_guid, depv[2].guid);

    /* Satisfy the right column event of local tile with the data block allocated above */
    int* curr_right_column = left_tile_right_column;
    for ( index = 0; index < tile_height; ++index ) {
        curr_right_column[index] = TILE_INDEX_2D(curr_tile,index+1,tile_width);
    }
    ocrEventSatisfy(tile_matrix[i][j].right_column_event_guid, depv[0].guid);

    /* Satisfy the bottom row event of local tile with the data block allocated above */
    int* curr_bottom_row = above_tile_bottom_row;
    for ( index = 0; index < tile_width; ++index ) {
        curr_bottom_row[index] = TILE_INDEX_2D(curr_tile,tile_height,index+1);
    }
    ocrEventSatisfy(tile_matrix[i][j].bottom_row_event_guid, depv[1].guid);

    free(curr_tile);
    //free(paramv);
    /* If this is the last tile (bottom right most tile), finish */
    if ( i == n_tiles_height && j == n_tiles_width ) {
        fprintf(stderr, "score: %d\n", curr_bottom_row[tile_width-1]);
        ocrFinish();
    }
}

static void initialize_border_values( Tile_t** tile_matrix, int n_tiles_width, int n_tiles_height, int tile_width, int tile_height ) {
    int i, j;
    /* Create a datablock for the bottom right element for tile[0][0] */
    void* db_guid_0_0_br_data;
    ocrGuid_t temp;
    ocrDbCreate( &temp, &db_guid_0_0_br_data, sizeof(int), FLAGS, NULL, NO_ALLOC );
    /* Satisfy the bottom right event for tile[0][0] with the respective datablock */
    assert(db_guid_0_0_br_data);
    int* allocated = (int*)db_guid_0_0_br_data;
    allocated[0] = 0;
    ocrEventSatisfy(tile_matrix[0][0].bottom_right_event_guid, temp);

    /* Create datablocks for the bottom right elements and bottom rows for tiles[0][j]
     * and Satisfy the bottom row event for tile[0][j] with the respective datablock */
    for ( j = 1; j < n_tiles_width + 1; ++j ) {
        void* db_guid_0_j_brow_data;
        ocrGuid_t temp_j;
        ocrDbCreate( &temp_j, &db_guid_0_j_brow_data, sizeof(int)*tile_width, FLAGS, NULL, NO_ALLOC );
        assert(db_guid_0_j_brow_data);

        allocated = (int*)db_guid_0_j_brow_data;
        for( i = 0; i < tile_width ; ++i ) {
            allocated[i] = GAP*((j-1)*tile_width+i+1);
        }

        ocrEventSatisfy(tile_matrix[0][j].bottom_row_event_guid, temp_j);

        void* db_guid_0_j_br_data;
        ocrGuid_t temp_br_j;
        ocrDbCreate( &temp_br_j, &db_guid_0_j_br_data, sizeof(int), FLAGS, NULL, NO_ALLOC );
        assert(db_guid_0_j_br_data);
        allocated = (int*)db_guid_0_j_br_data;
        allocated[0] = GAP*(j*tile_width); //sagnak: needed to handle tilesize 2

        ocrEventSatisfy(tile_matrix[0][j].bottom_right_event_guid, temp_br_j);
    }

    /* Create datablocks for the right columns for tiles[i][0]
     * and Satisfy the right column event for tile[i][0] with the respective datablock */
    for ( i = 1; i < n_tiles_height + 1; ++i ) {
        void* db_guid_i_0_rc_data;
        ocrGuid_t temp_i;
        ocrDbCreate( &temp_i, &db_guid_i_0_rc_data, sizeof(int)*tile_height, FLAGS, NULL, NO_ALLOC );
        assert(db_guid_i_0_rc_data);
        allocated = (int*)db_guid_i_0_rc_data;
        for ( j = 0; j < tile_height ; ++j ) {
            allocated[j] = GAP*((i-1)*tile_height+j+1);
        }
        ocrEventSatisfy(tile_matrix[i][0].right_column_event_guid, temp_i);

        void* db_guid_i_0_br_data;
        ocrGuid_t temp_br_i;
        ocrDbCreate( &temp_br_i, &db_guid_i_0_br_data, sizeof(int), FLAGS, NULL, NO_ALLOC );
        assert(db_guid_i_0_br_data);

        allocated = (int*)db_guid_i_0_br_data;
        allocated[0] = GAP*(i*tile_height); //sagnak: needed to handle tilesize 2

        ocrEventSatisfy(tile_matrix[i][0].bottom_right_event_guid, temp_br_i);
    }
}

#define SEED 1000

void generate_strings ( int size1, int size2, char* string1, char* string2 ) {
    srand(SEED);

    int index = 0;
    for ( ; index < size1; ++index ) {
        string1[index] = 1 + rand()%4;
    }

    index = 0;
    for ( ; index < size2; ++index ) {
        string2[index] = 1 + rand()%4;
    }
    string1[size1] = 0;
    string2[size2] = 0;
}

void populateAnnotations( long double** granularityAnnotations, int n_tiles_height, int n_tiles_width) {
    int i,j;
    for ( j = n_tiles_width; j > 0 ; --j) {
        granularityAnnotations[n_tiles_height-1][j-1] = n_tiles_width-j; 
    }
    for ( i = n_tiles_height; i > 0 ; --i) {
        granularityAnnotations[i-1][n_tiles_width-1] = n_tiles_height-i; 
    }
    for ( i = n_tiles_height-2; i >= 0 ; --i) {
        for ( j = n_tiles_width-2; j >= 0 ; --j) {
            granularityAnnotations[i][j] = granularityAnnotations[i+1][j]+granularityAnnotations[i][j+1]+granularityAnnotations[i+1][j+1]+3;
        }
    }
}

void freeGranularityAnnotations(long double** granularityAnnotations, int n_tiles_height) {
    int i;
    for ( i = 0; i < n_tiles_height; ++i ) {
        free(granularityAnnotations[i]);
    }
    free(granularityAnnotations);
}

void freeTileMatrix ( Tile_t** tile_matrix, int n_tiles_height, int n_tiles_width ) {
    int i,j;
    for ( i = 0; i < n_tiles_height+1; ++i ) {
        for ( j = 0; j < n_tiles_width+1; ++j ) {
            ocrEventDestroy(tile_matrix[i][j].bottom_row_event_guid);
            ocrEventDestroy(tile_matrix[i][j].right_column_event_guid);
            ocrEventDestroy(tile_matrix[i][j].bottom_right_event_guid);

            free(tile_matrix[i][j].paramv);
        }
        free(tile_matrix[i]);
    }
    free(tile_matrix);
}

int main ( int argc, char* argv[] ) {

    int i, j, times;

    if ( argc < 9 ) {
        fprintf(stderr, "Usage: %s -md <> -bind <> strlen1 strlen2 tileWidth tileHeight times\n", argv[0]);
        exit(1);
    }

    int strlen1     = (int) atoi (argv[5]);
    int strlen2     = (int) atoi (argv[6]);
    int tile_width  = (int) atoi (argv[7]);
    int tile_height = (int) atoi (argv[8]);
    int nTimes      = (int) atoi (argv[9]);

    int n_tiles_width  = strlen1/tile_width;
    int n_tiles_height = strlen2/tile_height;

    if ( n_tiles_width * tile_width != strlen1 ) {
        fprintf(stderr, " strlen1:%d is not divisible by tileWidth:%d\n", strlen1, n_tiles_width);
    }

    if ( n_tiles_height * tile_height != strlen2 ) {
        fprintf(stderr, " strlen2:%d is not divisible by tileHeight:%d\n", strlen2, n_tiles_height);
    }

    // fprintf(stdout, "Imported %d x %d tiles.\n", n_tiles_width, n_tiles_height);

    char* string_1 = (char*) malloc(sizeof(char)*(1+strlen1));
    char* string_2 = (char*) malloc(sizeof(char)*(1+strlen2));
    generate_strings(strlen1, strlen2, string_1, string_2);

    // fprintf(stdout, "Allocating tile matrix\n");

    long double** granularityAnnotations = (long double**)malloc(sizeof(long double*)*n_tiles_height);
    for ( i = 0; i < n_tiles_height; ++i ) {
        granularityAnnotations[i] = (long double*)malloc(sizeof(long double)*n_tiles_width);
    }

    populateAnnotations(granularityAnnotations, n_tiles_height, n_tiles_width);

    for ( times = 0; times < nTimes; ++times ) {
        int changing_argc = argc;
        char** changing_argv = (char**) malloc(sizeof(char*)*argc);
        int argc_it = 0;
        /*sagnak, this will have to leak memory because of the way shift_arguments are implemented in the runtime :( */
        for ( ; argc_it < argc; ++argc_it ) {
            changing_argv[argc_it] = (char*)malloc(sizeof(char)*strlen(argv[argc_it])+1);
            strcpy(changing_argv[argc_it], argv[argc_it]);
        }
        OCR_INIT(&changing_argc, changing_argv, smith_waterman_task );

        /* Allocate a 2D matrix of 'Tile's where every Tile has 3 events that represents the readiness
         * of the bottom row, right column and bottom right element of that tile */
        Tile_t** tile_matrix = (Tile_t **) malloc(sizeof(Tile_t*)*(n_tiles_height+1));
        for ( i = 0; i < n_tiles_height+1; ++i ) {
            tile_matrix[i] = (Tile_t *) malloc(sizeof(Tile_t)*(n_tiles_width+1));
            for ( j = 0; j < n_tiles_width+1; ++j ) {
                /* Create readiness events for every tile */
                ocrEventCreate(&(tile_matrix[i][j].bottom_row_event_guid ), OCR_EVENT_STICKY_T, true);
                ocrEventCreate(&(tile_matrix[i][j].right_column_event_guid ), OCR_EVENT_STICKY_T, true);
                ocrEventCreate(&(tile_matrix[i][j].bottom_right_event_guid ), OCR_EVENT_STICKY_T, true);

                tile_matrix[i][j].paramv = (intptr_t *)malloc(9*sizeof(intptr_t));

            }
        }

        // fprintf(stdout, "Allocated tile matrix\n");

        initialize_border_values(tile_matrix, n_tiles_width, n_tiles_height, tile_width, tile_height);

        struct timeval a;
        struct timeval b;
        gettimeofday(&a, 0);

        for ( i = 1; i < n_tiles_height+1; ++i ) {
            for ( j = 1; j < n_tiles_width+1; ++j ) {

                /* Box function parameters and put them on the heap for lifetime */
                intptr_t *paramv = tile_matrix[i][j].paramv;
                paramv[0]=(intptr_t) i;
                paramv[1]=(intptr_t) j;
                paramv[2]=(intptr_t) tile_width;
                paramv[3]=(intptr_t) tile_height;
                paramv[4]=(intptr_t) tile_matrix;
                paramv[5]=(intptr_t) string_1;
                paramv[6]=(intptr_t) string_2;
                paramv[7]=(intptr_t) n_tiles_height;
                paramv[8]=(intptr_t) n_tiles_width;

                /* Create an event-driven tasks of smith_waterman tasks */
                ocrGuid_t task_guid;

                ocrEdtCreate(&task_guid, smith_waterman_task, 9, NULL, (void **) paramv, PROPERTIES, 3, NULL, granularityAnnotations[i-1][j-1],granularityAnnotations[i-1][j-1]);

                /* add dependency to the EDT from the west tile's right column ready event */
                ocrAddDependence(tile_matrix[i][j-1].right_column_event_guid, task_guid, 0);

                /* add dependency to the EDT from the north tile's bottom row ready event */
                ocrAddDependence(tile_matrix[i-1][j].bottom_row_event_guid, task_guid, 1);

                /* add dependency to the EDT from the northwest tile's bottom right ready event */
                ocrAddDependence(tile_matrix[i-1][j-1].bottom_right_event_guid, task_guid, 2);

                /* schedule task*/
                ocrEdtSchedule(task_guid);
            }
        }

        ocrCleanup();
        gettimeofday(&b, 0);
        fprintf(stderr,"The computation took %f seconds\r\n",((b.tv_sec - a.tv_sec)*1000000+(b.tv_usec - a.tv_usec))*1.0/1000000);

        freeTileMatrix(tile_matrix, n_tiles_height, n_tiles_width);
        for ( argc_it = 0; argc_it < changing_argc; ++argc_it ) {
            free(changing_argv[argc_it]);
        }
        free(changing_argv);
        unravel();
    }

    freeGranularityAnnotations(granularityAnnotations, n_tiles_height);
    free(string_2);
    free(string_1);
    return 0;
}
