/*
  This file is part of MAMBO, a low-overhead dynamic binary modification tool:
      https://github.com/beehive-lab/mambo

  Copyright 2013-2016 Cosmin Gorgovan <cosmin at linux-geek dot org>

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#ifndef __DBM_H__
#define __DBM_H__

#include <stdbool.h>

#include "pie/pie-arm-decoder.h"
#include "pie/pie-thumb-decoder.h"

#include "common.h"
#include "util.h"

/* Various parameters which can be tuned */

// BASIC_BLOCK_SIZE should be a power of 2
#define BASIC_BLOCK_SIZE 64
#ifdef DBM_TRACES
  #define CODE_CACHE_SIZE 55000
#else
  #define CODE_CACHE_SIZE 65000
#endif
#define TRACE_FRAGMENT_NO 60000
#define CODE_CACHE_OVERP 30
#define MAX_BRANCH_RANGE (16*1024*1024)
#define TRACE_CACHE_SIZE (MAX_BRANCH_RANGE - (CODE_CACHE_SIZE*BASIC_BLOCK_SIZE * 4))
#define TRACE_LIMIT_OFFSET (1024)

#define TRACE_ALIGN 4 // must be a power of 2
#define TRACE_ALIGN_MASK (TRACE_ALIGN-1)

#define INST_CNT 400

#define MAX_TB_INDEX  152
#define TB_CACHE_SIZE 32

#define MAX_BACK_INLINE 5
#define MAX_TRACE_FRAGMENTS 20

#define RAS_SIZE (4096*5)
#define TBB_TARGET_REACHED_SIZE 30

#define MAX_CC_LINKS 100000

#define THUMB 0x1
#define FULLADDR 0x2

#define MAX_PLUGIN_NO (10)

typedef enum {
  mambo_bb = 0,
  mambo_trace,
  mambo_trace_entry
} cc_type;

typedef enum {
  unknown,
  uncond_b_to_bl_thumb,
  uncond_imm_thumb,
  uncond_reg_thumb,
  cond_imm_thumb,
  cond_reg_thumb,
  cbz_thumb,
  uncond_blxi_thumb,
  cond_blxi_thumb,
  cond_imm_arm,
  uncond_imm_arm,
  cond_reg_arm,
  uncond_reg_arm,
  uncond_blxi_arm,
  cond_blxi_arm,
  tbb,
  tbh,
  tb_indirect,
  stub,
  pred_bxlr,
  pred_pop16pc,
  pred_ldmfd32pc,
  pred_armbxlr,
  pred_ldrpcsp,
  pred_armldmpc,
  pred_bad,
  trace_inline_max,
} branch_type;

typedef struct {
  uint32_t words[BASIC_BLOCK_SIZE];
} dbm_block;

typedef struct {
  dbm_block blocks[CODE_CACHE_SIZE];
  uint8_t  traces[TRACE_CACHE_SIZE];
} dbm_code_cache;

typedef struct {
  uint16_t *source_addr;
  branch_type exit_branch_type;
  uint16_t *exit_branch_addr;
  uint32_t branch_taken_addr;
  uint32_t branch_skipped_addr;
  uint32_t branch_condition;
  uint32_t branch_cache_status;
  uint32_t rn;
  uint32_t free_b;
  ll_entry *linked_from;
} dbm_code_cache_meta;

typedef struct {
  unsigned long flags;
  void *child_stack;
  unsigned long *ptid;
  unsigned long tls;
  unsigned long *ctid;
} sys_clone_args;

typedef struct {
  int free_block;
  uint32_t dispatcher_addr;
  uint32_t syscall_wrapper_addr;
  uint32_t scratch_regs[3];
  uint32_t parent_scratch_regs[3];
  bool is_vfork_child;

  dbm_code_cache *code_cache;
  dbm_code_cache_meta code_cache_meta[CODE_CACHE_SIZE + TRACE_FRAGMENT_NO];
  hash_table entry_address;
#ifdef DBM_TRACES
  hash_table trace_entry_address;

  uint8_t   exec_count[CODE_CACHE_SIZE];
  uint32_t  trace_head_incr_addr;
  uint8_t  *trace_cache_next;
  int       trace_id;
  int       trace_fragment_count;
#endif

  ll *cc_links;

  uint32_t tls;
  uint32_t child_tls;

#ifdef PLUGINS_NEW
  void *plugin_priv[MAX_PLUGIN_NO];
#endif
  void *clone_ret_addr;
  volatile int tid;
  sys_clone_args *clone_args;
  bool clone_vm;
} dbm_thread;

typedef enum {
  ARM_INST,
  THUMB_INST
} inst_set;

#include "api/plugin_support.h"

typedef struct {
  uint32_t disp_thread_data_off;
#ifdef PLUGINS_NEW
  int free_plugin;
  mambo_plugin plugins[MAX_PLUGIN_NO];
#endif
} dbm_global;

void dbm_exit(dbm_thread *thread_data, uint32_t code);

extern void dispatcher_trampoline();
extern void syscall_wrapper();
extern void* end_of_dispatcher_s;
extern void th_to_arm();
extern void th_enter(void *stack);

bool allocate_thread_data(dbm_thread **thread_data);
void init_thread(dbm_thread *thread_data);
uint32_t lookup_or_scan(dbm_thread *thread_data, uint32_t target, bool *cached);
uint32_t lookup_or_stub(dbm_thread *thread_data, uint32_t target);
uint32_t scan(dbm_thread *thread_data, uint16_t *address, int basic_block);
uint32_t scan_arm(dbm_thread *thread_data, uint32_t *read_address, int basic_block, cc_type type, uint32_t *write_p);
uint32_t scan_thumb(dbm_thread *thread_data, uint16_t *read_address, int basic_block, cc_type type, uint16_t *write_p);
int allocate_bb(dbm_thread *thread_data);
void trace_dispatcher(uint32_t target, uint32_t *next_addr, uint32_t source_index, dbm_thread *thread_data);
void flush_code_cache(dbm_thread *thread_data);

void thumb_encode_stub_bb(dbm_thread *thread_data, int basic_block, uint32_t target);
void arm_encode_stub_bb(dbm_thread *thread_data, int basic_block, uint32_t target);

int addr_to_bb_id(dbm_thread *thread_data, uint32_t addr);
void record_cc_link(dbm_thread *thread_data, uint32_t linked_from, uint32_t linked_to_addr);
bool is_bb(dbm_thread *thread_data, uint32_t addr);

extern dbm_global global_data;
extern dbm_thread *disp_thread_data;
extern __thread dbm_thread *current_thread;

#ifdef PLUGINS_NEW
void set_mambo_context(mambo_context *ctx, dbm_thread *thread_data, inst_set inst_type,
                       cc_type fragment_type, int fragment_id, int inst, mambo_cond cond,
                       void *read_address, void *write_p, unsigned long *args);
void mambo_deliver_callbacks(unsigned cb_id, dbm_thread *thread_data, inst_set inst_type,
                             cc_type fragment_type, int fragment_id, int inst, mambo_cond cond,
                             void *read_address, void *write_p, unsigned long *regs);
#endif

/* Constants */

#define SYSCALL_EXIT         1
#define SYSCALL_CLOSE        6
#define SYSCALL_CLONE        120
#define SYSCALL_RT_SIGACTION 174
#define SYSCALL_EXIT_GROUP   248
#define SYSCALL_SET_TLS      0xF0005

#define ALLOCATE_BB 0

#endif
