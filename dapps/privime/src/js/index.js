'use strict';

// ================================================================
// ENTRY POINT — imports all modules and wires up window.* exports
// ================================================================

// Config (no window exports needed)
import './config.js';

// State (no window exports needed — accessed via module imports)
import './state.js';

// Helpers
import { copyToClipboard, closeFileViewer, viewerCopyOrSave } from './helpers.js';

// Wallet API (no direct window exports)
import './wallet-api.js';

// Shader (no direct window exports)
import './shader.js';

// SBBS (no direct window exports)
import './sbbs.js';

// Storage (no direct window exports)
import './storage.js';

// Block user (must import before storage's loadFromStorage is called)
import { toggleBlockUser, clearChatHistory, deleteChatFromKebab } from './block-user.js';

// Registration
import { onHandleInput, onWalletIdInput, tryAutoFillWalletId, doRegisterHandle } from './registration.js';

// Contacts (no direct window exports)
import './contacts.js';

// Render modules (no direct window exports — called internally)
import './render-home.js';
import './render-chat.js';
import './render-profile.js';

// Navigation
import { showHome, showRegisterPage, showChatPage, showContactProfile,
         showMyProfile, profileBack, openChatFromProfile,
         copyProfileWalletId } from './navigation.js';

// Chat input
import { onChatInput, onChatKeydown, doSendMessage, cancelReply } from './chat-input.js';

// Chat search
import { toggleChatSearch, onChatSearchInput, onChatSearchKeydown,
         chatSearchPrev, chatSearchNext, closeChatSearch } from './chat-search.js';

// Context menus
import { showContactContextMenu, deleteChat, showBubbleMenu,
         doForwardTo, closeForwardModal, onForwardSearch,
         doCopyBubble, doReplyBubble, doForwardBubble, doDeleteBubble,
         setupGlobalListeners } from './context-menus.js';

// Emoji
import { toggleEmojiPanel, renderEmojiTab, selectEmoji } from './emoji.js';

// Kebab
import { toggleKebabMenu, viewChatProfile, exportChatHistory } from './kebab.js';

// Slash commands
import { toggleSlashPopup, selectSlashCommand } from './slash-commands.js';

// New chat
import { promptNewChat, closeNewChatModal, onNewChatInput, startNewChat } from './new-chat.js';

// Search bar
import { onSearchInput, clearSearch } from './search-bar.js';

// Profile actions
import { showEditDisplayName, hideEditDisplayName, doSaveDisplayName,
         showUpdateAddr, hideUpdateAddr, doUpdateAddress,
         confirmReleaseHandle, onUpdateWalletIdInput,
         tryAutoFillUpdateAddr, showConfirmModal } from './profile-actions.js';

// Wallet connection
import { detectAndConnect } from './wallet-connect.js';

// File sharing
import { openFilePicker, handleFileSelection, onFileAction,
         openLightbox, closeLightbox, copyLightboxImage,
         cancelFileAttachment, doSaveFileBubble,
         onChatDragOver, onChatDragEnter, onChatDragLeave, onChatDrop } from './file-sharing.js';

// Crypto (no direct window exports)
import './crypto.js';

// Startup (side-effect: registers event handlers with wallet-api.js)
import './startup.js';

// ================================================================
// WINDOW EXPORTS — functions referenced from HTML onclick/oninput/oncontextmenu
// ================================================================

// Navigation
window.showHome = showHome;
window.showRegisterPage = showRegisterPage;
window.showChatPage = showChatPage;
window.showContactProfile = showContactProfile;
window.showMyProfile = showMyProfile;
window.profileBack = profileBack;
window.openChatFromProfile = openChatFromProfile;
window.copyProfileWalletId = copyProfileWalletId;

// Search bar (home page)
window.onSearchInput = onSearchInput;
window.clearSearch = clearSearch;

// Registration
window.onHandleInput = onHandleInput;
window.onWalletIdInput = onWalletIdInput;
window.tryAutoFillWalletId = tryAutoFillWalletId;
window.doRegisterHandle = doRegisterHandle;

// New chat modal
window.promptNewChat = promptNewChat;
window.closeNewChatModal = closeNewChatModal;
window.onNewChatInput = onNewChatInput;
window.startNewChat = startNewChat;

// Chat search
window.toggleChatSearch = toggleChatSearch;
window.onChatSearchInput = onChatSearchInput;
window.onChatSearchKeydown = onChatSearchKeydown;
window.chatSearchPrev = chatSearchPrev;
window.chatSearchNext = chatSearchNext;
window.closeChatSearch = closeChatSearch;

// Kebab menu
window.toggleKebabMenu = toggleKebabMenu;
window.viewChatProfile = viewChatProfile;
window.exportChatHistory = exportChatHistory;

// Block / delete
window.toggleBlockUser = toggleBlockUser;
window.clearChatHistory = clearChatHistory;
window.deleteChatFromKebab = deleteChatFromKebab;
window.deleteChat = deleteChat;

// Chat input
window.doSendMessage = doSendMessage;
window.onChatInput = onChatInput;
window.onChatKeydown = onChatKeydown;
window.cancelReply = cancelReply;

// Emoji
window.toggleEmojiPanel = toggleEmojiPanel;
window.renderEmojiTab = renderEmojiTab;
window.selectEmoji = selectEmoji;

// Slash commands
window.toggleSlashPopup = toggleSlashPopup;
window.selectSlashCommand = selectSlashCommand;

// Contact context menu
window.showContactContextMenu = showContactContextMenu;

// Bubble context menu
window.showBubbleMenu = showBubbleMenu;
window.doCopyBubble = doCopyBubble;
window.doReplyBubble = doReplyBubble;
window.doForwardBubble = doForwardBubble;
window.doDeleteBubble = doDeleteBubble;

// Forward modal
window.doForwardTo = doForwardTo;
window.closeForwardModal = closeForwardModal;
window.onForwardSearch = onForwardSearch;

// Profile actions
window.showEditDisplayName = showEditDisplayName;
window.hideEditDisplayName = hideEditDisplayName;
window.doSaveDisplayName = doSaveDisplayName;
window.showUpdateAddr = showUpdateAddr;
window.hideUpdateAddr = hideUpdateAddr;
window.doUpdateAddress = doUpdateAddress;
window.confirmReleaseHandle = confirmReleaseHandle;
window.onUpdateWalletIdInput = onUpdateWalletIdInput;
window.tryAutoFillUpdateAddr = tryAutoFillUpdateAddr;

// File sharing
window.openFilePicker = openFilePicker;
window._handleFileSelection = handleFileSelection;
window.onFileAction = onFileAction;
window.openLightbox = openLightbox;
window.closeLightbox = closeLightbox;
window.copyLightboxImage = copyLightboxImage;
window.cancelFileAttachment = cancelFileAttachment;
window.doSaveFileBubble = doSaveFileBubble;
window.onChatDragOver = onChatDragOver;
window.onChatDragEnter = onChatDragEnter;
window.onChatDragLeave = onChatDragLeave;
window.onChatDrop = onChatDrop;

// File viewer
window.closeFileViewer = closeFileViewer;
window.viewerCopyOrSave = viewerCopyOrSave;

// Helpers (used in dynamic innerHTML)
window.copyToClipboard = copyToClipboard;

// ================================================================
// INIT
// ================================================================
function init() {
    setupGlobalListeners();
    detectAndConnect();
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}
