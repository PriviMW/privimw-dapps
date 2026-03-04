'use strict';

// ================================================================
// GLOBAL STATE
// ================================================================

// Wallet API
export var BEAM = null;
export function setBEAM(v) { BEAM = v; }

export var apiCallbacks  = new Map();
export var callIdCounter = 0;
export function setCallIdCounter(v) { callIdCounter = v; }
export function incrementCallId() { return ++callIdCounter; }

export var shaderBytes   = null;
export function setShaderBytes(v) { shaderBytes = v; }

export var isConnected   = false;
export function setConnected(v) { isConnected = v; }

export var initLock      = false;
export function setInitLock(v) { initLock = v; }

// Navigation
export var currentPage  = 'splash';
export function setCurrentPage(v) { currentPage = v; }

export var activeChat   = null;   // walletIdHex of open chat (null = none)
export function setActiveChat(v) { activeChat = v; }

export var profileFrom  = 'home'; // which page to return to from profile
export function setProfileFrom(v) { profileFrom = v; }

// Identity
export var myHandle     = null;   // { handle, wallet_id, display_name, registered_height }
export function setMyHandle(v) { myHandle = v; }

export var myWalletId   = null;   // my own SBBS wallet address (hex string, 66 or 68 chars)
export function setMyWalletId(v) { myWalletId = v; }

// Messaging
export var contacts      = {};    // { walletIdHex: { handle, display_name } }
export function setContacts(v) { contacts = v; }

export var conversations = {};    // { walletIdHex: [{ text, ts, sent }] }
export function setConversations(v) { conversations = v; }

export var deletedConvs  = {};    // { convKey: maxTs } -- tombstones so deleted chats don't re-import
export function setDeletedConvs(v) { deletedConvs = v; }

export var allMessages   = [];    // raw from read_messages
export function setAllMessages(v) { allMessages = v; }

export var unreadCounts  = {};    // { walletIdHex: count }
export function setUnreadCounts(v) { unreadCounts = v; }

export var msgPollTimer  = null;
export function setMsgPollTimer(v) { msgPollTimer = v; }

export var isSubscribed  = false;
export function setSubscribed(v) { isSubscribed = v; }

// Contract params
export var registrationFee = 1; // BEAM, updated from view_pool
export function setRegistrationFee(v) { registrationFee = v; }

// Lookup debounce
export var handleCheckTimer  = null;
export function setHandleCheckTimer(v) { handleCheckTimer = v; }

export var newChatCheckTimer = null;
export function setNewChatCheckTimer(v) { newChatCheckTimer = v; }

export var searchTimer       = null;
export function setSearchTimer(v) { searchTimer = v; }

export var newChatResolved   = null; // { handle, wallet_id, display_name } or null
export function setNewChatResolved(v) { newChatResolved = v; }

// Search
export var searchDebounceTimer = null;
export function setSearchDebounceTimer(v) { searchDebounceTimer = v; }

// Emoji picker
export var emojiPanelOpen = false;
export function setEmojiPanelOpen(v) { emojiPanelOpen = v; }

export var EMOJI_DATA = {
    '\u{1F600}': ['\u{1F600}','\u{1F603}','\u{1F604}','\u{1F601}','\u{1F606}','\u{1F605}','\u{1F923}','\u{1F602}','\u{1F642}','\u{1F60A}','\u{1F607}','\u{1F970}','\u{1F60D}','\u{1F929}','\u{1F618}','\u{1F617}','\u{1F60B}','\u{1F61B}','\u{1F61C}','\u{1F92A}','\u{1F61D}','\u{1F911}','\u{1F917}','\u{1F92D}','\u{1F92B}','\u{1F914}','\u{1F610}','\u{1F611}','\u{1F636}','\u{1F60F}','\u{1F612}','\u{1F644}','\u{1F62C}','\u{1F62E}','\u{1F62F}','\u{1F632}','\u{1F633}','\u{1F97A}','\u{1F622}','\u{1F62D}','\u{1F624}','\u{1F621}','\u{1F92C}','\u{1F608}','\u{1F480}','\u{1F4A9}','\u{1F921}','\u{1F47B}','\u{1F47D}','\u{1F916}'],
    '\u{1F44D}': ['\u{1F44B}','\u{1F91A}','\u270B','\u{1F590}','\u{1F44C}','\u{1F90C}','\u{1F90F}','\u270C\uFE0F','\u{1F91E}','\u{1FAF0}','\u{1F91F}','\u{1F918}','\u{1F919}','\u{1F448}','\u{1F449}','\u{1F446}','\u{1F447}','\u261D\uFE0F','\u{1F44D}','\u{1F44E}','\u270A','\u{1F44A}','\u{1F91B}','\u{1F91C}','\u{1F44F}','\u{1F64C}','\u{1FAF6}','\u{1F450}','\u{1F932}','\u{1F64F}','\u{1F4AA}','\u{1F9BE}'],
    '\u2764\uFE0F': ['\u2764\uFE0F','\u{1F9E1}','\u{1F49B}','\u{1F49A}','\u{1F499}','\u{1F49C}','\u{1F5A4}','\u{1F90D}','\u{1F90E}','\u{1F494}','\u2763\uFE0F','\u{1F495}','\u{1F49E}','\u{1F493}','\u{1F497}','\u{1F496}','\u{1F498}','\u{1F49D}','\u{1F49F}','\u{1F525}','\u2B50','\u2728','\u{1F4AB}','\u{1F31F}','\u{1F4A5}','\u{1F4A2}','\u{1F4A4}','\u{1F4AC}','\u{1F4AD}','\u{1F573}\uFE0F','\u{1F4A3}','\u{1F4AE}'],
    '\u{1F431}': ['\u{1F436}','\u{1F431}','\u{1F42D}','\u{1F439}','\u{1F430}','\u{1F98A}','\u{1F43B}','\u{1F43C}','\u{1F428}','\u{1F42F}','\u{1F981}','\u{1F42E}','\u{1F437}','\u{1F438}','\u{1F435}','\u{1F648}','\u{1F649}','\u{1F64A}','\u{1F412}','\u{1F414}','\u{1F427}','\u{1F426}','\u{1F424}','\u{1F986}','\u{1F985}','\u{1F989}','\u{1F987}','\u{1F43A}','\u{1F417}','\u{1F434}','\u{1F984}','\u{1F41D}'],
    '\u{1F355}': ['\u{1F34E}','\u{1F350}','\u{1F34A}','\u{1F34B}','\u{1F34C}','\u{1F349}','\u{1F347}','\u{1F353}','\u{1FAD0}','\u{1F352}','\u{1F351}','\u{1F96D}','\u{1F34D}','\u{1F965}','\u{1F95D}','\u{1F345}','\u{1F346}','\u{1F951}','\u{1F966}','\u{1F96C}','\u{1F336}\uFE0F','\u{1FAD1}','\u{1F952}','\u{1F33D}','\u{1F955}','\u{1FAD2}','\u{1F9C4}','\u{1F9C5}','\u{1F344}','\u{1F95C}','\u{1F35E}','\u{1F9C0}','\u{1F356}','\u{1F355}','\u{1F354}','\u{1F35F}','\u{1F32D}','\u{1F37F}','\u{1F9C2}','\u{1F964}','\u{1F37A}','\u{1F377}','\u{1F942}','\u{1F37E}','\u2615'],
    '\u26BD': ['\u26BD','\u{1F3C0}','\u{1F3C8}','\u26BE','\u{1F3BE}','\u{1F3D0}','\u{1F3B1}','\u{1F3D3}','\u{1F3F8}','\u{1F3D2}','\u{1F94A}','\u{1F3AF}','\u26F3','\u{1F3BF}','\u{1F6F9}','\u{1F3AE}','\u{1F579}\uFE0F','\u{1F3B0}','\u{1F3B2}','\u{1F9E9}','\u265F\uFE0F','\u{1F3AD}','\u{1F3A8}','\u{1F3B5}','\u{1F3B6}','\u{1F3A4}','\u{1F3A7}','\u{1F3B8}','\u{1F3B9}','\u{1F941}','\u{1F3B7}','\u{1F3BA}','\u{1F3BB}','\u{1F3C6}','\u{1F947}','\u{1F948}','\u{1F949}','\u{1F3C5}','\u{1F396}\uFE0F','\u{1F397}\uFE0F']
};

// Bubble context menu
export var chatMsgCache = [];   // mirrors rendered messages for context menu lookup
export function setChatMsgCache(v) { chatMsgCache = v; }

export var bubbleCtxMsg = null; // message object from right-click
export function setBubbleCtxMsg(v) { bubbleCtxMsg = v; }

export var bubbleCtxWid = null; // walletId of the chat at right-click time
export function setBubbleCtxWid(v) { bubbleCtxWid = v; }

// Reply / forward state
export var replyTo     = null;  // { text } when replying, null otherwise
export function setReplyTo(v) { replyTo = v; }

export var forwardMode = false;
export function setForwardMode(v) { forwardMode = v; }

export var forwardText = null;
export function setForwardText(v) { forwardText = v; }
