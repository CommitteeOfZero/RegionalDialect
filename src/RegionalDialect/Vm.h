#pragma once

#include "Hook.h"

namespace rd {
namespace vm {

#pragma pack(4)
struct ScriptThreadState {
  /* 0000 */ int accumulator;
  /* 0004 */ char gap4[16];
  /* 0014 */ unsigned int thread_group_id;
  /* 0018 */ unsigned int sleep_timeout;
  /* 001C */ char gap28[8];
  /* 0024 */ unsigned int loop_counter;
  /* 0028 */ unsigned int loop_target_label_id;
  /* 002C */ unsigned int call_stack_depth;
  /* 0030 */ unsigned int ret_address_ids[8];
  /* 0050 */ unsigned int ret_address_script_buffer_ids[8];
  /* 0070 */ int thread_id;
  /* 0074 */ int script_buffer_id;
  /* 0078 */ char gap120[68];
  /* 00BC */ int thread_local_variables[32];
  /* 013C */ int somePageNumber;
  /* 0140 */ ScriptThreadState *next_context;
  /* 0144 */ ScriptThreadState *prev_context;
  /* 0148 */ ScriptThreadState *next_free_context;
  /* 014C */ void *pc;
};

DECLARE_HOOK(CalMain, void, ScriptThreadState *param_1, int32_t *param_2);

void Init();

}  // namespace vm
}  // namespace rd