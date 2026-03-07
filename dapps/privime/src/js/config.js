'use strict';

// ================================================================
// CONFIG
// ================================================================
export var PRIVIME_CID = '{{CID}}';
export var GROTH_PER_BEAM = 100000000;
export var MAX_MSG_CHARS  = 950;   // 1024 byte limit with JSON overhead
export var MSG_REFRESH_MS = 2000; // 2s poll for new messages (read_messages is a local wallet call, cheap)

// File sharing
export var MAX_FILE_SIZE     = 5 * 1024 * 1024;   // 5 MB
export var IPFS_ADD_TIMEOUT  = 60000;              // 60s upload timeout
export var IPFS_GET_TIMEOUT  = 30000;              // 30s download timeout
export var MAX_FILENAME_LEN  = 60;                 // truncate in SBBS message
export var AUTO_DL_MAX_SIZE  = 2 * 1024 * 1024;    // auto-download images under 2 MB
export var ALLOWED_MIME_TYPES = [
    'image/jpeg', 'image/png', 'image/gif', 'image/webp',
    'application/pdf', 'text/plain'
];
