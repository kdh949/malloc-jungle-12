#####################################################################
# CS:APP Malloc Lab
# Traces
#
# Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
# May not be used, modified, or copied without permission.
#
######################################################################

This directory contains traces of allocate and free requests that are
used by the test harness to evaluate the student malloc packages.

*********
1. Files
*********

*.rep		Original traces
*-bal.rep	Balanced versions of the original traces
gen_XXX.pl	Perl script that generates *.rep	
checktrace.pl	Checks trace for consistency and outputs a balanced version
Makefile	Generates traces

Note: A "balanced" trace has a matching free request for each allocate
request.

**********************
2. Building the traces
**********************
To rebuild the traces from scratch, type 

	unix> make

********************
3. Trace file format
********************

A trace file is an ASCII file. It begins with a 4-line header:

<sugg_heapsize>   /* suggested heap size (unused) */
<num_ids>         /* number of request id's */
<num_ops>         /* number of requests (operations) */
<weight>          /* weight for this trace (unused) */

The header is followed by num_ops text lines. Each line denotes either
an allocate [a], reallocate [r], or free [f] request. The <alloc_id>
is an integer that uniquely identifies an allocate or reallocate
request.

a <id> <bytes>  /* ptr_<id> = malloc(<bytes>) */
r <id> <bytes>  /* realloc(ptr_<id>, <bytes>) */ 
f <id>          /* free(ptr_<id>) */

For example, the following trace file:

<beginning of file>
20000
3
8
1
a 0 512
a 1 128
r 0 640
a 2 128
f 1
r 0 768
f 0
f 2
<end of file>

is balanced. It has a recommended heap size of 20000 bytes (ignored),
three distinct request ids (0, 1, and 2), eight different requests
(one per line), and a weight of 1 (ignored).

************************
4. Description of traces
************************

* short{1,2}-bal.rep

Tiny synthetic tracefiles for debugging

* {amptjp,cccp,cp-decl,expr}-bal.rep

Traces generated from real programs.

* {binary,binary2}-bal.rep

The allocation pattern is to alternatively allocate a small-sized
chunk of memory and a large-sized chunk. The small-sized chunks
(either 16 or 64 ) are deliberately set to be power of 2 while the
large-size chunks (either 112 or 448) are not a power of 2. Defeats
buddy algorithms. However, a simple-minded algorithm might prevail in
this scenario because a first-fit scheme will be good enough.

* coalescing-bal.rep

Repeatedly allocate two equal-sized chunks (4095 in size) and release
them, and then immediately allocate and free a chunk twice as big
(8190). This tests if the students' algorithm ever really releases
memory and does coalescing. The size is chosen to give advantage to
tree-based or segrated fits algorithms where there is no header or
footer overhead.

* {random,random2}-bal.rep
	
Random allocate and free requesets that simply test the correctness
and robustness of the algorithm.


* {realloc,realloc2}-bal.rep
	
Reallocate previously allocated blocks interleaved by other allocation
request. The purpose is to test whether a certain amount of internal
fragments are allocated or not. Naive realloc implementations that
always realloc a brand new block will suffer.

---
# 한국어 버전
#####################################################################
# CS:APP Malloc Lab
# Traces
#
# Copyright (c) 2002, R. Bryant and D. O'Hallaron, All rights reserved.
# May not be used, modified, or copied without permission.
#
######################################################################

이 디렉토리는 테스트 하네스(test harness)가 학생들의 malloc 패키지를 평가하는 데 사용하는 할당(allocate) 및 해제(free) 요청 트레이스 파일들을 포함하고 있습니다.

파일 구성

*.rep		원본 트레이스 파일
*-bal.rep	원본 트레이스의 "균형 잡힌(Balanced)" 버전
gen_XXX.pl	*.rep 파일을 생성하는 Perl 스크립트
checktrace.pl	트레이스의 일관성을 확인하고 균형 잡힌 버전을 출력하는 스크립트
Makefile	트레이스를 생성하기 위한 메이크파일

참고: "균형 잡힌(balanced)" 트레이스란 모든 할당(allocate) 요청에 대해 그에 대응하는 해제(free) 요청이 존재하는 트레이스를 의미합니다.

트레이스 빌드 방법

트레이스를 처음부터 다시 빌드하려면 다음을 입력하세요:

unix> make


트레이스 파일 형식

트레이스 파일은 ASCII 텍스트 파일입니다. 파일은 다음과 같은 4줄의 헤더로 시작합니다:

<sugg_heapsize>   /* 권장 힙 크기 (사용되지 않음) /
<num_ids>         / 요청 ID의 개수 /
<num_ops>         / 요청(작업)의 총 개수 /
<weight>          / 이 트레이스의 가중치 (사용되지 않음) */

헤더 뒤에는 num_ops만큼의 텍스트 줄이 이어집니다. 각 줄은 할당 [a], 재할당 [r], 또는 해제 [f] 요청을 나타냅니다. <alloc_id>는 할당 또는 재할당 요청을 고유하게 식별하는 정수입니다.

a <id> <bytes>  /* ptr_<id> = malloc(<bytes>) /
r <id> <bytes>  / realloc(ptr_<id>, <bytes>) /
f <id>          / free(ptr_<id>) */

예를 들어, 아래의 트레이스 파일은 균형 잡힌 트레이스입니다:

<파일 시작>
20000
3
8
1
a 0 512
a 1 128
r 0 640
a 2 128
f 1
r 0 768
f 0
f 2
<파일 끝>

권장 힙 크기는 20000바이트(무시됨), 3개의 고유한 요청 ID(0, 1, 2), 8개의 서로 다른 요청(한 줄당 하나), 그리고 가중치 1(무시됨)을 가집니다.

트레이스 상세 설명

short{1,2}-bal.rep

디버깅을 위한 아주 작은 합성(synthetic) 트레이스 파일들입니다.

{amptjp,cccp,cp-decl,expr}-bal.rep

실제 프로그램 실행 중에 생성된 트레이스들입니다.

{binary,binary2}-bal.rep

작은 크기의 메모리 청크와 큰 크기의 청크를 번갈아 할당하는 패턴입니다. 작은 청크(16 또는 64)는 의도적으로 2의 거듭제곱으로 설정된 반면, 큰 청크(112 또는 448)는 2의 거듭제곱이 아닙니다. 이는 버디(buddy) 알고리즘을 무력화하기 위함입니다. 그러나 이 시나리오에서는 단순한 First-fit 방식이 충분히 좋은 성능을 보일 수 있습니다.

coalescing-bal.rep

크기가 같은 두 개의 청크(4095)를 반복적으로 할당하고 해제한 뒤, 즉시 그 두 배 크기의 청크(8190)를 할당하고 해제합니다. 이는 학생들의 알고리즘이 실제로 메모리를 해제하고 병합(coalescing)을 수행하는지 테스트합니다. 크기는 헤더나 푸터 오버헤드가 없는 트리 기반 또는 분리 가용 리스트(segregated fits) 알고리즘에 유리하도록 선택되었습니다.

{random,random2}-bal.rep

알고리즘의 정확성과 견고성을 단순하게 테스트하는 무작위 할당 및 해제 요청들입니다.

{realloc,realloc2}-bal.rep

다른 할당 요청들 사이에 끼워진 기존 블록들에 대한 재할당 요청들입니다. 목적은 일정량의 내부 단편화(internal fragments)가 발생하는지 여부를 테스트하는 것입니다. 항상 새로운 블록을 할당하는 방식의 나이브(naive)한 realloc 구현은 이 테스트에서 성능 저하를 겪게 됩니다.

## 각 열의 의미

- trace: 몇 번째 테스트 케이스인지 나타내는 인덱스입니다. 어떤 파일인지는 맨 뒤 tracefile 열과 대응됩니다.

- valid: 해당 trace를 직접 구현한 allocator가 올바르게 처리했는지입니다.
yes면 통과, no면 중간에 오류가 났다는 뜻입니다.

- util: 공간 활용도입니다.
trace 실행 중 “실제로 사용자 payload로 쓴 최대 바이트”를 “heap이 최대로 커진 크기”로 나눈 비율이라서, 높을수록 단편화가 적고 효율적입니다.

- ops: 그 trace 안에 들어 있는 총 연산 수입니다.
malloc, free, realloc 요청 개수를 모두 합친 값입니다.

- secs: 그 trace를 처리하는 데 걸린 시간입니다. 초 단위입니다.

- Kops: 처리량입니다. 초당 몇 천 개 연산을 처리했는지라서, 계산은 대략 ops / secs / 1000입니다. 높을수록 빠릅니다.

- tracefile: 실제 실행된 trace 파일 이름입니다.
어떤 케이스에서 성능이 좋고 나쁜지 직접 확인할 때 보면 됩니다.
