/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t newsize);
static void place(void *bp, size_t newsize);

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

/* 즉시 연결 >> (+): 외부 단편화 즉각 감소 || (-): 해제할 때마다 병합 연산이 발생 오버헤드가 커질 수 있음
 * 지연 연결 >> (+): 병합 빈도를 줄여 free 연산 자체 오버헤드 감소 || (-): 외부 단편화 일시적 증가
 */

/********** 연결(coalesce) 시점 정의 **********/
#define IMMEDIATE_COALESCING // 즉시 연결
// #define DEFERRED_COALESCING // 지연 연결

/********** 배치(place) 방식 정의 **********/
#define FIRSTFIT // 최초 적합
// #define NEXTFIT // 다음 적합
// #define BESTFIT // 최적 적합

#ifdef NEXTFIT
static void *last_bp = NULL;
#endif


/********** 재할당(reAlloc) 방식 정의 **********/
// #define DEFAULT_REALLOC // 기본 제공 코드
#define NEW_REALLOC

/* single word (4) or double word (8) alignment */
#define SINGLEWORD 4
#define DOUBLEWORD 8

#define ALIGNMENT DOUBLEWORD

/* rounds up to the nearest multiple of ALIGNMENT */
// 맨 마지막 3비트를 000으로 바꿈으로써 8의 배수로 만드는 원리
// 001~111 8로 나눈 나머지 (1~7 사이) 값이 무시됨 => 그러니까 올림을 위해 8의 배수가 되지 않도록 8 직전인 7을 더하고 나머지를 버림
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7) //##ALIGNMENT(8)의 배수로 맞춰주는 매크로, 바이트로 반환

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

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

/* 
#define NEXT_BLKP(bp) ((char*)(bp) + (GET_SIZE((char*)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char*)(bp) - (GET_SIZE((char*)(bp) - DSIZE)))
*/

/* 매크로 NEXT_BLKP, PREV_BLKP를 직관적으로 재정의 */

#define NEXT_BLKP(bp) ((char*)(bp) + (GET_SIZE(HDRP(bp)))) //다음 블럭의 페이로드(bp)
#define PREV_BLKP(bp) ((char*)(bp) - (GET_SIZE(HDRP(bp) - WSIZE))) // 이전 블럭의 페이로드(bp)

/* 교재 내용 외 추가 정의 */
#define NEXT_HDRP(bp) (HDRP(NEXT_BLKP(bp))) // 다음 블록의 헤더
#define PREV_FTRP(bp) ((char *)(bp) - DSIZE) // 이전 블록의 푸터

static char* heap_listp = NULL;	 // 힙의 시작점을 기억하는 포인터

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
	if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void*)-1) {  // 4워드만큼 늘려서 heap_listp에 저장
		return -1;	  // 힙 할당에 실패했다면 -1 반환 (오류)
	}

	// 헤더가 4바이트(1워드)이므로, 페이로드가 8바이트 단위에 있도록 맨 처음 빈 공간 4바이트+헤더 4바이트를 만듦
	PUT(heap_listp, 0);	 // 1워드(4바이트)를 빈 공간으로 채움 (패딩)

    /* 프롤로그(Prologue): Coalescing 할 때, 이전 블록의 경계선 설정. 이전 블록을 참조할 때 힙 영역을 벗어나는 것을 방지함 */
	PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // 1워드 헤더 // 힙의 시작점에 1워드를 더하고, 2워드(8바이트/헤더+푸터, 페이로드 없음) 공간으로 사용 중(1) 표시 
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // 1워드 푸터 // 힙의 시작점에 2워드를 더하고, 2워드(8바이트/헤더+푸터, 페이로드 없음) 공간으로 사용 중(1) 표시

    /* 에필로그(Epilogue): Coalescing 할 때, 다음 블록의 경계선 설정. 다음 블록을 참조할 때 힙 영역을 벗어나는 것을 방지함 */
	PUT(heap_listp + (3 * WSIZE), PACK(0, 1));	// 1워드 헤더

	heap_listp += (2 * WSIZE); // 힙의 시작 위치를 패딩과 프롤로그 헤더 다음으로 설정 (첫 번째 블럭 페이로드)

#ifdef NEXTFIT //Next fit 배치 방식 사용시 사용
    last_bp = heap_listp;
#endif

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) // CHUNKSIZE를 워드로 변환(CHUNKSIZE/WSIZE) 후 힙 확장 요청. 확장된 힙에는 헤더와 푸더가 있고, 페이로드를 가리키는 포인터가 반환됨
        return -1; // 할당에 실패한 경우 -1 반환

	return 0;
}

/* extend_heap - 요청한 words 만큼 힙의 크기를 증가시킨 후 블럭 생성 (헤더+페이로드+(패딩)+푸터)
* 사용 용도: 할당할 수 있는 블록이 없어서 새롭게 만들어야 하거나, 최초 생성시 CHUNKSIZE만큼 생성
* 
* @param words 확장이 필요한 워드
* @return 생성된 블럭의 페이로드 시작 주소(bp), 할당할 수 없으면 NULL 반환
*/
static void *extend_heap(size_t words){
    char *bp; // 새롭게 생성된 블럭의 페이로드 부분 시작 주소를 가리킬 변수
    size_t size; // 늘려야 할 바이트 크기를 저장할 변수

    /* 워드의 개수는 짝수(8바이트의 배수)로 맞춰야 하므로 아래 수식 사용 */
    size = (words%2) ? (words+1) * WSIZE : words * WSIZE; // 워드가 홀수면 (words+1)*WSIZE / 짝수면 words*WSIZE
    
    if((long)(bp = mem_sbrk(size)) == -1){ // size만큼 힙 메모리 확장 요청, bp에 확장된 힙의 시작점 저장
        return NULL; // 할당에 실패한 경우(brk를 변경하지 못한 경우) NULL을 반환
    }

    PUT(HDRP(bp), PACK(size, 0)); // HDRP(bp)로 에필로그 헤더 위치를 가리킴 // 에필로그 헤더를 현재 블록의 헤더로 덮어씌움
    PUT(FTRP(bp), PACK(size, 0)); // FTRP(bp)로 힙의 마지막에서 2 words(푸터 넣을 공간+에필로그 헤더 넣을 공간) 이전을 가리킴

    // 마지막 남은 1word 공간에는 에필로그 헤더를 삽입
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 다음 블럭의 헤더(힙의 마지막)에 크기가 0인 블럭이 있음을 표시

    return coalesce(bp); // 연결하는 함수 호출 (경계 태그 연결 기법을 따라 4가지 케이스 중 하나에 맞춰 동작)
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
    size_t size = GET_SIZE(HDRP(bp)); // 현재 블럭의 크기

    /* 블럭의 헤더와 푸더를 미할당(0) 상태로 변경 */
    PUT(HDRP(bp), PACK(size, 0)); //헤더 변경
    PUT(FTRP(bp), PACK(size, 0)); //푸터 변경

#ifdef IMMEDIATE_COALESCING
	coalesce(bp);  // 즉시 연결 방식을 사용하므로 해제시 연결
#endif
}

/*
 * coalesce - 이전/이후 블럭의 상태를 확인 후 경계 태그 연결 기법에 따라 블럭 연결
 */
static void *coalesce(void *bp){
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

    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); //이전 블록의 푸터를 확인하여 할당 여부 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); //다음 블록의 헤더를 확인하여 할당 여부 확인
    size_t size = GET_SIZE(HDRP(bp)); // 현재 블록의 크기

    /* CASE 1: 이전, 다음 블록: 모두 할당(1) */
	if (prev_alloc && next_alloc) {
		return bp;	// 아무 행동도 하지 않고 반환 (연결할 수 있는 블록 없음)
	}

	/* CASE 2: 이전 블록: 할당됨(1) || 다음 블록: 미할당(0) */
	else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록의 크기와 현재 블록의 크기를 더함

        /* 현재 블록의 헤더와 다음 블록의 푸터를 새로운 크기로 변경 */
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0)); 
        
        /* #### 왜 다음 블록 푸터가 FTRP(NEXT_BLKP(bp))가 아닌 FTRP(bp)인가?! ####
        * HDRP(bp)로 현재 블록(bp)의 헤더 크기를 합친 크기로 바꿨음
        * FTRP 매크로는 현재 블록의 헤더에 작성된 크기를 읽고, 해당 크기만큼 더해서 건너뜀
        * 그렇다면 현재 bp의 헤더에는 다음 블록까지 합한 길이가 있을 것이므로 헤더만으로 다음 블록의 푸터까지 갈 수 있는 것
        */

        // bp = bp; //bp는 현재 블럭의 페이로드이므로 변경 불필요
	}

	/* CASE 3: 이전 블록: 미할당(0) || 다음 블록: 할당됨(1) */
	else if (!prev_alloc && next_alloc) {
		size += GET_SIZE(FTRP(PREV_BLKP(bp))); // 이전 블록의 크기와 현재 블록의 크기를 더함

        /* 현재 블록의 푸터와 이전 블록의 헤더를 새로운 크기로 변경 */
        PUT(FTRP(bp), PACK(size, 0));

        /* 현재 블록의 푸터를 바꾼 것이므로 현재 블록의 헤더를 읽어서 이전 블록으로 이동할 수 있음 */
        bp = PREV_BLKP(bp); //bp의 위치를 이전 블록의 페이로드 시작점으로 바꿈
        PUT(HDRP(bp), PACK(size, 0)); //이전 블록의 헤더 변경
	}

	/* CASE 4: 이전, 다음 블록: 모두 미할당(0) */
    // else if(!prev_alloc && !next_alloc){
    else {
        /* 이전 블록의 크기, 현재 블록의 크기, 다음 블록의 크기를 더함 */
		size += GET_SIZE(PREV_FTRP(bp)) + GET_SIZE(NEXT_HDRP(bp)); 

        /* 이전 블록의 헤더와 다음 블록의 푸터를 새로운 크기로 변경 */
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));

        //블럭 포인터의 위치를 이전 블럭의 페이로드 시작점으로 바꿈
        bp = PREV_BLKP(bp);
	}
#ifdef NEXTFIT
	/* Codex 의견: last_bp는 payload 포인터이므로, 비교도 bp와 NEXT_BLKP(bp)처럼 payload 기준으로 하는 게 자연스럽습니다.
     * 지금처럼 HDRP(bp)와 FTRP(bp)를 쓰면 “헤더~푸터 사이”를 보는 셈이라 개념상 조금 덜 직관적입니다.
     * char *로 캐스팅해 비교하는 쪽이 포인터 범위 비교 의도가 더 분명합니다.
     */
    // if (HDRP(bp) < last_bp && last_bp < FTRP(bp)) {  // 합치려는 블록이 NEXTFIT으로 최근에 배치했던 위치인 경우
	if ((char*)bp <= (char*)last_bp && (char*)last_bp < (char*)NEXT_BLKP(bp)) {
		last_bp = bp;
	}
#endif

	return bp; //CASE 2, 3, 4 처리 후 변경된 bp 반환
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) 
{ 
#ifdef DEFERRED_COALESCING
	// TODO: 메모리 할당시 연결 방식 구현
#endif

	if (size == 0)
		return NULL;  // 요청한 크기가 없으면 NULL 반환
	
	size_t newsize; // 헤더 및 푸터의 크기를 포함한 실제 할당 필요 크기
    size_t extendsize; // 할당할 수 있는 힙의 크기가 부족한 경우
    char *bp; // 할당될 메모리 블록의 시작 주소를 담을 포인터

	if (size <= DSIZE) // 최소 단위 (헤더 (1워드) + 페이로드(2워드) + 푸터(1워드)) 4워드=16바이트 보다 작은 경우 
		newsize = 2 * DSIZE; // 4워드만큼 할당
	else
		newsize = ALIGN(size + DSIZE);	// 요청한 크기에 헤더와 푸터(2words)의 크기를 더한뒤 8의 배수로 맞춤

    if((bp = find_fit(newsize)) != NULL){ // 가용 리스트에서 배치 가능한 위치를 찾은 경우 수행
        place(bp, newsize); // 블록 배치
        return bp; // 생성된 블록 포인터 반환 (페이로드부터 시작)
    }

    // else: 배치 가능한 블럭을 찾지 못한 경우
    extendsize = MAX(CHUNKSIZE, newsize); //최소 크기 (CHUNKSIZE)이상 키우도록 설정
    
    if((bp = extend_heap(extendsize/WSIZE)) == NULL) // 할당할 수 없으면 NULL 반환
        return NULL;
    
    place(bp, newsize); // 블록 분할
    return bp; // 생성된 블록 포인터 반환 (페이로드부터 시작)
}


/*
 * find_fit - 선택된 배치 적합 방식에 따른 배치 가능한 블록을 반환
 */
static void* find_fit(size_t newsize) {

/* FIRST FIT : 최초 적합*/
#ifdef FIRSTFIT
    void *bp; // 힙의 블럭 순회를 위한 포인터
    bp = heap_listp;

    while (GET_SIZE(HDRP(bp)) > 0) {
		if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= newsize)) {
			return bp; // 찾은 블럭의 페이로드 시작 주소 반환
		}

		bp = NEXT_BLKP(bp);
    }
    
    return NULL; // 찾지 못한 경우
#endif

#ifdef NEXTFIT
    void *bp = last_bp; // 힙의 블럭 순회를 위한 포인터
    void *start_bp = bp;

	do {
		if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= newsize)) {
			last_bp = bp;
			return bp;	// 찾은 블럭의 페이로드 시작 주소 반환
		}

		bp = NEXT_BLKP(bp);
		if (GET_SIZE(HDRP(bp)) == 0) {	// 에필로그 블럭을 만난 경우
			bp = heap_listp;	 // 힙의 첫 번째 페이로드
		}
	} while (bp != start_bp);

	return NULL;

#endif

#ifdef BESTFIT
	void* bp;  // 힙의 블럭 순회를 위한 포인터
	bp = heap_listp;

    void *best_bp = NULL;

	while (GET_SIZE(HDRP(bp)) > 0) {
		if (!GET_ALLOC(HDRP(bp))) { // 미할당 상태면서

			if (GET_SIZE(HDRP(bp)) == newsize)	// 딱 필요한 사이즈라면
				return bp;	// 찾은 블럭의 페이로드 시작 주소 반환
			else if (GET_SIZE(HDRP(bp)) >= newsize && best_bp == NULL)
				best_bp = bp;
			else if (GET_SIZE(HDRP(bp)) >= newsize && GET_SIZE(HDRP(best_bp)) > GET_SIZE(HDRP(bp)))
				best_bp = bp;
		}
		bp = NEXT_BLKP(bp);
	}
    if (best_bp != NULL)
        return best_bp;
    
	return NULL;  // 찾지 못한 경우

#endif
}

/*
 * place - 블록 배치 함수
 * 블록의 크기가 크다면 블록의 내부 단편화가 생기지 않도록 적절히 분할
 */
static void place(void *bp, size_t newsize) {
    size_t bsize = GET_SIZE(HDRP(bp)); // 블럭 크기

    /* 블럭 분할 후 배치 */
    if((bsize - newsize) >= (2*DSIZE)){ // 내부 단편화 크기가 최소 블럭 크기보다 크다면
        // newsize는 8의 배수! (2워드 단위)
        /* 헤더와 푸터의 크기를 새로운 사이즈로 변경 */
        PUT(HDRP(bp), PACK(newsize, 1));
        PUT(FTRP(bp), PACK(newsize, 1));

        /* bp를 다음 블록의 페이로드로 이동 */
        bp = NEXT_BLKP(bp);

        /* 분할 후 공간 맨 처음에 새로운 헤더 추가 */
        PUT(HDRP(bp), PACK(bsize - newsize, 0));

        /* 분할 후 공간의 푸터의 크기 값 변경 */
        PUT(FTRP(bp), PACK(bsize - newsize, 0));
    }

    /* 블럭 배치 */
    else {
        PUT(HDRP(bp), PACK(bsize, 1));
        PUT(FTRP(bp), PACK(bsize, 1));
    }
}


/*
 * mm_realloc - 동적 메모리 재할당 (크기를 늘리거나 줄임)
 */
void *mm_realloc(void *bp, size_t size)
{
#ifdef DEFAULT_REALLOC
	void* oldptr = bp;
	void* newptr;
	size_t copySize;

	newptr = mm_malloc(size);
	if (newptr == NULL) return NULL;
	copySize = *(size_t*)((char*)oldptr - SIZE_T_SIZE);
	if (size < copySize) copySize = size;
	memcpy(newptr, oldptr, copySize);
	mm_free(oldptr);
	return newptr;

#endif

#ifdef NEW_REALLOC
    void *old_bp = bp;
    void *new_bp;
    size_t old_block_size;   // 헤더+푸터 포함 전체 블록 크기
    size_t old_payload_size; // 실제 복사해야 하는 payload 크기
    size_t asize;            // mm_malloc과 동일 규칙으로 계산한 목표 블록 크기

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

    /* mm_malloc과 같은 방식으로 크기를 정규화해야 place와 호환 가능 */
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size + DSIZE);

    /* 이미 현재 블록만으로 충분하면, 필요 시 분할만 하고 그대로 반환 */
    if (old_block_size >= asize) {
        place(bp, asize);
        return bp;
    }

	/* 이전 블럭이 미할당 상태이고, 이전, 현재 블럭 모두 더한 값이 size 보다 크거나 같으면 연결 */
    if (!GET_ALLOC(PREV_FTRP(bp)) &&
        GET_SIZE(HDRP(PREV_BLKP(bp))) + old_block_size >= asize) {
        void *prev_bp = PREV_BLKP(bp);
        size_t expanded_size = GET_SIZE(HDRP(prev_bp)) + old_block_size;

        PUT(HDRP(prev_bp), PACK(expanded_size, 1));
        PUT(FTRP(prev_bp), PACK(expanded_size, 1));

        /* 겹칠 수 있으므로 memcpy가 아닌 memmove 사용 */
        memmove(prev_bp, old_bp, old_payload_size);

        place(prev_bp, asize);
        return prev_bp;
    }


	/* 다음 블럭이 미할당 상태이고, 현재, 다음 블럭 모두 더한 값이 size 보다 크거나 같으면 연결 */
    if (!GET_ALLOC(NEXT_HDRP(bp)) &&
        old_block_size + GET_SIZE(NEXT_HDRP(bp)) >= asize) {
        size_t expanded_size = old_block_size + GET_SIZE(NEXT_HDRP(bp));

        PUT(HDRP(bp), PACK(expanded_size, 1));
        PUT(FTRP(bp), PACK(expanded_size, 1));

        place(bp, asize);
        return bp;
    }

	/* 이전, 다음 블럭이 모두 미할당 상태이고, 이전, 현재, 다음 블럭 모두 더한 값이 size 보다 크거나 같으면 연결 */
    if (!GET_ALLOC(PREV_FTRP(bp)) &&
        !GET_ALLOC(NEXT_HDRP(bp)) &&
        GET_SIZE(HDRP(PREV_BLKP(bp))) + old_block_size + GET_SIZE(NEXT_HDRP(bp)) >= asize) {
        void *prev_bp = PREV_BLKP(bp);
        void *next_bp = NEXT_BLKP(bp);
        size_t expanded_size =
            GET_SIZE(HDRP(prev_bp)) + old_block_size + GET_SIZE(HDRP(next_bp));

        PUT(HDRP(prev_bp), PACK(expanded_size, 1));
        PUT(FTRP(next_bp), PACK(expanded_size, 1));

        memmove(prev_bp, old_bp, old_payload_size);

        place(prev_bp, asize);
        return prev_bp;
    }

    /* 확장이 불가능한 케이스 > fallback */
    new_bp = mm_malloc(size);
    if (new_bp == NULL)
        return NULL;

    /* 새 요청 크기보다 기존 payload가 더 작을 수 있으므로 작은 쪽만 복사 */
    memmove(new_bp, old_bp, (old_payload_size < size) ? old_payload_size : size);
    mm_free(old_bp);

    return new_bp;
#endif
}
