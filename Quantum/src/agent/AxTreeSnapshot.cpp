#include "agent/AxTreeSnapshot.h"

#include <nlohmann/json.hpp>

namespace quantum {

std::string buildAxTreeSnapshotScript() {
    return R"JS(
(() => {
  const isVisible = (el) => {
    const style = getComputedStyle(el);
    const rect = el.getBoundingClientRect();
    if (style.visibility === 'hidden' || style.display === 'none' || style.opacity === '0') return false;
    if (rect.width < 1 || rect.height < 1) return false;
    
    // Topmost check: verify if the element is actually reachable
    const cx = rect.left + rect.width / 2;
    const cy = rect.top + rect.height / 2;
    if (cx > 0 && cy > 0 && cx < innerWidth && cy < innerHeight) {
      const top = document.elementFromPoint(cx, cy);
      if (top && !el.contains(top) && !top.contains(el)) {
        // Covered by something else? Only filter if the 'top' element is significantly larger or a modal
        const topStyle = getComputedStyle(top);
        if (topStyle.position === 'fixed' || topStyle.position === 'absolute') return false;
      }
    }

    return rect.bottom >= 0 && rect.right >= 0 && rect.top <= innerHeight && rect.left <= innerWidth;
  };

  const cssPath = (el) => {
    if (!el || el.nodeType !== 1) return '';
    if (el.id) return '#' + CSS.escape(el.id);
    const parts = [];
    let node = el;
    while (node && node.nodeType === 1 && parts.length < 5) {
      let part = node.localName.toLowerCase();
      if (node.classList && node.classList.length) {
        part += '.' + Array.from(node.classList).slice(0, 2).map(CSS.escape).join('.');
      }
      let sibling = node;
      let index = 1;
      while ((sibling = sibling.previousElementSibling)) {
        if (sibling.localName === node.localName) index++;
      }
      part += `:nth-of-type(${index})`;
      parts.unshift(part);
      node = node.parentElement;
    }
    return parts.join(' > ');
  };

  const roleFor = (el) => {
    if (el.getAttribute('role')) return el.getAttribute('role');
    const tag = el.localName;
    if (tag === 'a') return 'link';
    if (tag === 'button') return 'button';
    if (tag === 'input' || tag === 'textarea' || tag === 'select') return 'input';
    if (/^h[1-6]$/.test(tag)) return 'heading';
    return tag;
  };

  const nameFor = (el) =>
    (el.getAttribute('aria-label') ||
     el.getAttribute('title') ||
     el.getAttribute('placeholder') ||
     el.innerText ||
     el.value ||
     '').trim().replace(/\s+/g, ' ').slice(0, 180);

  const candidates = document.querySelectorAll(
    'a,button,input,textarea,select,[role],[aria-label],h1,h2,h3,h4,[contenteditable="true"],dialog,[role="dialog"],[role="alert"]'
  );

  const isModal = (el) => {
    if (el.localName === 'dialog' || el.getAttribute('aria-modal') === 'true' || el.getAttribute('role') === 'dialog') return true;
    const style = getComputedStyle(el);
    if (style.position === 'fixed' || style.position === 'sticky') {
      const rect = el.getBoundingClientRect();
      if (rect.width > innerWidth * 0.3 && rect.height > innerHeight * 0.2) return true;
    }
    return false;
  };

  const nodes = [];
  let idCounter = 0;
  for (const el of candidates) {
    if (!isVisible(el)) continue;
    const rect = el.getBoundingClientRect();
    const name = nameFor(el);
    if (!name && !['input', 'textarea', 'select'].includes(el.localName)) continue;
    
    nodes.push({
      id: idCounter++,
      role: roleFor(el),
      name,
      text: (el.innerText || '').trim().replace(/\s+/g, ' ').slice(0, 240),
      selector: cssPath(el),
      modal: isModal(el),
      bounds: {
        x: Math.round(rect.x),
        y: Math.round(rect.y),
        w: Math.round(rect.width),
        h: Math.round(rect.height)
      }
    });
    if (nodes.length >= 250) break;
  }

  const bodyText = (document.body ? document.body.innerText : '')
    .trim()
    .replace(/\s+/g, ' ')
    .slice(0, 9000);

  const isLoading = !!document.querySelector('.spinner, .loading, .skeleton, [aria-busy="true"], .progress-bar, #loading-bar, .loading-spinner');

  return JSON.stringify({
    url: location.href,
    title: document.title,
    text: bodyText,
    loading: isLoading,
    nodes
  });
})();
)JS";
}

std::string unwrapWebViewJsonString(const std::string& scriptResult) {
    if (scriptResult.empty()) {
        return {};
    }

    try {
        auto parsed = nlohmann::json::parse(scriptResult);
        if (parsed.is_string()) {
            return parsed.get<std::string>();
        }
        return parsed.dump();
    } catch (...) {
        return scriptResult;
    }
}

} // namespace quantum

