'use strict';

import { newChatResolved, setNewChatResolved, newChatCheckTimer, setNewChatCheckTimer } from './state.js';
import { resolveHandleToContact } from './contacts.js';
import { showChatPage } from './navigation.js';

// ================================================================
// NEW CHAT MODAL
// ================================================================
var newChatCheckTimerLocal = null;

export function promptNewChat() {
    document.getElementById('newChatHandleInput').value = '';
    document.getElementById('newChatHint').textContent = 'Enter the @handle of the person you want to message';
    document.getElementById('newChatHint').className = 'form-hint';
    document.getElementById('newChatHandleWrap').className = 'form-input-wrap';
    document.getElementById('newChatStartBtn').disabled = true;
    setNewChatResolved(null);
    document.getElementById('newChatModal').classList.add('active');
    setTimeout(function() { document.getElementById('newChatHandleInput').focus(); }, 100);
}

export function closeNewChatModal() {
    document.getElementById('newChatModal').classList.remove('active');
}

export function onNewChatInput(val) {
    var hint = document.getElementById('newChatHint');
    var wrap = document.getElementById('newChatHandleWrap');
    var btn  = document.getElementById('newChatStartBtn');
    wrap.className = 'form-input-wrap';
    setNewChatResolved(null);
    btn.disabled = true;

    if (newChatCheckTimerLocal) clearTimeout(newChatCheckTimerLocal);
    val = val.toLowerCase().replace(/[^a-z0-9_]/g, '');
    document.getElementById('newChatHandleInput').value = val;

    if (val.length < 3) {
        hint.textContent = 'Enter at least 3 characters';
        hint.className = 'form-hint';
        return;
    }

    hint.textContent = 'Looking up @' + val + '...';
    hint.className = 'form-hint checking';

    newChatCheckTimerLocal = setTimeout(function() {
        resolveHandleToContact(val, function(err, contact) {
            if (err) {
                wrap.className = 'form-input-wrap invalid';
                hint.textContent = '\u2717 @' + val + ' not found';
                hint.className = 'form-hint taken';
            } else {
                wrap.className = 'form-input-wrap valid';
                hint.textContent = '\u2713 Found: ' + (contact.display_name || contact.handle);
                hint.className = 'form-hint available';
                setNewChatResolved(contact);
                btn.disabled = false;
            }
        });
    }, 500);
}

export function startNewChat() {
    if (!newChatResolved) return;
    var target = newChatResolved;
    closeNewChatModal();
    showChatPage('@' + target.handle);
}
