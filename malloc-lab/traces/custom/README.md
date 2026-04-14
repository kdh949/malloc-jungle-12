# Custom Trace Corpus

`traces/custom`는 기존 CS:APP trace를 건드리지 않고 확장 검증 시나리오를 추가한 디렉터리입니다.

- `valid/`: stock `mdriver`로 끝까지 실행되는 balanced trace
- `behavioral/`: 형식은 맞지만 0-byte 요청이나 OOM처럼 기대 실패를 관찰하는 trace
- `negative/`: validator 또는 parser 단계에서 거부되어야 하는 의도적 오류 입력
- `generators/`: deterministic stress trace를 재생성하는 Perl 스크립트

권장 실행 순서:

1. `smoke`
2. `fragmentation`
3. `realloc`
4. `stress`
5. `behavioral`
6. `negative`

공통 명령:

- balanced 확인: `perl traces/checktrace.pl -s < traces/custom/valid/<file>`
- valid 실행: `./mdriver -V -l -f traces/custom/valid/<file>`
- behavioral 실행: `./mdriver -V -f traces/custom/behavioral/<file>`
- negative validator 검사: `perl traces/checktrace.pl < traces/custom/negative/<file>`
- negative parser 검사: `./mdriver -V -f traces/custom/negative/<file>`
- stress 재생성:
  - `perl traces/custom/generators/gen_random_suite.pl`
  - `perl traces/custom/generators/gen_biased_suite.pl`
  - `perl traces/custom/generators/gen_fragment_suite.pl`
  - `perl traces/custom/generators/gen_realloc_suite.pl`

## Valid Traces

| File | Class | Purpose | Command | Expected Result | Notes |
| --- | --- | --- | --- | --- | --- |
| `boundary-min-sizes-bal.rep` | smoke | `1,2,7,8,9,15,16,17` 바이트 요청으로 최소 블록과 정렬 경계를 확인 | `./mdriver -V -l -f traces/custom/valid/boundary-min-sizes-bal.rep` | pass | hand-crafted |
| `boundary-chunksize-cross-bal.rep` | smoke | `CHUNKSIZE` 전후 요청으로 heap extend 경계를 확인 | `./mdriver -V -l -f traces/custom/valid/boundary-chunksize-cross-bal.rep` | pass | hand-crafted |
| `coalesce-4cases-bal.rep` | fragmentation | 이전/다음 블록 상태 4조합에서 coalescing을 유도 | `./mdriver -V -l -f traces/custom/valid/coalesce-4cases-bal.rep` | pass | hand-crafted |
| `fragment-split-reuse-bal.rep` | fragmentation | 큰 free block을 여러 작은 요청이 분할 재사용하는지 확인 | `./mdriver -V -l -f traces/custom/valid/fragment-split-reuse-bal.rep` | pass | hand-crafted |
| `fragment-hole-fill-bal.rep` | fragmentation | 교차 해제로 만든 hole을 유사 크기 요청으로 다시 채우기 | `./mdriver -V -l -f traces/custom/valid/fragment-hole-fill-bal.rep` | pass | hand-crafted |
| `exact-fit-reuse-bal.rep` | fragmentation | free block과 같은 크기의 요청을 다시 넣어 exact fit 재사용을 확인 | `./mdriver -V -l -f traces/custom/valid/exact-fit-reuse-bal.rep` | pass | hand-crafted |
| `realloc-grow-shrink-bal.rep` | realloc | 단일 블록 grow/shrink 시나리오와 bookkeeping을 확인 | `./mdriver -V -l -f traces/custom/valid/realloc-grow-shrink-bal.rep` | pass | hand-crafted |
| `realloc-interleaved-preserve-bal.rep` | realloc | 여러 ID의 교차 realloc과 데이터 보존을 압박 | `./mdriver -V -l -f traces/custom/valid/realloc-interleaved-preserve-bal.rep` | pass | hand-crafted |
| `realloc-neighbor-grow-bal.rep` | realloc | 인접 free block이 있을 때 realloc grow가 어떻게 처리되는지 확인 | `./mdriver -V -l -f traces/custom/valid/realloc-neighbor-grow-bal.rep` | pass | hand-crafted |
| `realloc-same-size-bal.rep` | realloc | 같은 크기로 realloc하는 no-op 성격의 공통 패턴을 확인 | `./mdriver -V -l -f traces/custom/valid/realloc-same-size-bal.rep` | pass | hand-crafted |
| `free-order-lifo-bal.rep` | fragmentation | 최근 free block이 먼저 재사용되는 패턴을 관찰 | `./mdriver -V -l -f traces/custom/valid/free-order-lifo-bal.rep` | pass | hand-crafted |
| `free-order-fifo-bal.rep` | fragmentation | 오래된 free block이 남는 패턴을 관찰 | `./mdriver -V -l -f traces/custom/valid/free-order-fifo-bal.rep` | pass | hand-crafted |
| `large-near-limit-bal.rep` | stress | live payload를 약 15.6 MiB까지 올려 `MAX_HEAP` 근처 대형 요청을 확인 | `./mdriver -V -l -f traces/custom/valid/large-near-limit-bal.rep` | pass | hand-crafted, near-limit |
| `stress-random-s17-bal.rep` | stress | seed `17`, `2048` ids, max request `4096B`의 일반 랜덤 churn | `./mdriver -V -l -f traces/custom/valid/stress-random-s17-bal.rep` | pass | `gen_random_suite.pl` 생성 |
| `stress-random-s29-bal.rep` | stress | seed `29`, `4096` ids, max request `1024B`의 장기 랜덤 churn | `./mdriver -V -l -f traces/custom/valid/stress-random-s29-bal.rep` | pass | `gen_random_suite.pl` 생성 |
| `stress-bias-small-s41-bal.rep` | stress | 요청의 대부분을 `1~64B`로 치우친 small-hotset 시나리오 | `./mdriver -V -l -f traces/custom/valid/stress-bias-small-s41-bal.rep` | pass | `gen_biased_suite.pl` 생성 |
| `stress-bias-large-s53-bal.rep` | stress | large anchor와 small churn을 섞은 large-biased 시나리오 | `./mdriver -V -l -f traces/custom/valid/stress-bias-large-s53-bal.rep` | pass | `gen_biased_suite.pl` 생성 |
| `stress-fragment-wave-s67-bal.rep` | stress | 홀수/짝수 wave 해제로 외부 단편화를 반복 유발 | `./mdriver -V -l -f traces/custom/valid/stress-fragment-wave-s67-bal.rep` | pass | `gen_fragment_suite.pl` 생성 |
| `stress-realloc-heavy-s79-bal.rep` | stress | realloc 비중이 높은 장기 trace | `./mdriver -V -l -f traces/custom/valid/stress-realloc-heavy-s79-bal.rep` | pass | `gen_realloc_suite.pl` 생성 |
| `stress-long-lived-s97-bal.rep` | stress | `64`개 장수명 anchor와 수천 개 단수명 요청을 혼합 | `./mdriver -V -l -f traces/custom/valid/stress-long-lived-s97-bal.rep` | pass | `gen_biased_suite.pl` 생성 |

## Behavioral Traces

| File | Class | Purpose | Command | Expected Result | Notes |
| --- | --- | --- | --- | --- | --- |
| `probe-zero-malloc.rep` | behavioral | `malloc(0)` 요청 관찰 | `./mdriver -V -f traces/custom/behavioral/probe-zero-malloc.rep` | expected runtime failure | stock `mdriver`는 `size > 0`을 가정 |
| `probe-zero-realloc.rep` | behavioral | `realloc(ptr, 0)` 요청 관찰 | `./mdriver -V -f traces/custom/behavioral/probe-zero-realloc.rep` | expected runtime failure | stock `mdriver`와 충돌 가능 |
| `probe-oom-single.rep` | behavioral | 단일 대형 요청으로 `MAX_HEAP` 초과를 유도 | `./mdriver -V -f traces/custom/behavioral/probe-oom-single.rep` | expected runtime failure | `mem_sbrk` OOM 기대 |
| `probe-oom-growth.rep` | behavioral | 누적 할당으로 `MAX_HEAP` 초과를 유도 | `./mdriver -V -f traces/custom/behavioral/probe-oom-growth.rep` | expected runtime failure | 성장 과정 관찰용 |
| `probe-realloc-oom.rep` | behavioral | 대형 realloc로 OOM을 유도 | `./mdriver -V -f traces/custom/behavioral/probe-realloc-oom.rep` | expected runtime failure | realloc failure probe |

## Negative Traces

| File | Class | Purpose | Command | Expected Result | Notes |
| --- | --- | --- | --- | --- | --- |
| `invalid-double-free.rep` | negative | 동일 ID를 두 번 free | `perl traces/checktrace.pl < traces/custom/negative/invalid-double-free.rep` | expected validator rejection | allocator misuse |
| `invalid-free-unallocated.rep` | negative | alloc 없이 free | `perl traces/checktrace.pl < traces/custom/negative/invalid-free-unallocated.rep` | expected validator rejection | allocator misuse |
| `invalid-realloc-before-alloc.rep` | negative | alloc 없이 realloc | `perl traces/checktrace.pl < traces/custom/negative/invalid-realloc-before-alloc.rep` | expected validator rejection | allocator misuse |
| `invalid-reused-id.rep` | negative | free 없이 동일 ID를 다시 할당 | `perl traces/checktrace.pl < traces/custom/negative/invalid-reused-id.rep` | expected validator rejection | ID discipline |
| `invalid-opcode.rep` | negative | 정의되지 않은 opcode 포함 | `./mdriver -V -f traces/custom/negative/invalid-opcode.rep` | expected parser rejection | bogus opcode |
| `invalid-numids-small.rep` | negative | header의 `num_ids`보다 큰 ID 사용 | `./mdriver -V -f traces/custom/negative/invalid-numids-small.rep` | expected parser rejection | `max_index` assert 기대 |
| `invalid-numids-large.rep` | negative | header의 `num_ids`가 실제 max id보다 큼 | `./mdriver -V -f traces/custom/negative/invalid-numids-large.rep` | expected parser rejection | `max_index` assert 기대 |
| `invalid-numops-large.rep` | negative | header의 `num_ops`가 실제 행 수보다 큼 | `./mdriver -V -f traces/custom/negative/invalid-numops-large.rep` | expected parser rejection | `num_ops` assert 기대 |
