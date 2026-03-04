'use strict';

import { contacts, searchDebounceTimer, setSearchDebounceTimer } from './state.js';
import { escHtml, escAttr } from './helpers.js';
import { privimeInvoke } from './shader.js';

// ================================================================
// SEARCH BAR
// ================================================================
export function clearSearch() {
    var input = document.getElementById('searchInput');
    if (input) input.value = '';
    document.getElementById('searchResults').style.display = 'none';
    document.getElementById('contactList').style.display = '';
    document.getElementById('searchClearBtn').style.display = 'none';
    if (searchDebounceTimer) { clearTimeout(searchDebounceTimer); setSearchDebounceTimer(null); }
}

export function onSearchInput(val) {
    var results = document.getElementById('searchResults');
    var list    = document.getElementById('contactList');
    var clearBtn = document.getElementById('searchClearBtn');

    if (!val.trim()) {
        results.style.display = 'none';
        list.style.display = '';
        clearBtn.style.display = 'none';
        if (searchDebounceTimer) { clearTimeout(searchDebounceTimer); setSearchDebounceTimer(null); }
        return;
    }

    clearBtn.style.display = '';
    var q = val.trim().replace(/^@/, '').toLowerCase();
    if (!q) return;

    results.style.display = '';
    list.style.display = 'none';

    // 1. Instant local results -- filter existing contacts by handle + display_name
    var localHtml = [];
    var localHandles = {};
    Object.keys(contacts).forEach(function(key) {
        if (!key.startsWith('@')) return;
        var c = contacts[key];
        var handle = (c.handle || key.slice(1)).toLowerCase();
        var dname  = (c.display_name || '').toLowerCase();
        if (handle.indexOf(q) !== -1 || dname.indexOf(q) !== -1) {
            localHandles[handle] = true;
            var initial = handle.charAt(0).toUpperCase();
            var label = c.display_name || '@' + escHtml(c.handle || key.slice(1));
            localHtml.push(
                '<div class="search-result-item" onclick="showChatPage(\'' + escAttr(key) + '\')">' +
                '<div class="contact-avatar" style="width:40px;height:40px;font-size:16px;">' + initial + '</div>' +
                '<div><div class="search-result-name">' + escHtml(label) + '</div>' +
                '<div class="search-result-sub">@' + escHtml(c.handle || key.slice(1)) + '</div></div></div>'
            );
        }
    });

    var chainLabel = '<div id="searchChainStatus" style="padding:8px 16px;font-size:12px;color:var(--text-muted);">Searching on-chain...</div>';
    results.innerHTML = localHtml.join('') + chainLabel;

    if (localHtml.length === 0 && !results.querySelector('#searchChainStatus')) {
        results.innerHTML = chainLabel;
    }

    // 2. Debounced on-chain prefix search
    if (searchDebounceTimer) clearTimeout(searchDebounceTimer);
    setSearchDebounceTimer(setTimeout(function() {
        var currentVal = document.getElementById('searchInput').value.trim().replace(/^@/, '').toLowerCase();
        if (currentVal !== q) return; // stale
        privimeInvoke('user', 'search_handles', { prefix: q }, function(result) {
            if (document.getElementById('searchInput').value.trim().replace(/^@/, '').toLowerCase() !== q) return;
            var statusEl = document.getElementById('searchChainStatus');
            if (!statusEl) return;
            var matches = result && result.results ? result.results : [];
            if (Array.isArray(result)) matches = result;
            var chainHtml = [];
            if (matches.length > 0) {
                matches.forEach(function(r) {
                    var handle = (r.handle || '').replace(/\0/g, '').toLowerCase();
                    if (!handle || localHandles[handle]) return; // skip already shown
                    var hKey = '@' + handle;
                    contacts[hKey] = contacts[hKey] || {};
                    contacts[hKey].handle = handle;
                    if (r.wallet_id) contacts[hKey].wallet_id = r.wallet_id;
                    if (r.display_name) contacts[hKey].display_name = r.display_name;
                    var initial = handle.charAt(0).toUpperCase();
                    var label = r.display_name || '@' + escHtml(handle);
                    chainHtml.push(
                        '<div class="search-result-item" onclick="showChatPage(\'' + escAttr(hKey) + '\')">' +
                        '<div class="contact-avatar" style="width:40px;height:40px;font-size:16px;">' + initial + '</div>' +
                        '<div><div class="search-result-name">' + escHtml(label) + '</div>' +
                        '<div class="search-result-sub">@' + escHtml(handle) + '</div></div></div>'
                    );
                });
            }
            if (chainHtml.length > 0) {
                statusEl.outerHTML = chainHtml.join('');
            } else if (localHtml.length > 0) {
                statusEl.remove();
            } else {
                statusEl.textContent = 'No results for @' + q;
            }
        });
    }, 500));
}
