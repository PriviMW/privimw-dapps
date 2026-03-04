'use strict';

import { EMOJI_DATA, emojiPanelOpen, setEmojiPanelOpen } from './state.js';
import { onChatInput } from './chat-input.js';

// ================================================================
// EMOJI PICKER
// ================================================================
export function toggleEmojiPanel() {
    var panel = document.getElementById('emojiPanel');
    if (emojiPanelOpen) {
        panel.classList.remove('active');
        setEmojiPanelOpen(false);
        return;
    }
    setEmojiPanelOpen(true);
    panel.classList.add('active');
    // Build tabs if not already built
    var tabs = document.getElementById('emojiTabs');
    if (!tabs.children.length) {
        var keys = Object.keys(EMOJI_DATA);
        tabs.innerHTML = keys.map(function(k, i) {
            return '<button class="emoji-tab' + (i === 0 ? ' active' : '') + '" onclick="renderEmojiTab(\'' + k + '\',this)">' + k + '</button>';
        }).join('');
        renderEmojiTab(keys[0], null);
    }
}

export function renderEmojiTab(tabKey, tabBtn) {
    var grid = document.getElementById('emojiGrid');
    var emojis = EMOJI_DATA[tabKey] || [];
    grid.innerHTML = emojis.map(function(e) {
        return '<span onclick="selectEmoji(this.textContent)">' + e + '</span>';
    }).join('');
    // Update active tab
    if (tabBtn) {
        var tabs = document.getElementById('emojiTabs').children;
        for (var i = 0; i < tabs.length; i++) tabs[i].classList.remove('active');
        tabBtn.classList.add('active');
    }
}

export function selectEmoji(emoji) {
    var input = document.getElementById('chatInput');
    var start = input.selectionStart;
    var end = input.selectionEnd;
    var val = input.value;
    input.value = val.substring(0, start) + emoji + val.substring(end);
    input.selectionStart = input.selectionEnd = start + emoji.length;
    input.focus();
    onChatInput();
}

export function closeEmojiPanel() {
    if (emojiPanelOpen) {
        document.getElementById('emojiPanel').classList.remove('active');
        setEmojiPanelOpen(false);
    }
}
