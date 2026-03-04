'use strict';

// ================================================================
// CONFIG
// ================================================================
export var PRIVIME_CID = '{{CID}}';
export var GROTH_PER_BEAM = 100000000;
export var MAX_MSG_CHARS  = 950;   // 1024 byte limit with JSON overhead
export var MSG_REFRESH_MS = 2000; // 2s poll for new messages (read_messages is a local wallet call, cheap)
