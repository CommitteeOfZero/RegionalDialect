#define MAX_PROCESSED_STRING_LENGTH 2000
#define GLYPH_ID_FULLWIDTH_SPACE 0
#define GLYPH_ID_HALFWIDTH_SPACE 63
#define NOT_A_LINK 0xFF

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
#pragma pack(0)

typedef struct {
  int lines;
  int length;
  int textureStartX[MAX_PROCESSED_STRING_LENGTH];
  int textureStartY[MAX_PROCESSED_STRING_LENGTH];
  int textureWidth[MAX_PROCESSED_STRING_LENGTH];
  int textureHeight[MAX_PROCESSED_STRING_LENGTH];
  int displayStartX[MAX_PROCESSED_STRING_LENGTH];
  int displayStartY[MAX_PROCESSED_STRING_LENGTH];
  int displayEndX[MAX_PROCESSED_STRING_LENGTH];
  int displayEndY[MAX_PROCESSED_STRING_LENGTH];
  int color[MAX_PROCESSED_STRING_LENGTH];
  int glyph[MAX_PROCESSED_STRING_LENGTH];
  uint8_t linkNumber[MAX_PROCESSED_STRING_LENGTH];
  int linkCharCount;
  int linkCount;
  int curLinkNumber;
  int curColor;
  int usedLineLength;
  bool error;
  char text[MAX_PROCESSED_STRING_LENGTH];
} ProcessedSc3String_t;

typedef struct {
  int8_t* start;
  int8_t* end;
  uint16_t cost;
  bool startsWithSpace;
  bool endsWithLinebreak;
} StringWord_t;