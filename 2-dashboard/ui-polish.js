// Presentation adapter: retain the original select and its change handler.
(() => {
  const select = document.getElementById('mode-select');
  const bar = select.closest('.mode-bar');
  const group = document.createElement('div');
  group.className = 'mode-options';
  group.setAttribute('role', 'radiogroup');
  group.setAttribute('aria-label', 'เลือกโหมดมาตรฐานคุณภาพน้ำ');
  const icons = { conservation: '🌱', coral: '🪸', aquaculture: '🐟', recreation: '🏖️', industrial: '⚓', community: '🏘️' };
  let buttons = [];
  let signature = '';
  function sync() {
    const options = Array.from(select.options);
    if (!options.length) return; // Keep the original control if loading fails.
    const next = options.map(o => o.value + o.textContent).join('|');
    if (signature !== next) {
      signature = next;
      group.replaceChildren();
      buttons = options.map((option, index) => {
        const button = document.createElement('button');
        button.type = 'button';
        button.className = 'mode-option';
        button.setAttribute('role', 'radio');
        button.dataset.value = option.value;
        button.style.setProperty('--mode-delay', index * 45 + 'ms');
        const icon = document.createElement('span');
        icon.className = 'mode-option-icon';
        icon.setAttribute('aria-hidden', 'true');
        icon.textContent = icons[option.value] || '🌊';
        const name = document.createElement('span');
        name.className = 'mode-option-name';
        name.textContent = option.textContent.replace(/^\d+\.\s*/, '');
        const state = document.createElement('span');
        state.className = 'mode-option-state';
        state.setAttribute('aria-hidden', 'true');
        button.append(icon, name, state);
        button.addEventListener('click', () => {
          if (select.value === option.value) return;
          select.value = option.value;
          select.dispatchEvent(new Event('change', { bubbles: true }));
          sync();
        });
        group.append(button);
        return button;
      });
      select.hidden = true;
      bar.classList.add('mode-enhanced');
    }
    for (const button of buttons) {
      const active = button.dataset.value === select.value;
      button.setAttribute('aria-checked', String(active));
      button.tabIndex = active ? 0 : -1;
      button.querySelector('.mode-option-state').textContent = active ? '✓ กำลังใช้งาน' : 'เลือกโหมดนี้';
    }
  }
  group.addEventListener('keydown', event => {
    const index = buttons.indexOf(document.activeElement);
    if (index < 0) return;
    let next;
    if (event.key === 'ArrowRight' || event.key === 'ArrowDown') next = (index + 1) % buttons.length;
    if (event.key === 'ArrowLeft' || event.key === 'ArrowUp') next = (index + buttons.length - 1) % buttons.length;
    if (event.key === 'Home') next = 0;
    if (event.key === 'End') next = buttons.length - 1;
    if (next === undefined) return;
    event.preventDefault();
    buttons[next].focus();
    buttons[next].click();
  });
  bar.append(group);
  new MutationObserver(sync).observe(select, { childList: true, subtree: true });
  // applyMode updates this label for local choices and external mode sync.
  new MutationObserver(sync).observe(document.getElementById('mode-name'), { childList: true, subtree: true });
  select.addEventListener('change', sync);
  sync();
})();

// Reorder existing elements only; their IDs and data handlers stay intact.
(() => {
  const ai = document.querySelector('.ai-bar');
  const status = document.getElementById('swim-bar');
  ai.before(status);
  const reduceMotion = matchMedia('(prefers-reduced-motion: reduce)');
  document.querySelectorAll('#cards .value').forEach(value => {
    value.parentElement.classList.add('measurement');
    let previous = value.textContent;
    let animation;
    new MutationObserver(() => {
      const next = value.textContent;
      if (next === previous) return;
      const wasNumber = /\d/.test(previous);
      previous = next;
      if (!wasNumber || !/\d/.test(next) || reduceMotion.matches) return;
      animation?.cancel();
      animation = value.animate([
        { opacity: .65, transform: 'translateY(2px)' },
        { opacity: 1, transform: 'translateY(0)' }
      ], { duration: 350, easing: 'ease-out' });
    }).observe(value, { childList: true, characterData: true, subtree: true });
  });
})();
