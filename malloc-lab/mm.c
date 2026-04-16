/*
 * mm.c - boundary tag를 기반으로 동작하는 malloc package.
 *
 * 기본 경로는 implicit free list + immediate coalescing + first fit을 사용하고,
 * 매크로 전환으로 explicit free list도 선택할 수 있도록 구성하였다.
 * free block은 헤더와 푸터를 이용해 연결 여부를 판단하며, explicit 경로에서는
 * payload 앞부분에 pred/succ 포인터를 저장한다. realloc은 가능하면 인접한
 * free block을 재사용하고, 어려우면 새 블록을 할당한 뒤 payload를 복사한다.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "2 Team",
    /* First member's full name */
    "DongHyun Kim",
    /* First member's email address */
    "kdh949@naver.com",
    /* Second member's full name (leave blank if none) */
    "ChangWon Lee",
    /* Second member's email address (leave blank if none) */
    "gravity980530@gmail.com",
    /* Last member */
    "DaAe Kim",
    "1325680q@gmail.com"
};

/*********************************************************
 **********             환경 변수 설정             **********
 ********************************************************/

/********** 가용 리스트 방식 정의 **********/
#define IMPLICIT_FREE_LIST
// #define EXPLICIT_FREE_LIST

#if (defined(IMPLICIT_FREE_LIST) + defined(EXPLICIT_FREE_LIST)) != 1
#error "IMPLICIT_FREE_LIST와 EXPLICIT_FREE_LIST 중 하나만 선택해야 합니다."
#endif

/* 즉시 연결 >> (+): 외부 단편화 즉각 감소 || (-): 해제할 때마다 병합 연산이 발생 오버헤드가 커질 수 있음
 * 지연 연결 >> (+): 병합 빈도를 줄여 free 연산 자체 오버헤드 감소 || (-): 외부 단편화 일시적 증가
 */

/********** 연결(coalesce) 시점 정의 **********/
#define IMMEDIATE_COALESCING // 즉시 연결
// #define DEFERRED_COALESCING // 지연 연결

#if (defined(IMMEDIATE_COALESCING) + defined(DEFERRED_COALESCING)) != 1
#error "IMMEDIATE_COALESCING와 DEFERRED_COALESCING 중 하나만 선택해야 합니다."
#endif

#ifdef DEFERRED_COALESCING
#error "DEFERRED_COALESCING는 아직 지원하지 않습니다."
#endif

/********** 배치(place) 방식 정의 **********/
// #define FIRSTFIT // 최초 적합
// #define NEXTFIT // 다음 적합
// #define BESTFIT // 최적 적합

#if !defined(FIRSTFIT) && !defined(NEXTFIT) && !defined(BESTFIT)
#define FIRSTFIT
#endif

#if (defined(FIRSTFIT) + defined(NEXTFIT) + defined(BESTFIT)) != 1
#error "FIRSTFIT, NEXTFIT, BESTFIT 중 하나만 선택해야 합니다."
#endif

/********** 재할당(reAlloc) 방식 정의 **********/
// #define DEFAULT_REALLOC // 기본 제공 코드
#define NEW_REALLOC

#if (defined(DEFAULT_REALLOC) + defined(NEW_REALLOC)) != 1
#error "DEFAULT_REALLOC와 NEW_REALLOC 중 하나만 선택해야 합니다."
#endif

/* single word (4) or double word (8) alignment */
#define SINGLEWORD 4
#define DOUBLEWORD 8

#define ALIGNMENT DOUBLEWORD

/* rounds up to the nearest multiple of ALIGNMENT */
// 맨 마지막 3비트를 000으로 바꿈으로써 8의 배수로 만드는 원리
// 001~111 8로 나눈 나머지 (1~7 사이) 값이 무시됨 => 그러니까 올림을 위해 8의 배수가 되지 않도록 8 직전인 7을 더하고 나머지를 버림
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7) //##ALIGNMENT(8)의 배수로 맞춰주는 매크로, 바이트로 반환

/* 교재 9.43 매크로 코드 */
#define WSIZE 4 // 1워드 (4바이트)
#define DSIZE 8 // 2워드 (8바이트)
#define CHUNKSIZE (1<<12) // 2^12를 의미, 2^2*2^10과 같으므로 4Kbyte이다
/*
 * [참고]
 * 운영체제에서 메모리를 관리하는 기본 페이지 크기(Page Size)가 대부분 4Kbyte로 설정되어 메모리 분할 및 할당에 사용
 */

#define MAX(a, b) ((a) > (b) ? (a) : (b)) // 최대값 반환

/*
 * 어차피 size는 8(바이트)의 배수이므로, 맨 뒤 3자리가 000임
 * 그러면 alloc은 어차피 맨 마지막 3개의 비트만 사용할 예정으므로 OR 연산을 통해 합칠 수 있다는 의미
 */
#define PACK(size, alloc) ((size) | (alloc)) // 크기와 할당 여부를 묶어주는 매크로

#define GET(p) (*(unsigned int *)(p)) // p의 값(4바이트)을 가져온다
#define PUT(p, value) (*(unsigned int *)(p) = (value)) //p의 위치에 value를 작성

//맨 마지막 3비트만 000으로 바꿈으로서 크기를 구함
#define GET_SIZE(p) (GET(p) & ~0x7) // 블록의 크기 반환, p(헤더 or 푸터)

//맨 마지막 1비트만 가져와서 할당 여부 확인
#define GET_ALLOC(p) (GET(p) & 0x1) // 블록의 할당 여부 반환, p(헤더 or 푸터)

/* bp: (현재 블럭의 페이로드 시작점) */
#define HDRP(bp) ((char *)(bp) - WSIZE) // bp의 헤더
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // bp의 푸터

/* HDRP(bp): bp(현재 블럭의 페이로드 시작점)의 헤더
 * GET_SIZE(HDRP(bp)): bp(현재 블럭의 페이로드 시작점)의 크기 (현재 블럭의 헤더를 이용해 구함)
 *
 * HDRP(bp) - WSIZE: 이전 블럭의 푸터 == bp(현재 블럭의 페이로드 시작점)의 헤더에서 1워드 이전 시작점
 * GET_SIZE(HDRP(bp) - WSIZE): 이전 블럭의 크기 (이전 블럭의 푸터를 이용해 구함) */

/* 매크로 NEXT_BLKP, PREV_BLKP를 직관적으로 재정의 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp))) // 다음 블럭의 페이로드(bp)
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(HDRP(bp) - WSIZE)) // 이전 블럭의 페이로드(bp)

/* 교재 내용 외 추가 정의 */
#define NEXT_HDRP(bp) (HDRP(NEXT_BLKP(bp))) // 다음 블록의 헤더
#define PREV_FTRP(bp) ((char *)(bp) - DSIZE) // 이전 블록의 푸터

/* implicit은 기존 최소 블록 크기(16바이트)를 유지하고,
 * explicit은 free 블록 payload 안에 pred/succ 포인터를 같이 담을 수 있어야 한다.
 */
#ifdef EXPLICIT_FREE_LIST
#define MINBLOCKSIZE ALIGN(DSIZE + (2 * sizeof(void *)))
#else
#define MINBLOCKSIZE (2 * DSIZE)
#endif

#ifdef EXPLICIT_FREE_LIST
#define PRED_PTR(bp) ((char *)(bp))
#define SUCC_PTR(bp) ((char *)(bp) + sizeof(void *))
#define GET_PTR(p) (*(void **)(p))
#define PUT_PTR(p, value) (*(void **)(p) = (void *)(value))
#define PRED(bp) (GET_PTR(PRED_PTR(bp)))
#define SUCC(bp) (GET_PTR(SUCC_PTR(bp)))
#define SET_PRED(bp, value) (PUT_PTR(PRED_PTR(bp), (value)))
#define SET_SUCC(bp, value) (PUT_PTR(SUCC_PTR(bp), (value)))
#endif

static char *heap_listp = NULL; // 힙의 시작점을 기억하는 포인터

#ifdef EXPLICIT_FREE_LIST
static void *free_listp = NULL; // explicit free list의 시작점을 기억하는 포인터
#endif

#ifdef NEXTFIT
static void *last_bp = NULL;
#endif

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t newsize);
static void place(void *bp, size_t newsize);
static void place_merged_block(void *bp, size_t newsize);
static void place_block(void *bp, size_t newsize, int tracked_in_list);
static size_t adjust_block_size(size_t size);
static void write_alloc_block(void *bp, size_t size);
static void write_free_block(void *bp, size_t size);

#ifdef EXPLICIT_FREE_LIST
static void insert_free_block(void *bp);
static void remove_free_block(void *bp);
#endif

/*
 * adjust_block_size - 요청한 payload 크기를 현재 정책에 맞는 블록 크기로 정규화
 */
static size_t adjust_block_size(size_t size)
{
    size_t asize;

    if (size <= DSIZE)
        asize = MINBLOCKSIZE;
    else
        asize = ALIGN(size + DSIZE);

    if (asize < MINBLOCKSIZE)
        asize = MINBLOCKSIZE;

    return asize;
}

/*
 * write_alloc_block - 헤더/푸터를 사용 중 상태로 기록
 */
static void write_alloc_block(void *bp, size_t size)
{
    PUT(HDRP(bp), PACK(size, 1));
    PUT(FTRP(bp), PACK(size, 1));
}

/*
 * write_free_block - 헤더/푸터를 미할당 상태로 기록
 */
static void write_free_block(void *bp, size_t size)
{
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
}

#ifdef EXPLICIT_FREE_LIST
/*
 * insert_free_block - explicit free list 맨 앞에 free 블록을 삽입
 */
static void insert_free_block(void *bp)
{
    SET_PRED(bp, NULL);
    SET_SUCC(bp, free_listp);

    if (free_listp != NULL)
        SET_PRED(free_listp, bp);

    free_listp = bp;

#ifdef NEXTFIT
    if (last_bp == NULL)
        last_bp = bp;
#endif
}

/*
 * remove_free_block - explicit free list에서 free 블록을 제거
 */
static void remove_free_block(void *bp)
{
    void *pred_bp = PRED(bp);
    void *succ_bp = SUCC(bp);

    if (pred_bp != NULL)
        SET_SUCC(pred_bp, succ_bp);
    else
        free_listp = succ_bp;

    if (succ_bp != NULL)
        SET_PRED(succ_bp, pred_bp);

#ifdef NEXTFIT
    if (last_bp == bp)
        last_bp = (succ_bp != NULL) ? succ_bp : free_listp;
#endif

    SET_PRED(bp, NULL);
    SET_SUCC(bp, NULL);
}
#endif

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) { // 4워드만큼 늘려서 heap_listp에 저장
        return -1; // 힙 할당에 실패했다면 -1 반환 (오류)
    }

    // 헤더가 4바이트(1워드)이므로, 페이로드가 8바이트 단위에 있도록 맨 처음 빈 공간 4바이트+헤더 4바이트를 만듦
    PUT(heap_listp, 0); // 1워드(4바이트)를 빈 공간으로 채움 (패딩)

    /* 프롤로그(Prologue): Coalescing 할 때, 이전 블록의 경계선 설정. 이전 블록을 참조할 때 힙 영역을 벗어나는 것을 방지함 */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // 1워드 헤더 // 힙의 시작점에 1워드를 더하고, 2워드(8바이트/헤더+푸터, 페이로드 없음) 공간으로 사용 중(1) 표시
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // 1워드 푸터 // 힙의 시작점에 2워드를 더하고, 2워드(8바이트/헤더+푸터, 페이로드 없음) 공간으로 사용 중(1) 표시

    /* 에필로그(Epilogue): Coalescing 할 때, 다음 블록의 경계선 설정. 다음 블록을 참조할 때 힙 영역을 벗어나는 것을 방지함 */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1)); // 1워드 헤더

    heap_listp += (2 * WSIZE); // 힙의 시작 위치를 패딩과 프롤로그 헤더 다음으로 설정 (첫 번째 블럭 페이로드)

#ifdef EXPLICIT_FREE_LIST
    free_listp = NULL;
#endif

#ifdef NEXTFIT
#if defined(IMPLICIT_FREE_LIST)
    last_bp = heap_listp;
#else
    last_bp = NULL;
#endif
#endif

    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) // CHUNKSIZE를 워드로 변환(CHUNKSIZE/WSIZE) 후 힙 확장 요청. 확장된 힙에는 헤더와 푸터가 있고, 페이로드를 가리키는 포인터가 반환됨
        return -1; // 할당에 실패한 경우 -1 반환

    return 0;
}

/* extend_heap - 요청한 words 만큼 힙의 크기를 증가시킨 후 블럭 생성 (헤더+페이로드+(패딩)+푸터)
 * 사용 용도: 할당할 수 있는 블록이 없어서 새롭게 만들어야 하거나, 최초 생성시 CHUNKSIZE만큼 생성
 *
 * @param words 확장이 필요한 워드
 * @return 생성된 블럭의 페이로드 시작 주소(bp), 할당할 수 없으면 NULL 반환
 */
static void *extend_heap(size_t words)
{
    char *bp; // 새롭게 생성된 블럭의 페이로드 부분 시작 주소를 가리킬 변수
    size_t size; // 늘려야 할 바이트 크기를 저장할 변수

    /* 워드의 개수는 짝수(8바이트의 배수)로 맞춰야 하므로 아래 수식 사용 */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE; // 워드가 홀수면 (words+1)*WSIZE / 짝수면 words*WSIZE

    if ((long)(bp = mem_sbrk(size)) == -1) { // size만큼 힙 메모리 확장 요청, bp에 확장된 힙의 시작점 저장
        return NULL; // 할당에 실패한 경우(brk를 변경하지 못한 경우) NULL을 반환
    }

    write_free_block(bp, size); // 에필로그 헤더를 현재 free 블록의 헤더/푸터로 덮어씌움

    // 마지막 남은 1word 공간에는 에필로그 헤더를 삽입
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 다음 블럭의 헤더(힙의 마지막)에 크기가 0인 블럭이 있음을 표시

    return coalesce(bp); // 연결하는 함수 호출 (경계 태그 연결 기법을 따라 4가지 케이스 중 하나에 맞춰 동작)
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size;

    if (bp == NULL)
        return;

    size = GET_SIZE(HDRP(bp)); // 현재 블럭의 크기
    write_free_block(bp, size); // 블럭의 헤더와 푸터를 미할당(0) 상태로 변경

#ifdef IMMEDIATE_COALESCING
    coalesce(bp); // 즉시 연결 방식을 사용하므로 해제시 연결
#else
#ifdef EXPLICIT_FREE_LIST
    insert_free_block(bp); // 지연 연결이라도 explicit free list에서는 탐색할 수 있도록 free list에 삽입
#endif
#endif
}

/*
 * coalesce - 이전/이후 블럭의 상태를 확인 후 경계 태그 연결 기법에 따라 블럭 연결
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc; // 이전 블록의 할당 여부
    size_t next_alloc; // 다음 블록의 할당 여부
    size_t size; // 현재 블록을 포함해 합쳐질 블록 크기

    /*
     * 이전 블록의 푸터: FTRP(PREV_BLKP(bp))
     * 이전 블록의 헤더: HDRP(PREV_BLKP(bp))
     *
     * 현재 블록의 푸터: FTRP(bp)
     * 현재 블록의 헤더: HDRP(bp)
     *
     * 다음 블록의 푸터: FTRP(NEXT_BLKP(bp))
     * 다음 블록의 헤더: HDRP(NEXT_BLKP(bp))
     */
    prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록의 푸터를 확인하여 할당 여부 확인
    next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 다음 블록의 헤더를 확인하여 할당 여부 확인
    size = GET_SIZE(HDRP(bp)); // 현재 블록의 크기

    /* CASE 1: 이전, 다음 블록: 모두 할당(1) */
    if (prev_alloc && next_alloc) {
#ifdef EXPLICIT_FREE_LIST
        insert_free_block(bp);
#endif
        return bp; // 아무 행동도 하지 않고 반환 (연결할 수 있는 블록 없음)
    }

    /* CASE 2: 이전 블록: 할당됨(1) || 다음 블록: 미할당(0) */
    else if (prev_alloc && !next_alloc) {
        void *next_bp = NEXT_BLKP(bp);

#ifdef EXPLICIT_FREE_LIST
        remove_free_block(next_bp);
#endif
        size += GET_SIZE(HDRP(next_bp)); // 다음 블록의 크기와 현재 블록의 크기를 더함
        write_free_block(bp, size); // 현재 블록의 헤더와 다음 블록의 푸터를 새로운 크기로 변경
    }

    /* CASE 3: 이전 블록: 미할당(0) || 다음 블록: 할당됨(1) */
    else if (!prev_alloc && next_alloc) {
        void *prev_bp = PREV_BLKP(bp);

#ifdef EXPLICIT_FREE_LIST
        remove_free_block(prev_bp);
#endif
        size += GET_SIZE(HDRP(prev_bp)); // 이전 블록의 크기와 현재 블록의 크기를 더함
        write_free_block(prev_bp, size); // 이전 블록의 헤더와 현재 블록의 푸터를 새로운 크기로 변경
        bp = prev_bp; // bp의 위치를 이전 블록의 페이로드 시작점으로 바꿈
    }

    /* CASE 4: 이전, 다음 블록: 모두 미할당(0) */
    else {
        void *prev_bp = PREV_BLKP(bp);
        void *next_bp = NEXT_BLKP(bp);

#ifdef EXPLICIT_FREE_LIST
        remove_free_block(prev_bp);
        remove_free_block(next_bp);
#endif
        size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp)); // 이전, 현재, 다음 블록의 크기를 모두 더함
        write_free_block(prev_bp, size); // 이전 블록의 헤더와 다음 블록의 푸터를 새로운 크기로 변경
        bp = prev_bp; // 블럭 포인터의 위치를 이전 블럭의 페이로드 시작점으로 바꿈
    }

#ifdef EXPLICIT_FREE_LIST
    insert_free_block(bp);
#endif

#if defined(IMPLICIT_FREE_LIST) && defined(NEXTFIT)
    /* last_bp는 payload 포인터이므로, 비교도 bp와 NEXT_BLKP(bp)처럼 payload 기준으로 하는 게 자연스럽다.
     * 합치려는 블록 범위 안에 최근 탐색 위치가 포함되면 병합 후 블록의 시작점으로 다시 맞춘다.
     */
    if ((char *)bp <= (char *)last_bp && (char *)last_bp < (char *)NEXT_BLKP(bp))
        last_bp = bp;
#endif

    return bp; // CASE 2, 3, 4 처리 후 변경된 bp 반환
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t newsize; // 헤더 및 푸터의 크기를 포함한 실제 할당 필요 크기
    size_t extendsize; // 할당할 수 있는 힙의 크기가 부족한 경우
    char *bp; // 할당될 메모리 블록의 시작 주소를 담을 포인터

    if (size == 0)
        return NULL; // 요청한 크기가 없으면 NULL 반환

    newsize = adjust_block_size(size); // 현재 가용 리스트 정책에 맞는 최소 블록 크기로 정규화

    if ((bp = find_fit(newsize)) != NULL) { // 가용 리스트에서 배치 가능한 위치를 찾은 경우 수행
        place(bp, newsize); // 블록 배치
        return bp; // 생성된 블록 포인터 반환 (페이로드부터 시작)
    }

    // else: 배치 가능한 블럭을 찾지 못한 경우
    extendsize = MAX(CHUNKSIZE, newsize); // 최소 크기(CHUNKSIZE) 이상 키우도록 설정

    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) // 할당할 수 없으면 NULL 반환
        return NULL;

    place(bp, newsize); // 블록 배치
    return bp; // 생성된 블록 포인터 반환 (페이로드부터 시작)
}

/*
 * find_fit - 선택된 배치 적합 방식에 따른 배치 가능한 블록을 반환
 */
static void *find_fit(size_t newsize)
{
#ifdef FIRSTFIT
    void *bp; // 힙 혹은 free list를 순회하기 위한 포인터

#ifdef IMPLICIT_FREE_LIST
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= newsize)
            return bp; // 찾은 블럭의 페이로드 시작 주소 반환
    }
#else
    for (bp = free_listp; bp != NULL; bp = SUCC(bp)) {
        if (GET_SIZE(HDRP(bp)) >= newsize)
            return bp; // 찾은 블럭의 페이로드 시작 주소 반환
    }
#endif

    return NULL; // 찾지 못한 경우
#endif

#ifdef NEXTFIT
    void *bp; // 힙 혹은 free list를 순회하기 위한 포인터
    void *start_bp; // 한 바퀴 순환 시 종료 조건으로 사용할 시작 위치

#ifdef IMPLICIT_FREE_LIST
    if (last_bp == NULL)
        last_bp = heap_listp;

    bp = last_bp;
    start_bp = bp;

    do {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= newsize) {
            last_bp = bp;
            return bp; // 찾은 블럭의 페이로드 시작 주소 반환
        }

        bp = NEXT_BLKP(bp);
        if (GET_SIZE(HDRP(bp)) == 0) // 에필로그 블럭을 만난 경우
            bp = heap_listp; // 힙의 첫 번째 페이로드
    } while (bp != start_bp);
#else
    if (free_listp == NULL)
        return NULL;

    start_bp = (last_bp != NULL) ? last_bp : free_listp;
    bp = start_bp;

    do {
        if (GET_SIZE(HDRP(bp)) >= newsize) {
            last_bp = bp;
            return bp; // 찾은 블럭의 페이로드 시작 주소 반환
        }

        bp = SUCC(bp);
        if (bp == NULL)
            bp = free_listp; // free list 끝까지 갔으면 다시 head부터 순회
    } while (bp != start_bp);
#endif

    return NULL;
#endif

#ifdef BESTFIT
    void *bp; // 힙 혹은 free list를 순회하기 위한 포인터
    void *best_bp = NULL; // 가장 잘 맞는 블록을 기억할 포인터

#ifdef IMPLICIT_FREE_LIST
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= newsize) {
            if (GET_SIZE(HDRP(bp)) == newsize)
                return bp; // 딱 필요한 사이즈라면 바로 반환

            if (best_bp == NULL || GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(best_bp)))
                best_bp = bp;
        }
    }
#else
    for (bp = free_listp; bp != NULL; bp = SUCC(bp)) {
        if (GET_SIZE(HDRP(bp)) >= newsize) {
            if (GET_SIZE(HDRP(bp)) == newsize)
                return bp; // 딱 필요한 사이즈라면 바로 반환

            if (best_bp == NULL || GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(best_bp)))
                best_bp = bp;
        }
    }
#endif

    return best_bp; // best fit 후보가 있으면 반환, 없으면 NULL 반환
#endif

    return NULL;
}

/*
 * place - free list에서 찾은 블록을 사용 중 상태로 배치
 */
static void place(void *bp, size_t newsize)
{
    place_block(bp, newsize, 1);
}

/*
 * place_merged_block - realloc 등으로 임시로 확보한 블록을 사용 중 상태로 배치
 */
static void place_merged_block(void *bp, size_t newsize)
{
    place_block(bp, newsize, 0);
}

/*
 * place_block - 블록의 크기가 크다면 내부 단편화가 생기지 않도록 적절히 분할
 */
static void place_block(void *bp, size_t newsize, int tracked_in_list)
{
    size_t bsize = GET_SIZE(HDRP(bp)); // 현재 블럭 크기

#ifdef EXPLICIT_FREE_LIST
    if (tracked_in_list)
        remove_free_block(bp); // explicit free list에서는 배치 전에 기존 free list에서 제거
#else
    (void)tracked_in_list;
#endif

    /* 블럭 분할 후 배치 */
    if ((bsize - newsize) >= MINBLOCKSIZE) { // 내부 단편화 크기가 최소 블럭 크기보다 크다면
        void *next_bp;

        write_alloc_block(bp, newsize); // 앞부분은 사용 중 블록으로 배치
        next_bp = NEXT_BLKP(bp); // 분할 뒤 남은 공간의 페이로드 시작점
        write_free_block(next_bp, bsize - newsize); // 남은 공간은 free 블록으로 다시 기록

#ifdef IMMEDIATE_COALESCING
        coalesce(next_bp); // 분할 후 남은 공간이 다음 free 블록과 맞닿아 있다면 즉시 병합
#else
#ifdef EXPLICIT_FREE_LIST
        insert_free_block(next_bp); // 지연 연결에서는 남은 free 블록만 free list에 삽입
#endif
#endif
    }

    /* 블럭 전체를 배치 */
    else {
        write_alloc_block(bp, bsize);
    }
}

/*
 * mm_realloc - 동적 메모리 재할당 (크기를 늘리거나 줄임)
 */
void *mm_realloc(void *bp, size_t size)
{
#ifdef DEFAULT_REALLOC
    void *oldptr = bp;
    void *newptr;
    size_t old_block_size;
    size_t copySize;

    if (bp == NULL)
        return mm_malloc(size);

    if (size == 0) {
        mm_free(bp);
        return NULL;
    }

    newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;

    old_block_size = GET_SIZE(HDRP(oldptr));
    copySize = old_block_size - DSIZE;
    if (size < copySize)
        copySize = size;

    memmove(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
#endif

#ifdef NEW_REALLOC
    void *old_bp = bp;
    void *new_bp;
    size_t old_block_size; // 헤더+푸터 포함 전체 블록 크기
    size_t old_payload_size; // 실제 복사해야 하는 payload 크기
    size_t asize; // mm_malloc과 동일 규칙으로 계산한 목표 블록 크기

    /* realloc(NULL, size) == malloc(size) */
    if (bp == NULL)
        return mm_malloc(size);

    /* realloc(ptr, 0) == free(ptr), return NULL */
    if (size == 0) {
        mm_free(bp);
        return NULL;
    }

    old_block_size = GET_SIZE(HDRP(bp));
    old_payload_size = old_block_size - DSIZE;
    asize = adjust_block_size(size); // mm_malloc과 같은 방식으로 크기를 정규화해야 place와 호환 가능

    /* 이미 현재 블록만으로 충분하면, 필요 시 분할만 하고 그대로 반환 */
    if (old_block_size >= asize) {
        place_merged_block(bp, asize);
#if defined(NEXTFIT) && defined(IMPLICIT_FREE_LIST)
        last_bp = bp;
#endif
        return bp;
    }

    /* 이전 블럭이 미할당 상태이고, 이전+현재 블럭만으로 충분하면 이전 블럭까지 합쳐서 확장 */
    if (!GET_ALLOC(PREV_FTRP(bp)) &&
        GET_SIZE(HDRP(PREV_BLKP(bp))) + old_block_size >= asize) {
        void *prev_bp = PREV_BLKP(bp);
        size_t expanded_size = GET_SIZE(HDRP(prev_bp)) + old_block_size;

#ifdef EXPLICIT_FREE_LIST
        remove_free_block(prev_bp);
#endif
        write_free_block(prev_bp, expanded_size);

        /* 새 위치가 앞으로 당겨지므로 겹칠 수 있어 memcpy가 아닌 memmove 사용 */
        memmove(prev_bp, old_bp, old_payload_size);
        place_merged_block(prev_bp, asize);

#if defined(NEXTFIT) && defined(IMPLICIT_FREE_LIST)
        last_bp = prev_bp;
#endif
        return prev_bp;
    }

    /* 다음 블럭이 미할당 상태이고, 현재+다음 블럭만으로 충분하면 같은 위치에서 확장 */
    if (!GET_ALLOC(NEXT_HDRP(bp)) &&
        old_block_size + GET_SIZE(NEXT_HDRP(bp)) >= asize) {
        void *next_bp = NEXT_BLKP(bp);
        size_t expanded_size = old_block_size + GET_SIZE(HDRP(next_bp));

#ifdef EXPLICIT_FREE_LIST
        remove_free_block(next_bp);
#endif
        write_free_block(bp, expanded_size);
        place_merged_block(bp, asize);

#if defined(NEXTFIT) && defined(IMPLICIT_FREE_LIST)
        last_bp = bp;
#endif
        return bp;
    }

    /* 이전, 다음 블럭이 모두 미할당 상태이고 셋을 합치면 충분하면 한 번에 확장 */
    if (!GET_ALLOC(PREV_FTRP(bp)) &&
        !GET_ALLOC(NEXT_HDRP(bp)) &&
        GET_SIZE(HDRP(PREV_BLKP(bp))) + old_block_size + GET_SIZE(NEXT_HDRP(bp)) >= asize) {
        void *prev_bp = PREV_BLKP(bp);
        void *next_bp = NEXT_BLKP(bp);
        size_t expanded_size =
            GET_SIZE(HDRP(prev_bp)) + old_block_size + GET_SIZE(HDRP(next_bp));

#ifdef EXPLICIT_FREE_LIST
        remove_free_block(prev_bp);
        remove_free_block(next_bp);
#endif
        write_free_block(prev_bp, expanded_size);

        /* 새 위치가 앞으로 당겨지므로 겹칠 수 있어 memcpy가 아닌 memmove 사용 */
        memmove(prev_bp, old_bp, old_payload_size);
        place_merged_block(prev_bp, asize);

#if defined(NEXTFIT) && defined(IMPLICIT_FREE_LIST)
        last_bp = prev_bp;
#endif
        return prev_bp;
    }

    /* 확장이 불가능한 케이스 > fallback */
    new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;

    /* 새 요청 크기보다 기존 payload가 더 작을 수 있으므로 작은 쪽만 복사 */
    memmove(new_bp, old_bp, (old_payload_size < size) ? old_payload_size : size);
    mm_free(old_bp);

#if defined(NEXTFIT) && defined(IMPLICIT_FREE_LIST)
    last_bp = new_bp;
#endif
    return new_bp;
#endif
}
